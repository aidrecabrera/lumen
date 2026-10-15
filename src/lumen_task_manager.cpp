#include "lumen_task_manager.h"

#include <esp_log.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <optional>

#include "lumen_board_config.h"
#include "lumen_command_handler.h"
#include "lumen_energy_tracker.h"
#include "lumen_led_control.h"
#include "lumen_local_transport.h"
#include "lumen_mqtt_client.h"
#include "lumen_sensor_manager.h"
#include "lumen_system_utils.h"
#include "lumen_typed_queue.h"
#include "lumen_wifi_manager.h"

namespace {
static const char* TAG = "task_manager";

static TaskHandle_t sensors_task_handle = nullptr;
static TaskHandle_t led_task_handle = nullptr;
static TaskHandle_t network_task_handle = nullptr;

// a status after a command goes to the cloud at once; one after an auto-mode step waits for
// the next heartbeat, so auto mode cannot flood the cloud path.
struct StatusUpdate {
    StatusMessage status;
    bool from_command;
};

static StaticTypedQueue<SensorReading, QUEUE_SENSOR_TO_NETWORK_LENGTH> sensor_to_network_queue;
static StaticTypedQueue<SensorReading, QUEUE_SENSOR_TO_LED_LENGTH> sensor_to_led_queue;
static StaticTypedQueue<CommandEnvelope, QUEUE_COMMAND_LENGTH> command_dispatch_queue;
static StaticTypedQueue<StatusUpdate, QUEUE_STATUS_LENGTH> status_to_network_queue;
static StaticTypedQueue<EnergyMessage, QUEUE_ENERGY_LENGTH> energy_to_network_queue;
static StaticTypedQueue<AckMessage, QUEUE_ACK_LENGTH> ack_to_cloud_queue;
static StaticTypedQueue<AckMessage, QUEUE_ACK_LENGTH> ack_to_local_queue;

void registerCurrentTaskWithWatchdog(const char* task_name) {
    const esp_err_t result = esp_task_wdt_add(nullptr);
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "%s watchdog add failed", task_name);
    }
}

// Non-blocking send.
template <typename T, size_t Length>
bool sendToQueue(StaticTypedQueue<T, Length>& queue, const T& item, const char* queue_name) {
    if (queue.send(item, 0)) {
        return true;
    }

    ESP_LOGW(TAG, "queue full %s", queue_name);
    return false;
}

void drainLatestSensor(SensorReading& latest_reading, bool& has_reading) {
    SensorReading pending = {};

    while (sensor_to_led_queue.receive(pending, 0)) {
        latest_reading = pending;
        has_reading = true;
    }
}

void enqueueAck(const AckMessage& ack) {
    if (ack.source == CommandSource::LOCAL) {
        sendToQueue(ack_to_local_queue, ack, "ack_to_local");
    } else {
        sendToQueue(ack_to_cloud_queue, ack, "ack_to_cloud");
    }
}

void enqueueStatus(bool from_command) {
    const StatusUpdate update = {LedControl::buildStatusMessage(), from_command};
    sendToQueue(status_to_network_queue, update, "status_to_network");
}

void enqueueRejectedAck(const CommandId& command_id, CommandSource source, AckReason reason) {
    enqueueAck(LedControl::buildRejectedAck(command_id, source, reason));
}

void inboundCommandBridge(
    CommandSource source, CommandKind kind, const uint8_t* payload, uint16_t length
) {
    CommandEnvelope command = {};
    const CommandHandler::DecodeStatus status =
        CommandHandler::decode(kind, payload, length, command);
    command.source = source;

    if (status == CommandHandler::DecodeStatus::UNADDRESSABLE) {
        // decode() logged the reason. Without a command_id there is no ack topic.
        return;
    }

    if (status == CommandHandler::DecodeStatus::INVALID_FIELDS) {
        enqueueRejectedAck(command.command_id, source, AckReason::INVALID_PAYLOAD);
        return;
    }

    // dispatchCommand logs the reason when the queue is missing or full.
    if (!TaskManager::dispatchCommand(command)) {
        enqueueRejectedAck(command.command_id, source, AckReason::COMMAND_QUEUE_FULL);
    }
}

void rejectUnknownLocalKind(const CommandId& command_id) {
    enqueueRejectedAck(command_id, CommandSource::LOCAL, AckReason::INVALID_PAYLOAD);
}

// the network task owns wifi and the cloud link, so it fills in their state.
StatusMessage withConnectivity(StatusMessage status) {
    status.degraded_tier = WifiManager::getDegradedTier();
    status.cloud = MqttClient::state();
    return status;
}

// keeps the latest cloud-bound message of each kind until its rate allows; a failed publish
// waits for the next flush.
class CloudOutbox {
   public:
    void offerTelemetry(const SensorReading& reading) { telemetry_ = reading; }

    void offerStatus(const StatusMessage& status, bool urgent) {
        status_ = status;
        status_urgent_ = status_urgent_ || urgent;
    }

    void offerEnergy(const EnergyMessage& energy) { energy_ = energy; }

    void flush(uint64_t now_ms) {
        if (telemetry_ && isDue(last_telemetry_ms_, CLOUD_TELEMETRY_INTERVAL_MS, now_ms) &&
            MqttClient::publishTelemetry(*telemetry_)) {
            telemetry_.reset();
            last_telemetry_ms_ = now_ms;
        }

        const bool status_due =
            status_urgent_ || isDue(last_status_ms_, STATUS_HEARTBEAT_INTERVAL_MS, now_ms);
        if (status_ && status_due && MqttClient::publishStatus(withConnectivity(*status_))) {
            status_.reset();
            status_urgent_ = false;
            last_status_ms_ = now_ms;
        }

        if (energy_ && isDue(last_energy_ms_, CLOUD_ENERGY_INTERVAL_MS, now_ms) &&
            MqttClient::publishEnergy(*energy_)) {
            energy_.reset();
            last_energy_ms_ = now_ms;
        }
    }

   private:
    static bool isDue(std::optional<uint64_t> last_ms, uint32_t interval_ms, uint64_t now_ms) {
        return !last_ms || (now_ms - *last_ms >= interval_ms);
    }

    std::optional<SensorReading> telemetry_;
    std::optional<StatusMessage> status_;
    bool status_urgent_ = false;
    std::optional<EnergyMessage> energy_;
    std::optional<uint64_t> last_telemetry_ms_;
    std::optional<uint64_t> last_status_ms_;
    std::optional<uint64_t> last_energy_ms_;
};

void fanOutQueuedMessages(CloudOutbox& cloud) {
    SensorReading reading = {};
    while (sensor_to_network_queue.receive(reading, 0)) {
        LocalTransport::publishTelemetry(reading);
        cloud.offerTelemetry(reading);
    }

    StatusUpdate update = {};
    while (status_to_network_queue.receive(update, 0)) {
        LocalTransport::publishStatus(withConnectivity(update.status));
        cloud.offerStatus(update.status, update.from_command);
    }

    EnergyMessage energy = {};
    while (energy_to_network_queue.receive(energy, 0)) {
        LocalTransport::publishEnergy(energy);
        cloud.offerEnergy(energy);
    }

    AckMessage ack = {};
    while (ack_to_local_queue.receive(ack, 0)) {
        LocalTransport::publishAck(ack);
    }
}

// cloud acks keep their order: a failed publish stays at the head for the next loop.
void publishCloudAcks() {
    AckMessage ack = {};
    while (ack_to_cloud_queue.peek(ack, 0) && MqttClient::publishAck(ack)) {
        ack_to_cloud_queue.receive(ack, 0);
    }
}

void taskSensors(void* parameter) {
    (void)parameter;

    registerCurrentTaskWithWatchdog("TaskSensors");

    TickType_t last_wake_ticks = xTaskGetTickCount();

    while (true) {
        esp_task_wdt_reset();
        vTaskDelayUntil(&last_wake_ticks, pdMS_TO_TICKS(SENSOR_INTERVAL_MS));
        esp_task_wdt_reset();

        SensorReading reading = {};
        if (!Sensors::readCurrent(reading)) {
            continue;
        }

        // sendToQueue logs a full queue. The next reading replaces a dropped one.
        sendToQueue(sensor_to_network_queue, reading, "sensor_to_network");
        sendToQueue(sensor_to_led_queue, reading, "sensor_to_led");
    }
}

void taskLed(void* parameter) {
    (void)parameter;

    registerCurrentTaskWithWatchdog("TaskLED");

    SensorReading latest_reading = {};
    bool has_sensor_reading = false;
    uint64_t last_energy_publish_ms = getNowMs();
    uint64_t last_energy_persist_ms = getNowMs();

    while (true) {
        esp_task_wdt_reset();

        CommandEnvelope command = {};
        const bool has_command =
            command_dispatch_queue.receive(command, pdMS_TO_TICKS(LED_CONTROL_INTERVAL_MS));

        esp_task_wdt_reset();
        drainLatestSensor(latest_reading, has_sensor_reading);

        if (has_command) {
            enqueueAck(LedControl::applyCommand(command));
            enqueueStatus(true);
        }

        if (has_sensor_reading && LedControl::controlTick(latest_reading)) {
            enqueueStatus(false);
        }

        EnergyTracker::updateFromLedState(LedControl::getCurrentLedState());

        const uint64_t now_ms = getNowMs();
        if (now_ms - last_energy_publish_ms >= ENERGY_PUBLISH_INTERVAL_MS) {
            sendToQueue(energy_to_network_queue, EnergyTracker::getSnapshot(), "energy_to_network");
            last_energy_publish_ms = now_ms;
        }

        if (now_ms - last_energy_persist_ms >= ENERGY_PERSIST_INTERVAL_MS) {
            // ConfigManager logs a failed NVS write. The next interval retries.
            EnergyTracker::requestPersist();
            last_energy_persist_ms = now_ms;
        }
    }
}

void taskNetwork(void* parameter) {
    (void)parameter;

    registerCurrentTaskWithWatchdog("TaskNetwork");

    MqttClient::registerInboundCallback(inboundCommandBridge);
    CloudOutbox cloud;
    bool was_cloud_ready = false;
    uint64_t last_heartbeat_ms = getNowMs();

    while (true) {
        esp_task_wdt_reset();

        WifiManager::connectOrPoll();
        const bool cloud_ready = MqttClient::connectOrPoll();
        LocalTransport::poll(inboundCommandBridge, rejectUnknownLocalKind);

        const uint64_t now_ms = getNowMs();
        const bool heartbeat_due = now_ms - last_heartbeat_ms >= STATUS_HEARTBEAT_INTERVAL_MS;
        if (heartbeat_due || (cloud_ready && !was_cloud_ready)) {
            cloud.offerStatus(LedControl::buildStatusMessage(), true);
        }

        if (heartbeat_due || LocalTransport::takeNewClient()) {
            LocalTransport::publishStatus(withConnectivity(LedControl::buildStatusMessage()));
        }

        if (heartbeat_due) {
            last_heartbeat_ms = now_ms;
        }

        fanOutQueuedMessages(cloud);

        if (cloud_ready) {
            cloud.flush(now_ms);
            publishCloudAcks();
        }

        was_cloud_ready = cloud_ready;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
}  // namespace

namespace TaskManager {
// StaticTypedQueue::create() cannot fail (static storage), so this step cannot fail.
void createQueues() {
    sensor_to_network_queue.create();
    sensor_to_led_queue.create();
    command_dispatch_queue.create();
    status_to_network_queue.create();
    energy_to_network_queue.create();
    ack_to_cloud_queue.create();
    ack_to_local_queue.create();

    ESP_LOGI(TAG, "queues created");
}

bool dispatchCommand(const CommandEnvelope& command) {
    if (!command_dispatch_queue.isCreated()) {
        ESP_LOGW(TAG, "command queue missing");
        return false;
    }

    if (!command_dispatch_queue.send(command, 0)) {
        ESP_LOGW(TAG, "command queue full, dropped command_id=%s", command.command_id.c_str());
        return false;
    }

    return true;
}

bool createTasks() {
    sensors_task_handle = nullptr;
    led_task_handle = nullptr;
    network_task_handle = nullptr;

    const BaseType_t sensor_result = xTaskCreatePinnedToCore(
        taskSensors,
        "TaskSensors",
        TASK_SENSORS_STACK_BYTES,
        nullptr,
        TASK_SENSORS_PRIORITY,
        &sensors_task_handle,
        TASK_SENSORS_CORE
    );

    if (sensor_result != pdPASS) {
        ESP_LOGE(TAG, "TaskSensors create failed");
        return false;
    }

    const BaseType_t led_result = xTaskCreatePinnedToCore(
        taskLed,
        "TaskLED",
        TASK_LED_STACK_BYTES,
        nullptr,
        TASK_LED_PRIORITY,
        &led_task_handle,
        TASK_LED_CORE
    );

    if (led_result != pdPASS) {
        ESP_LOGE(TAG, "TaskLED create failed");
        vTaskDelete(sensors_task_handle);
        sensors_task_handle = nullptr;
        return false;
    }

    const BaseType_t network_result = xTaskCreatePinnedToCore(
        taskNetwork,
        "TaskNetwork",
        TASK_NETWORK_STACK_BYTES,
        nullptr,
        TASK_NETWORK_PRIORITY,
        &network_task_handle,
        TASK_NETWORK_CORE
    );

    if (network_result != pdPASS) {
        ESP_LOGE(TAG, "TaskNetwork create failed");
        vTaskDelete(led_task_handle);
        vTaskDelete(sensors_task_handle);
        led_task_handle = nullptr;
        sensors_task_handle = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "TaskSensors created");
    ESP_LOGI(TAG, "TaskLED created");
    ESP_LOGI(TAG, "TaskNetwork created");
    return true;
}
}  // namespace TaskManager
#include "lumen_task_manager.h"

#include <esp_log.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <atomic>
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

static StaticTypedQueue<SensorReading, QUEUE_SENSOR_TO_NETWORK_LENGTH> sensor_to_network_queue;
static StaticTypedQueue<SensorReading, QUEUE_SENSOR_TO_LED_LENGTH> sensor_to_led_queue;
static StaticTypedQueue<CommandEnvelope, QUEUE_COMMAND_LENGTH> command_dispatch_queue;
static StaticTypedQueue<StatusMessage, QUEUE_STATUS_LENGTH> status_to_network_queue;
static StaticTypedQueue<EnergyMessage, QUEUE_ENERGY_LENGTH> energy_to_network_queue;
static StaticTypedQueue<AckMessage, QUEUE_ACK_LENGTH> ack_to_cloud_queue;
static StaticTypedQueue<AckMessage, QUEUE_ACK_LENGTH> ack_to_local_queue;
// the cloud publishes only the latest of these, at its own rate.
static StaticTypedQueue<SensorReading, 1> telemetry_to_cloud_queue;
static StaticTypedQueue<EnergyMessage, 1> energy_to_cloud_queue;

// set after a cloud command so the cloud sees its result at once. other changes wait for the
// heartbeat, so auto mode and the local dashboard cannot flood the cloud path.
static std::atomic<bool> cloud_status_wanted{false};

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

void enqueueStatus() {
    sendToQueue(status_to_network_queue, LedControl::buildStatusMessage(), "status_to_network");
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

StatusMessage withConnectivity(StatusMessage status) {
    status.degraded_tier = WifiManager::getDegradedTier();
    status.cloud = MqttClient::state();
    return status;
}

bool isDue(std::optional<uint64_t> last_ms, uint32_t interval_ms, uint64_t now_ms) {
    return !last_ms || (now_ms - *last_ms >= interval_ms);
}

// publishes the latest message of each kind once its rate allows; a failed publish retries on
// the next loop. cloud acks keep their order: a failed one stays at the head.
class CloudPublisher {
   public:
    void reconnected() { last_status_ms_.reset(); }

    void publishDue(uint64_t now_ms) {
        AckMessage ack = {};
        while (ack_to_cloud_queue.peek(ack, 0) && MqttClient::publishAck(ack)) {
            ack_to_cloud_queue.receive(ack, 0);
        }

        // take before publishing: a peek then receive could remove a newer overwrite.
        // on failure the item goes back unless a newer one already took the slot.
        SensorReading reading = {};
        if (isDue(last_telemetry_ms_, CLOUD_TELEMETRY_INTERVAL_MS, now_ms) &&
            telemetry_to_cloud_queue.receive(reading, 0)) {
            if (MqttClient::publishTelemetry(reading)) {
                last_telemetry_ms_ = now_ms;
            } else {
                telemetry_to_cloud_queue.send(reading, 0);
            }
        }

        const bool status_wanted = cloud_status_wanted.exchange(false);
        if (status_wanted || isDue(last_status_ms_, STATUS_HEARTBEAT_INTERVAL_MS, now_ms)) {
            if (MqttClient::publishStatus(withConnectivity(LedControl::buildStatusMessage()))) {
                last_status_ms_ = now_ms;
            } else if (status_wanted) {
                cloud_status_wanted.store(true);
            }
        }

        EnergyMessage energy = {};
        if (isDue(last_energy_ms_, CLOUD_ENERGY_INTERVAL_MS, now_ms) &&
            energy_to_cloud_queue.receive(energy, 0)) {
            if (MqttClient::publishEnergy(energy)) {
                last_energy_ms_ = now_ms;
            } else {
                energy_to_cloud_queue.send(energy, 0);
            }
        }
    }

   private:
    std::optional<uint64_t> last_telemetry_ms_;
    std::optional<uint64_t> last_status_ms_;
    std::optional<uint64_t> last_energy_ms_;
};

void fanOutQueuedMessages() {
    SensorReading reading = {};
    while (sensor_to_network_queue.receive(reading, 0)) {
        LocalTransport::publishTelemetry(reading);
        telemetry_to_cloud_queue.overwrite(reading);
    }

    StatusMessage status = {};
    while (status_to_network_queue.receive(status, 0)) {
        LocalTransport::publishStatus(withConnectivity(status));
    }

    EnergyMessage energy = {};
    while (energy_to_network_queue.receive(energy, 0)) {
        LocalTransport::publishEnergy(energy);
        energy_to_cloud_queue.overwrite(energy);
    }

    AckMessage ack = {};
    while (ack_to_local_queue.receive(ack, 0)) {
        LocalTransport::publishAck(ack);
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
            enqueueStatus();
            if (command.source == CommandSource::CLOUD) {
                cloud_status_wanted.store(true);
            }
        }

        if (has_sensor_reading && LedControl::controlTick(latest_reading)) {
            enqueueStatus();
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

    uint64_t last_heartbeat_ms = getNowMs();

    while (true) {
        esp_task_wdt_reset();

        WifiManager::connectOrPoll();
        LocalTransport::poll(inboundCommandBridge, rejectUnknownLocalKind);

        const uint64_t now_ms = getNowMs();
        const bool heartbeat_due = now_ms - last_heartbeat_ms >= STATUS_HEARTBEAT_INTERVAL_MS;
        if (heartbeat_due || LocalTransport::takeNewClient()) {
            LocalTransport::publishStatus(withConnectivity(LedControl::buildStatusMessage()));
        }

        if (heartbeat_due) {
            last_heartbeat_ms = now_ms;
        }

        fanOutQueuedMessages();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// not on the watchdog: dns, tcp, tls and the mqtt connack can each block a connect for seconds.
// it runs apart from the network task so the local path never waits on the cloud.
void taskCloud(void* parameter) {
    (void)parameter;

    MqttClient::registerInboundCallback(inboundCommandBridge);
    CloudPublisher publisher;
    bool was_ready = false;

    while (true) {
        const bool ready = MqttClient::connectOrPoll();
        if (ready && !was_ready) {
            publisher.reconnected();
        }

        if (ready) {
            publisher.publishDue(getNowMs());
        }

        was_ready = ready;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

bool createTask(
    TaskFunction_t task, const char* name, uint32_t stack_bytes, UBaseType_t priority, BaseType_t core
) {
    if (xTaskCreatePinnedToCore(task, name, stack_bytes, nullptr, priority, nullptr, core) != pdPASS) {
        ESP_LOGE(TAG, "%s create failed", name);
        return false;
    }

    ESP_LOGI(TAG, "%s created", name);
    return true;
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
    telemetry_to_cloud_queue.create();
    energy_to_cloud_queue.create();

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

bool createControlTasks() {
    return createTask(
               taskSensors, "TaskSensors", TASK_SENSORS_STACK_BYTES, TASK_SENSORS_PRIORITY, TASK_SENSORS_CORE
           ) &&
           createTask(taskLed, "TaskLED", TASK_LED_STACK_BYTES, TASK_LED_PRIORITY, TASK_LED_CORE);
}

bool createNetworkTasks() {
    return createTask(
               taskNetwork, "TaskNetwork", TASK_NETWORK_STACK_BYTES, TASK_NETWORK_PRIORITY, TASK_NETWORK_CORE
           ) &&
           createTask(taskCloud, "TaskCloud", TASK_CLOUD_STACK_BYTES, TASK_CLOUD_PRIORITY, TASK_CLOUD_CORE);
}
}  // namespace TaskManager

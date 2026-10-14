#include "lumen_task_manager.h"

#include <esp_log.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "lumen_board_config.h"
#include "lumen_command_handler.h"
#include "lumen_energy_tracker.h"
#include "lumen_led_control.h"
#include "lumen_mqtt_client.h"
#include "lumen_sensor_manager.h"
#include "lumen_system_utils.h"
#include "lumen_typed_queue.h"
#include "lumen_wifi_manager.h"

namespace {
static const char* TAG = "task_manager";

static TaskHandle_t sensors_task_handle = nullptr;
static TaskHandle_t led_task_handle = nullptr;
static TaskHandle_t mqtt_task_handle = nullptr;

static StaticTypedQueue<SensorReading, QUEUE_SENSOR_TO_MQTT_LENGTH> sensor_to_mqtt_queue;
static StaticTypedQueue<SensorReading, QUEUE_SENSOR_TO_LED_LENGTH> sensor_to_led_queue;
static StaticTypedQueue<CommandEnvelope, QUEUE_COMMAND_LENGTH> command_dispatch_queue;
static StaticTypedQueue<StatusMessage, QUEUE_STATUS_LENGTH> status_to_mqtt_queue;
static StaticTypedQueue<EnergyMessage, QUEUE_ENERGY_LENGTH> energy_to_mqtt_queue;
static StaticTypedQueue<AckMessage, QUEUE_ACK_LENGTH> ack_to_mqtt_queue;

void registerCurrentTaskWithWatchdog(const char* task_name) {
    const esp_err_t result = esp_task_wdt_add(nullptr);
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "%s watchdog add failed", task_name);
    }
}

bool areLedStatesEqual(const LedState& left, const LedState& right) {
    return (left.power == right.power) && (left.brightness_pct == right.brightness_pct) &&
           (left.red_enabled == right.red_enabled) && (left.blue_enabled == right.blue_enabled) &&
           (left.far_red_enabled == right.far_red_enabled) &&
           (left.red_dist_pct == right.red_dist_pct) &&
           (left.blue_dist_pct == right.blue_dist_pct) &&
           (left.far_red_dist_pct == right.far_red_dist_pct);
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

// Publishes queued items in order. A failed publish keeps the item at the head for retry.
template <typename T, size_t Length>
void drainPublishQueue(
    StaticTypedQueue<T, Length>& queue, bool (*publish)(const T&), const char* label
) {
    T item = {};

    while (queue.peek(item, 0)) {
        esp_task_wdt_reset();

        if (!publish(item)) {
            ESP_LOGW(TAG, "%s publish failed, retained for retry", label);
            break;
        }

        queue.receive(item, 0);
    }
}

void drainLatestSensor(SensorReading& latest_reading, bool& has_reading) {
    SensorReading pending = {};

    while (sensor_to_led_queue.receive(pending, 0)) {
        latest_reading = pending;
        has_reading = true;
    }
}

// Uses the existing ack wire format: AckResult::REJECTED with a reason string.
void enqueueRejectedAck(const CommandId& command_id, AckReason reason) {
    const AckMessage ack = LedControl::buildRejectedAck(command_id, reason);
    if (!sendToQueue(ack_to_mqtt_queue, ack, "ack_to_mqtt")) {
        ESP_LOGW(TAG, "rejected ack enqueue failed command_id=%s", command_id.c_str());
    }
}

void inboundCommandBridge(CommandKind kind, const uint8_t* payload, uint16_t length) {
    CommandEnvelope command = {};
    const CommandHandler::DecodeStatus status =
        CommandHandler::decode(kind, payload, length, command);

    if (status == CommandHandler::DecodeStatus::UNADDRESSABLE) {
        // decode() logged the reason. Without a command_id there is no ack topic.
        return;
    }

    if (status == CommandHandler::DecodeStatus::INVALID_FIELDS) {
        enqueueRejectedAck(command.command_id, AckReason::INVALID_PAYLOAD);
        return;
    }

    // dispatchCommand logs the reason when the queue is missing or full.
    if (!TaskManager::dispatchCommand(command)) {
        enqueueRejectedAck(command.command_id, AckReason::COMMAND_QUEUE_FULL);
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
        sendToQueue(sensor_to_mqtt_queue, reading, "sensor_to_mqtt");
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

        // sendToQueue logs a full queue.
        if (has_command) {
            const AckMessage ack = LedControl::applyCommand(command);
            sendToQueue(ack_to_mqtt_queue, ack, "ack_to_mqtt");

            const StatusMessage status =
                LedControl::buildStatusMessage(WifiManager::getDegradedTier());
            sendToQueue(status_to_mqtt_queue, status, "status_to_mqtt");
        }

        if (has_sensor_reading) {
            const LedState previous_led = LedControl::getCurrentLedState();
            LedControl::controlTick(latest_reading);

            const LedState current_led = LedControl::getCurrentLedState();
            if (!areLedStatesEqual(previous_led, current_led)) {
                const StatusMessage status =
                    LedControl::buildStatusMessage(WifiManager::getDegradedTier());
                sendToQueue(status_to_mqtt_queue, status, "status_to_mqtt");
            }
        }

        const LedState applied_led = LedControl::getCurrentLedState();
        EnergyTracker::updateFromLedState(applied_led);

        const uint64_t now_ms = getNowMs();
        if (now_ms - last_energy_publish_ms >= ENERGY_PUBLISH_INTERVAL_MS) {
            const EnergyMessage energy = EnergyTracker::getSnapshot();
            sendToQueue(energy_to_mqtt_queue, energy, "energy_to_mqtt");

            last_energy_publish_ms = now_ms;
        }

        if (now_ms - last_energy_persist_ms >= ENERGY_PERSIST_INTERVAL_MS) {
            // ConfigManager logs a failed NVS write. The next interval retries.
            EnergyTracker::requestPersist();

            last_energy_persist_ms = now_ms;
        }
    }
}

void taskMqtt(void* parameter) {
    (void)parameter;

    registerCurrentTaskWithWatchdog("TaskMQTT");

    MqttClient::registerInboundCallback(inboundCommandBridge);
    uint64_t last_heartbeat_ms = getNowMs();

    while (true) {
        esp_task_wdt_reset();

        const bool wifi_ready = WifiManager::connectOrPoll();
        const bool mqtt_ready = MqttClient::connectOrPoll();

        if (wifi_ready && mqtt_ready) {
            SensorReading reading = {};
            if (sensor_to_mqtt_queue.peek(reading, 0)) {
                if (MqttClient::publishTelemetry(reading)) {
                    sensor_to_mqtt_queue.receive(reading, 0);
                } else {
                    ESP_LOGW(TAG, "telemetry publish failed, retained for retry");
                }
            }

            drainPublishQueue(status_to_mqtt_queue, MqttClient::publishStatus, "status");
            drainPublishQueue(energy_to_mqtt_queue, MqttClient::publishEnergy, "energy");
            drainPublishQueue(ack_to_mqtt_queue, MqttClient::publishAck, "ack");

            const uint64_t now_ms = getNowMs();
            if (now_ms - last_heartbeat_ms >= MQTT_HEARTBEAT_INTERVAL_MS) {
                const StatusMessage heartbeat =
                    LedControl::buildStatusMessage(WifiManager::getDegradedTier());

                if (MqttClient::publishStatus(heartbeat)) {
                    last_heartbeat_ms = now_ms;
                } else {
                    ESP_LOGW(TAG, "heartbeat publish failed");
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
}  // namespace

namespace TaskManager {
// StaticTypedQueue::create() cannot fail (static storage), so this step cannot fail.
void createQueues() {
    sensor_to_mqtt_queue.create();
    sensor_to_led_queue.create();
    command_dispatch_queue.create();
    status_to_mqtt_queue.create();
    energy_to_mqtt_queue.create();
    ack_to_mqtt_queue.create();

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
    mqtt_task_handle = nullptr;

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

    const BaseType_t mqtt_result = xTaskCreatePinnedToCore(
        taskMqtt,
        "TaskMQTT",
        TASK_MQTT_STACK_BYTES,
        nullptr,
        TASK_MQTT_PRIORITY,
        &mqtt_task_handle,
        TASK_MQTT_CORE
    );

    if (mqtt_result != pdPASS) {
        ESP_LOGE(TAG, "TaskMQTT create failed");
        vTaskDelete(led_task_handle);
        vTaskDelete(sensors_task_handle);
        led_task_handle = nullptr;
        sensors_task_handle = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "TaskSensors created");
    ESP_LOGI(TAG, "TaskLED created");
    ESP_LOGI(TAG, "TaskMQTT created");
    return true;
}
}  // namespace TaskManager
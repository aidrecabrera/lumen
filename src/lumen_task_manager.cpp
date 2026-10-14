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
#include "lumen_wifi_manager.h"

namespace {
static const char* TAG = "task_manager";

static TaskHandle_t sensors_task_handle = nullptr;
static TaskHandle_t led_task_handle = nullptr;
static TaskHandle_t mqtt_task_handle = nullptr;

static QueueHandle_t sensor_to_mqtt_queue = nullptr;
static QueueHandle_t sensor_to_led_queue = nullptr;
static QueueHandle_t command_dispatch_queue = nullptr;
static QueueHandle_t status_to_mqtt_queue = nullptr;
static QueueHandle_t energy_to_mqtt_queue = nullptr;
static QueueHandle_t ack_to_mqtt_queue = nullptr;

static StaticQueue_t sensor_to_mqtt_queue_storage;
static uint8_t sensor_to_mqtt_queue_buffer[QUEUE_SENSOR_TO_MQTT_LENGTH * sizeof(SensorReading)];

static StaticQueue_t sensor_to_led_queue_storage;
static uint8_t sensor_to_led_queue_buffer[QUEUE_SENSOR_TO_LED_LENGTH * sizeof(SensorReading)];

static StaticQueue_t command_dispatch_queue_storage;
static uint8_t command_dispatch_queue_buffer[QUEUE_COMMAND_LENGTH * sizeof(CommandEnvelope)];

static StaticQueue_t status_to_mqtt_queue_storage;
static uint8_t status_to_mqtt_queue_buffer[QUEUE_STATUS_LENGTH * sizeof(StatusMessage)];

static StaticQueue_t energy_to_mqtt_queue_storage;
static uint8_t energy_to_mqtt_queue_buffer[QUEUE_ENERGY_LENGTH * sizeof(EnergyMessage)];

static StaticQueue_t ack_to_mqtt_queue_storage;
static uint8_t ack_to_mqtt_queue_buffer[QUEUE_ACK_LENGTH * sizeof(AckMessage)];

void registerCurrentTaskWithWatchdog(const char* task_name) {
    const esp_err_t result = esp_task_wdt_add(nullptr);
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "%s watchdog add failed", task_name);
    }
}

void deleteQueue(QueueHandle_t& queue_handle) {
    if (queue_handle != nullptr) {
        vQueueDelete(queue_handle);
        queue_handle = nullptr;
    }
}

void clearQueues() {
    deleteQueue(sensor_to_mqtt_queue);
    deleteQueue(sensor_to_led_queue);
    deleteQueue(command_dispatch_queue);
    deleteQueue(status_to_mqtt_queue);
    deleteQueue(energy_to_mqtt_queue);
    deleteQueue(ack_to_mqtt_queue);
}

bool areLedStatesEqual(const LedState& left, const LedState& right) {
    return (left.power == right.power) && (left.brightness_pct == right.brightness_pct) &&
           (left.red_enabled == right.red_enabled) && (left.blue_enabled == right.blue_enabled) &&
           (left.far_red_enabled == right.far_red_enabled) &&
           (left.red_dist_pct == right.red_dist_pct) &&
           (left.blue_dist_pct == right.blue_dist_pct) &&
           (left.far_red_dist_pct == right.far_red_dist_pct);
}

// Non-blocking send. The item type must match the queue item size.
template <typename T>
bool sendToQueue(QueueHandle_t queue_handle, const T& item, const char* queue_name) {
    const BaseType_t send_result = xQueueSend(queue_handle, &item, 0);
    if (send_result == pdTRUE) {
        return true;
    }

    ESP_LOGW(TAG, "queue full %s", queue_name);
    return false;
}

// Publishes queued items in order. A failed publish keeps the item at the head for retry.
template <typename T>
void drainPublishQueue(QueueHandle_t queue_handle, bool (*publish)(const T&), const char* label) {
    T item = {};

    while (xQueuePeek(queue_handle, &item, 0) == pdTRUE) {
        esp_task_wdt_reset();

        if (!publish(item)) {
            ESP_LOGW(TAG, "%s publish failed, retained for retry", label);
            break;
        }

        xQueueReceive(queue_handle, &item, 0);
    }
}

void drainLatestSensor(SensorReading& latest_reading, bool& has_reading) {
    SensorReading pending = {};

    while (xQueueReceive(sensor_to_led_queue, &pending, 0) == pdTRUE) {
        latest_reading = pending;
        has_reading = true;
    }
}

void inboundCommandBridge(const char* topic, const uint8_t* payload, uint16_t length) {
    CommandEnvelope command = {};
    const bool was_handled = CommandHandler::decode(topic, payload, length, command) &&
                             TaskManager::dispatchCommand(command);
    if (!was_handled) {
        ESP_LOGW(TAG, "inbound command not handled");
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
        const BaseType_t receive_result =
            xQueueReceive(command_dispatch_queue, &command, pdMS_TO_TICKS(LED_CONTROL_INTERVAL_MS));

        esp_task_wdt_reset();
        drainLatestSensor(latest_reading, has_sensor_reading);

        if (receive_result == pdTRUE) {
            const AckMessage ack = LedControl::applyCommand(command);
            if (!sendToQueue(ack_to_mqtt_queue, ack, "ack_to_mqtt")) {
                ESP_LOGW(TAG, "ack enqueue failed");
            }

            const StatusMessage status =
                LedControl::buildStatusMessage(WifiManager::getDegradedTier());
            if (!sendToQueue(status_to_mqtt_queue, status, "status_to_mqtt")) {
                ESP_LOGW(TAG, "status enqueue failed");
            }
        }

        if (has_sensor_reading) {
            const LedState previous_led = LedControl::getCurrentLedState();
            LedControl::controlTick(latest_reading);

            const LedState current_led = LedControl::getCurrentLedState();
            if (!areLedStatesEqual(previous_led, current_led)) {
                const StatusMessage status =
                    LedControl::buildStatusMessage(WifiManager::getDegradedTier());
                if (!sendToQueue(status_to_mqtt_queue, status, "status_to_mqtt")) {
                    ESP_LOGW(TAG, "status enqueue failed");
                }
            }
        }

        const LedState applied_led = LedControl::getCurrentLedState();
        EnergyTracker::updateFromLedState(applied_led);

        const uint64_t now_ms = getNowMs();
        if (now_ms - last_energy_publish_ms >= ENERGY_PUBLISH_INTERVAL_MS) {
            const EnergyMessage energy = EnergyTracker::getSnapshot();
            if (!sendToQueue(energy_to_mqtt_queue, energy, "energy_to_mqtt")) {
                ESP_LOGW(TAG, "energy enqueue failed");
            }

            last_energy_publish_ms = now_ms;
        }

        if (now_ms - last_energy_persist_ms >= ENERGY_PERSIST_INTERVAL_MS) {
            if (!EnergyTracker::requestPersist()) {
                ESP_LOGW(TAG, "energy persist failed");
            }

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
            if (xQueuePeek(sensor_to_mqtt_queue, &reading, 0) == pdTRUE) {
                if (MqttClient::publishTelemetry(reading)) {
                    xQueueReceive(sensor_to_mqtt_queue, &reading, 0);
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
// xQueueCreateStatic with static storage cannot return NULL (IDF 4.4 freertos/queue.h:202:
// NULL only when the buffer is NULL), so this step cannot fail.
void createQueues() {
    clearQueues();

    sensor_to_mqtt_queue = xQueueCreateStatic(
        QUEUE_SENSOR_TO_MQTT_LENGTH,
        sizeof(SensorReading),
        sensor_to_mqtt_queue_buffer,
        &sensor_to_mqtt_queue_storage
    );

    sensor_to_led_queue = xQueueCreateStatic(
        QUEUE_SENSOR_TO_LED_LENGTH,
        sizeof(SensorReading),
        sensor_to_led_queue_buffer,
        &sensor_to_led_queue_storage
    );

    command_dispatch_queue = xQueueCreateStatic(
        QUEUE_COMMAND_LENGTH,
        sizeof(CommandEnvelope),
        command_dispatch_queue_buffer,
        &command_dispatch_queue_storage
    );

    status_to_mqtt_queue = xQueueCreateStatic(
        QUEUE_STATUS_LENGTH,
        sizeof(StatusMessage),
        status_to_mqtt_queue_buffer,
        &status_to_mqtt_queue_storage
    );

    energy_to_mqtt_queue = xQueueCreateStatic(
        QUEUE_ENERGY_LENGTH,
        sizeof(EnergyMessage),
        energy_to_mqtt_queue_buffer,
        &energy_to_mqtt_queue_storage
    );

    ack_to_mqtt_queue = xQueueCreateStatic(
        QUEUE_ACK_LENGTH, sizeof(AckMessage), ack_to_mqtt_queue_buffer, &ack_to_mqtt_queue_storage
    );

    ESP_LOGI(TAG, "queues created");
}

bool dispatchCommand(const CommandEnvelope& command) {
    if (command_dispatch_queue == nullptr) {
        ESP_LOGW(TAG, "command queue missing");
        return false;
    }

    const BaseType_t send_result = xQueueSend(command_dispatch_queue, &command, 0);
    if (send_result != pdTRUE) {
        ESP_LOGW(TAG, "command queue full, dropped command_id=%s", command.command_id);
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
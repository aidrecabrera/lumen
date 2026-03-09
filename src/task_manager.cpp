#include "task_manager.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

#include "command_handler.h"
#include "config.h"
#include "energy_tracker.h"
#include "led_control.h"
#include "mqtt_client.h"
#include "sensors.h"
#include "wifi_manager.h"

QueueHandle_t sensor_to_mqtt_queue = nullptr;
QueueHandle_t sensor_to_led_queue = nullptr;
QueueHandle_t command_dispatch_queue = nullptr;
QueueHandle_t status_to_mqtt_queue = nullptr;
QueueHandle_t energy_to_mqtt_queue = nullptr;
QueueHandle_t ack_to_mqtt_queue = nullptr;

namespace {
static const char* TAG = "task_manager";

static TaskHandle_t sensors_task_handle = nullptr;
static TaskHandle_t led_task_handle = nullptr;
static TaskHandle_t mqtt_task_handle = nullptr;

uint64_t getNowMs() { return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL); }

void clearQueues() {
    if (sensor_to_mqtt_queue != nullptr) {
        vQueueDelete(sensor_to_mqtt_queue);
        sensor_to_mqtt_queue = nullptr;
    }

    if (sensor_to_led_queue != nullptr) {
        vQueueDelete(sensor_to_led_queue);
        sensor_to_led_queue = nullptr;
    }

    if (command_dispatch_queue != nullptr) {
        vQueueDelete(command_dispatch_queue);
        command_dispatch_queue = nullptr;
    }

    if (status_to_mqtt_queue != nullptr) {
        vQueueDelete(status_to_mqtt_queue);
        status_to_mqtt_queue = nullptr;
    }

    if (energy_to_mqtt_queue != nullptr) {
        vQueueDelete(energy_to_mqtt_queue);
        energy_to_mqtt_queue = nullptr;
    }

    if (ack_to_mqtt_queue != nullptr) {
        vQueueDelete(ack_to_mqtt_queue);
        ack_to_mqtt_queue = nullptr;
    }
}

bool sendSensorReading(
    QueueHandle_t queue_handle, const SensorReading& reading, const char* queue_name
) {
    BaseType_t send_result = xQueueSend(queue_handle, &reading, 0);
    if (send_result == pdTRUE) {
        return true;
    }

    ESP_LOGW(TAG, "queue full %s", queue_name);
    return false;
}

bool sendStatusMessage(const StatusMessage& status) {
    BaseType_t send_result = xQueueSend(status_to_mqtt_queue, &status, 0);
    if (send_result == pdTRUE) {
        return true;
    }

    ESP_LOGW(TAG, "queue full status_to_mqtt");
    return false;
}

bool sendAckMessage(const AckMessage& ack) {
    BaseType_t send_result = xQueueSend(ack_to_mqtt_queue, &ack, 0);
    if (send_result == pdTRUE) {
        return true;
    }

    ESP_LOGW(TAG, "queue full ack_to_mqtt");
    return false;
}

bool sendEnergyMessage(const EnergyMessage& energy) {
    BaseType_t send_result = xQueueSend(energy_to_mqtt_queue, &energy, 0);
    if (send_result == pdTRUE) {
        return true;
    }

    ESP_LOGW(TAG, "queue full energy_to_mqtt");
    return false;
}

bool areLedStatesEqual(const LedState& left, const LedState& right) {
    return memcmp(&left, &right, sizeof(LedState)) == 0;
}

void drainLatestSensor(SensorReading& latest_reading, bool& has_reading) {
    SensorReading pending = {};

    while (xQueueReceive(sensor_to_led_queue, &pending, 0) == pdTRUE) {
        latest_reading = pending;
        has_reading = true;
    }
}

void drainStatusQueue() {
    StatusMessage status = {};

    while (xQueueReceive(status_to_mqtt_queue, &status, 0) == pdTRUE) {
        bool was_published = MqttClient::publishStatus(status);
        if (!was_published) {
            ESP_LOGW(TAG, "status publish failed");
            return;
        }
    }
}

void drainEnergyQueue() {
    EnergyMessage energy = {};

    while (xQueueReceive(energy_to_mqtt_queue, &energy, 0) == pdTRUE) {
        bool was_published = MqttClient::publishEnergy(energy);
        if (!was_published) {
            ESP_LOGW(TAG, "energy publish failed");
            return;
        }
    }
}

void drainAckQueue() {
    AckMessage ack = {};

    while (xQueueReceive(ack_to_mqtt_queue, &ack, 0) == pdTRUE) {
        bool was_published = MqttClient::publishAck(ack);
        if (!was_published) {
            ESP_LOGW(TAG, "ack publish failed");
            return;
        }
    }
}

// Intentional deviation: wraps bool-returning handleInbound
// to match the void callback signature expected by PubSubClient.
void inboundCommandBridge(const char* topic, const uint8_t* payload, uint16_t length) {
    bool was_handled = CommandHandler::handleInbound(topic, payload, length);
    if (!was_handled) {
        ESP_LOGW(TAG, "inbound command not handled");
    }
}

void taskSensors(void* parameter) {
    (void)parameter;

    TickType_t last_wake_ticks = xTaskGetTickCount();

    while (true) {
        vTaskDelayUntil(&last_wake_ticks, pdMS_TO_TICKS(SENSOR_INTERVAL_MS));

        SensorReading reading = {};
        bool was_read = Sensors::readCurrent(reading);
        if (!was_read) {
            continue;
        }

        bool sent_mqtt = sendSensorReading(sensor_to_mqtt_queue, reading, "sensor_to_mqtt");
        bool sent_led = sendSensorReading(sensor_to_led_queue, reading, "sensor_to_led");

        if (!sent_mqtt || !sent_led) {
            continue;
        }
    }
}

void taskLed(void* parameter) {
    (void)parameter;

    SensorReading latest_reading = {};
    bool has_sensor_reading = false;
    uint64_t last_energy_publish_ms = getNowMs();
    uint64_t last_energy_persist_ms = getNowMs();

    while (true) {
        CommandEnvelope command = {};
        BaseType_t receive_result =
            xQueueReceive(command_dispatch_queue, &command, pdMS_TO_TICKS(LED_CONTROL_INTERVAL_MS));

        drainLatestSensor(latest_reading, has_sensor_reading);

        if (receive_result == pdTRUE) {
            AckMessage ack = LedControl::applyCommand(command);
            bool sent_ack = sendAckMessage(ack);
            if (!sent_ack) {
                ESP_LOGW(TAG, "ack enqueue failed");
            }

            StatusMessage status = LedControl::buildStatusMessage(WifiManager::getDegradedTier());
            bool sent_status = sendStatusMessage(status);
            if (!sent_status) {
                ESP_LOGW(TAG, "status enqueue failed");
            }
        }

        if (has_sensor_reading) {
            LedState previous_led = LedControl::getCurrentLedState();
            LedControl::controlTick(latest_reading);

            LedState current_led = LedControl::getCurrentLedState();
            if (!areLedStatesEqual(previous_led, current_led)) {
                StatusMessage status =
                    LedControl::buildStatusMessage(WifiManager::getDegradedTier());
                bool sent_status = sendStatusMessage(status);
                if (!sent_status) {
                    ESP_LOGW(TAG, "status enqueue failed");
                }
            }
        }

        LedState applied_led = LedControl::getCurrentLedState();
        EnergyTracker::updateFromLedState(applied_led);

        uint64_t now_ms = getNowMs();
        if (now_ms - last_energy_publish_ms >= ENERGY_PUBLISH_INTERVAL_MS) {
            EnergyMessage energy = EnergyTracker::getSnapshot();
            bool sent_energy = sendEnergyMessage(energy);
            if (!sent_energy) {
                ESP_LOGW(TAG, "energy enqueue failed");
            }

            last_energy_publish_ms = now_ms;
        }

        if (now_ms - last_energy_persist_ms >= ENERGY_PERSIST_INTERVAL_MS) {
            bool was_persisted = EnergyTracker::requestPersist();
            if (!was_persisted) {
                ESP_LOGW(TAG, "energy persist failed");
            }

            last_energy_persist_ms = now_ms;
        }
    }
}

void taskMqtt(void* parameter) {
    (void)parameter;

    MqttClient::registerInboundCallback(inboundCommandBridge);
    uint64_t last_heartbeat_ms = getNowMs();

    while (true) {
        SensorReading reading = {};
        BaseType_t receive_result =
            xQueueReceive(sensor_to_mqtt_queue, &reading, pdMS_TO_TICKS(100));

        bool wifi_ready = WifiManager::connectOrPoll();
        bool mqtt_ready = MqttClient::connectOrPoll();

        if (receive_result == pdTRUE && wifi_ready && mqtt_ready) {
            bool was_published = MqttClient::publishTelemetry(reading);
            if (!was_published) {
                ESP_LOGW(TAG, "telemetry publish failed");
            }
        }

        if (!mqtt_ready) {
            continue;
        }

        drainStatusQueue();
        drainEnergyQueue();
        drainAckQueue();

        uint64_t now_ms = getNowMs();
        if (now_ms - last_heartbeat_ms < MQTT_HEARTBEAT_INTERVAL_MS) {
            continue;
        }

        StatusMessage heartbeat = LedControl::buildStatusMessage(WifiManager::getDegradedTier());
        bool was_published = MqttClient::publishStatus(heartbeat);
        if (!was_published) {
            ESP_LOGW(TAG, "heartbeat publish failed");
        }

        last_heartbeat_ms = now_ms;
    }
}
}  // namespace

namespace TaskManager {
bool createQueues() {
    clearQueues();

    sensor_to_mqtt_queue = xQueueCreate(QUEUE_SENSOR_TO_MQTT_LENGTH, sizeof(SensorReading));
    sensor_to_led_queue = xQueueCreate(QUEUE_SENSOR_TO_LED_LENGTH, sizeof(SensorReading));
    command_dispatch_queue = xQueueCreate(QUEUE_COMMAND_LENGTH, sizeof(CommandEnvelope));
    status_to_mqtt_queue = xQueueCreate(QUEUE_STATUS_LENGTH, sizeof(StatusMessage));
    energy_to_mqtt_queue = xQueueCreate(QUEUE_ENERGY_LENGTH, sizeof(EnergyMessage));
    ack_to_mqtt_queue = xQueueCreate(QUEUE_ACK_LENGTH, sizeof(AckMessage));

    if (sensor_to_mqtt_queue == nullptr || sensor_to_led_queue == nullptr ||
        command_dispatch_queue == nullptr || status_to_mqtt_queue == nullptr ||
        energy_to_mqtt_queue == nullptr || ack_to_mqtt_queue == nullptr) {
        ESP_LOGE(TAG, "queue create failed");
        clearQueues();
        return false;
    }

    ESP_LOGI(TAG, "queues created");
    return true;
}

bool createTasks() {
    sensors_task_handle = nullptr;
    led_task_handle = nullptr;
    mqtt_task_handle = nullptr;

    BaseType_t sensor_result = xTaskCreatePinnedToCore(
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
        sensors_task_handle = nullptr;
        return false;
    }

    BaseType_t led_result = xTaskCreatePinnedToCore(
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
        return false;
    }

    BaseType_t mqtt_result = xTaskCreatePinnedToCore(
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
        return false;
    }

    ESP_LOGI(TAG, "TaskSensors created");
    ESP_LOGI(TAG, "TaskLED created");
    ESP_LOGI(TAG, "TaskMQTT created");
    return true;
}
}  // namespace TaskManager
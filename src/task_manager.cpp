#include "task_manager.h"

#include <esp_log.h>

#include "config.h"
#include "sensors.h"

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


void clearQueues()
{
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
    QueueHandle_t queue_handle,
    const SensorReading& reading,
    const char* queue_name)
{
    BaseType_t send_result = xQueueSend(queue_handle, &reading, 0);
    if (send_result == pdTRUE) {
        return true;
    }

    ESP_LOGW(TAG, "queue full %s", queue_name);
    return false;
}


void taskSensors(void* parameter)
{
    (void)parameter;

    TickType_t last_wake_ticks = xTaskGetTickCount();

    while (true) {
        vTaskDelayUntil(
            &last_wake_ticks,
            pdMS_TO_TICKS(SENSOR_INTERVAL_MS));

        SensorReading reading = {};
        bool was_read = Sensors::readCurrent(reading);
        if (!was_read) {
            continue;
        }

        bool sent_mqtt = sendSensorReading(
            sensor_to_mqtt_queue,
            reading,
            "sensor_to_mqtt");
        bool sent_led = sendSensorReading(
            sensor_to_led_queue,
            reading,
            "sensor_to_led");

        if (!sent_mqtt || !sent_led) {
            continue;
        }
    }
}
}  // namespace


namespace TaskManager {
bool createQueues()
{
    clearQueues();

    sensor_to_mqtt_queue = xQueueCreate(
        QUEUE_SENSOR_TO_MQTT_LENGTH,
        sizeof(SensorReading));
    sensor_to_led_queue = xQueueCreate(
        QUEUE_SENSOR_TO_LED_LENGTH,
        sizeof(SensorReading));
    command_dispatch_queue = xQueueCreate(
        QUEUE_COMMAND_LENGTH,
        sizeof(CommandEnvelope));
    status_to_mqtt_queue = xQueueCreate(
        QUEUE_STATUS_LENGTH,
        sizeof(StatusMessage));
    energy_to_mqtt_queue = xQueueCreate(
        QUEUE_ENERGY_LENGTH,
        sizeof(EnergyMessage));
    ack_to_mqtt_queue = xQueueCreate(
        QUEUE_ACK_LENGTH,
        sizeof(AckMessage));

    if (sensor_to_mqtt_queue == nullptr ||
        sensor_to_led_queue == nullptr ||
        command_dispatch_queue == nullptr ||
        status_to_mqtt_queue == nullptr ||
        energy_to_mqtt_queue == nullptr ||
        ack_to_mqtt_queue == nullptr) {
        ESP_LOGE(TAG, "queue create failed");
        clearQueues();
        return false;
    }

    ESP_LOGI(TAG, "queues created");
    return true;
}


bool createTasks()
{
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
        TASK_SENSORS_CORE);

    if (sensor_result != pdPASS) {
        ESP_LOGE(TAG, "TaskSensors create failed");
        sensors_task_handle = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "TaskSensors created");
    return true;
}
}  // namespace TaskManager

#include "task_manager.h"

#include <esp_log.h>

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
    return false;
}
}  // namespace TaskManager

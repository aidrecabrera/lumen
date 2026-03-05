#pragma once

#include "types.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

extern QueueHandle_t sensor_to_mqtt_queue;

extern QueueHandle_t sensor_to_led_queue;

extern QueueHandle_t command_dispatch_queue;

extern QueueHandle_t status_to_mqtt_queue;

extern QueueHandle_t energy_to_mqtt_queue;

extern QueueHandle_t ack_to_mqtt_queue;

namespace TaskManager {

bool createQueues();

bool createTasks();

}

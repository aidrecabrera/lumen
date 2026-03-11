#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "lumen_app_types.h"

namespace TaskManager {

bool createQueues();
bool createTasks();
bool dispatchCommand(const CommandEnvelope& command);

}  // namespace TaskManager
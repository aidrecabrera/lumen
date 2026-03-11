#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "types.h"

namespace TaskManager {

bool createQueues();
bool createTasks();
bool dispatchCommand(const CommandEnvelope& command);

}  // namespace TaskManager
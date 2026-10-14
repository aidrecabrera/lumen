#pragma once

#include "lumen_app_types.h"

namespace TaskManager {

bool createQueues();
bool createTasks();
bool dispatchCommand(const CommandEnvelope& command);

}  // namespace TaskManager
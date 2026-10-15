#pragma once

#include "lumen_app_types.h"

namespace TaskManager {

void createQueues();
// sensors and leds run without a network, so they start before the setup portal.
bool createControlTasks();
bool createNetworkTasks();
bool dispatchCommand(const CommandEnvelope& command);

}  // namespace TaskManager
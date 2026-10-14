#pragma once

#include "lumen_app_types.h"

namespace LedControl {

bool init();
AckMessage applyCommand(const CommandEnvelope& command);
// REJECTED ack for a command that never reached applyCommand (bad payload, full queue).
// It carries the current mode and LED state, like the other acks.
AckMessage buildRejectedAck(const char* command_id, const char* reason);
void controlTick(const SensorReading& latest_reading);
LedState getCurrentLedState();
DeviceMode getCurrentMode();
StatusMessage buildStatusMessage(DegradedTier degraded_tier);

}  // namespace LedControl
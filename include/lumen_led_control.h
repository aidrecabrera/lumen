#pragma once

#include "lumen_app_types.h"

namespace LedControl {

bool init();
AckMessage applyCommand(const CommandEnvelope& command);
void controlTick(const SensorReading& latest_reading);
LedState getCurrentLedState();
DeviceMode getCurrentMode();
StatusMessage buildStatusMessage(DegradedTier degraded_tier);

}  // namespace LedControl
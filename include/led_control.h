#pragma once

#include "types.h"

namespace LedControl {

bool init();

AckMessage applyCommand(const CommandEnvelope& command);

void controlTick(const SensorReading& latest_reading);

const LedState& getCurrentLedState();

DeviceMode getCurrentMode();

StatusMessage buildStatusMessage(uint8_t degradedTier);

}  // namespace LedControl

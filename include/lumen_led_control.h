#pragma once

#include "lumen_app_types.h"

namespace LedControl {

bool init();
AckMessage applyCommand(const CommandEnvelope& command);
// REJECTED ack for a command that never reached applyCommand (bad payload, full queue).
// It carries the current mode and LED state, like the other acks.
AckMessage buildRejectedAck(const CommandId& command_id, CommandSource source, AckReason reason);
// true when the led state or the auto state changed.
bool controlTick(const SensorReading& latest_reading);
LedState getCurrentLedState();
DeviceMode getCurrentMode();
StatusMessage buildStatusMessage();
// Pixel level 0..255 for one channel. Public so the device tests check the real code.
uint8_t scaleChannel(uint8_t brightness_pct, uint8_t channel_pct);

}  // namespace LedControl
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "lumen_app_types.h"

// ValidThresholds::parse, ValidLedState::parse and EnergyTotalWh::parse (lumen_app_types.h)
// are implemented in lumen_type_validation.cpp.
bool parseDeviceMode(uint8_t raw_mode, DeviceMode& out_mode);

// Accepts 1 to COMMAND_ID_MAX_LEN - 1 characters from [A-Za-z0-9_-]. length is the byte count
// of text, so an embedded NUL is rejected. The id becomes an MQTT topic level, and MQTT 3.1.1
// section 4.7.1 forbids '+' and '#' in a publish topic; '/' would add a level.
// On failure out_id is not changed.
bool parseCommandId(const char* text, size_t length, CommandId& out_id);

#include "lumen_type_validation.h"

#include <math.h>
#include <string.h>

namespace {
bool isCommandIdChar(char value) {
    const bool is_lower = (value >= 'a') && (value <= 'z');
    const bool is_upper = (value >= 'A') && (value <= 'Z');
    const bool is_digit = (value >= '0') && (value <= '9');
    const bool is_mark = (value == '-') || (value == '_');
    return is_lower || is_upper || is_digit || is_mark;
}
}  // namespace

bool isThresholdConfigValid(const ThresholdConfig& thresholds) {
    if (!isfinite(thresholds.temp_min_c) || !isfinite(thresholds.temp_max_c) ||
        !isfinite(thresholds.humidity_min_pct) || !isfinite(thresholds.humidity_max_pct)) {
        return false;
    }

    return (thresholds.temp_min_c <= thresholds.temp_max_c) &&
           (thresholds.humidity_min_pct <= thresholds.humidity_max_pct);
}

bool isLedStateValid(const LedState& led) {
    const uint16_t total_pct = static_cast<uint16_t>(led.red_dist_pct) +
                               static_cast<uint16_t>(led.blue_dist_pct) +
                               static_cast<uint16_t>(led.far_red_dist_pct);

    return (led.brightness_pct <= 100U) && (total_pct == 100U);
}

bool isEnergyTotalValid(float total_wh) { return isfinite(total_wh) && (total_wh >= 0.0f); }

bool parseDeviceMode(uint8_t raw_mode, DeviceMode& out_mode) {
    if (raw_mode == static_cast<uint8_t>(DeviceMode::AUTONOMOUS)) {
        out_mode = DeviceMode::AUTONOMOUS;
        return true;
    }

    if (raw_mode == static_cast<uint8_t>(DeviceMode::MANUAL)) {
        out_mode = DeviceMode::MANUAL;
        return true;
    }

    return false;
}

bool parseCommandId(const char* text, size_t length, CommandId& out_id) {
    if (text == nullptr) {
        return false;
    }

    if (length == 0U || length >= COMMAND_ID_MAX_LEN) {
        return false;
    }

    for (size_t index = 0; index < length; ++index) {
        if (!isCommandIdChar(text[index])) {
            return false;
        }
    }

    memcpy(out_id.text_, text, length);
    out_id.text_[length] = '\0';
    return true;
}

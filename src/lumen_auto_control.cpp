#include "lumen_auto_control.h"

#include <assert.h>
#include <math.h>

#include "lumen_board_config.h"

namespace {
bool isNight(AutoState previous, float light_lux) {
    if (previous == AutoState::NIGHT_OFF) {
        return light_lux <= NIGHT_ON_ABOVE_LUX;
    }

    return light_lux < NIGHT_OFF_BELOW_LUX;
}

bool isWithinThresholds(const SensorReading& reading, const ThresholdConfig& thresholds) {
    return (reading.temperature_c >= thresholds.temp_min_c) &&
           (reading.temperature_c <= thresholds.temp_max_c) &&
           (reading.humidity_pct >= thresholds.humidity_min_pct) &&
           (reading.humidity_pct <= thresholds.humidity_max_pct);
}

uint8_t brightnessFor(float light_lux, float lux_ceiling) {
    if (light_lux >= lux_ceiling) {
        return 0U;
    }

    const float brightness_pct = (1.0f - (light_lux / lux_ceiling)) * 100.0f;
    return static_cast<uint8_t>(fminf(brightness_pct, 100.0f) + 0.5f);
}
}  // namespace

AutoDecision decideAutonomous(
    AutoState previous,
    const SensorReading& reading,
    const ThresholdConfig& thresholds,
    ValidLuxCeiling lux_ceiling
) {
    // readCurrent reports only finite readings with lux >= 0.
    assert(isfinite(reading.light_lux) && reading.light_lux >= 0.0f);

    if (isNight(previous, reading.light_lux)) {
        return {AutoState::NIGHT_OFF, 0U};
    }

    if (!isWithinThresholds(reading, thresholds)) {
        return {AutoState::PAUSED_OUT_OF_RANGE, std::nullopt};
    }

    return {AutoState::ACTIVE, brightnessFor(reading.light_lux, lux_ceiling.raw())};
}

#pragma once

#include <optional>

#include "lumen_app_types.h"

struct AutoDecision {
    AutoState state;
    // empty: hold the current brightness.
    std::optional<uint8_t> brightness_pct;
};

// previous carries the night-off hysteresis.
AutoDecision decideAutonomous(
    AutoState previous,
    const SensorReading& reading,
    const ThresholdConfig& thresholds,
    ValidLuxCeiling lux_ceiling
);

#include "lumen_type_validation.h"

#include <math.h>

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
#pragma once

#include "types.h"

namespace ConfigManager {

bool init();

bool loadDefaults();

const RuntimeConfig& getConfig();

bool saveThresholds(const ThresholdConfig& thresholds);

bool saveMode(DeviceMode mode);

bool saveLedState(const LedState& led);

bool saveEnergyTotal(float total_wh);

}  // namespace ConfigManager

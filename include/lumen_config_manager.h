#pragma once

#include "lumen_app_types.h"

namespace ConfigManager {

bool init();
bool loadDefaults();
RuntimeConfig getConfigSnapshot();
bool saveThresholds(const ValidThresholds& thresholds);
bool saveMode(DeviceMode mode);
bool saveLedState(const ValidLedState& led);
bool saveEnergyTotal(EnergyTotalWh total_wh);
bool saveRuntimeConfig(const RuntimeConfig& config);

}  // namespace ConfigManager

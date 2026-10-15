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

// saveSetup clears the has-connected mark until the next good link.
bool saveSetup(const ValidWifiCredentials& credentials, const LocalPasswordHash& password_hash);
std::optional<ValidWifiCredentials> loadWifiCredentials();
std::optional<LocalPasswordHash> loadLocalPasswordHash();
bool wifiHasConnected();
void markWifiConnected();

}  // namespace ConfigManager

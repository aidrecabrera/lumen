#pragma once

#include "lumen_app_types.h"

bool isThresholdConfigValid(const ThresholdConfig& thresholds);
bool isLedStateValid(const LedState& led);
bool isEnergyTotalValid(float total_wh);
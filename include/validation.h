#pragma once

#include "types.h"

bool isThresholdConfigValid(const ThresholdConfig& thresholds);
bool isLedStateValid(const LedState& led);
bool isEnergyTotalValid(float total_wh);
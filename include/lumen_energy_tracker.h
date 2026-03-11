#pragma once

#include "lumen_app_types.h"

namespace EnergyTracker {

bool init();

void updateFromLedState(const LedState& led);

EnergyMessage getSnapshot();

bool requestPersist();

}  // namespace EnergyTracker

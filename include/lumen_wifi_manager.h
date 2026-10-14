#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lumen_app_types.h"

namespace WifiManager {

bool init();

bool connectOrPoll();

bool isConnected();

DegradedTier getDegradedTier();

void resetReconnect();

}  // namespace WifiManager

#pragma once

#include <stdbool.h>
#include <stdint.h>

namespace WifiManager {

bool init();

bool connectOrPoll();

bool isConnected();

uint8_t getDegradedTier();

void resetReconnect();

}  // namespace WifiManager

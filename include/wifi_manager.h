#pragma once

#include <stdint.h>
#include <stdbool.h>

namespace WifiManager {

bool init();

bool connectOrPoll();

bool isConnected();

uint8_t getDegradedTier();

void resetReconnect();

}

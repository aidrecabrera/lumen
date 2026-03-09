#pragma once

#include <stdint.h>

#include "types.h"

namespace CommandHandler {

bool init();

bool handleInbound(const char* topic, const uint8_t* payload, uint16_t length);

}  // namespace CommandHandler

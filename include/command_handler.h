#pragma once

#include "types.h"

#include <stdint.h>

namespace CommandHandler {

bool init();

bool handleInbound(
    const char* topic,
    const uint8_t* payload,
    uint16_t length
);

}

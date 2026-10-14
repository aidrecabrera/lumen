#pragma once

#include <stdint.h>

#include "lumen_app_types.h"

namespace CommandHandler {

// Decodes one inbound MQTT command. The command kind comes from the topic suffix.
// Has no side effects except logging.
bool decode(
    const char* topic, const uint8_t* payload, uint16_t length, CommandEnvelope& out_command
);

}  // namespace CommandHandler

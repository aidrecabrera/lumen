#pragma once

#include <stdint.h>

#include "lumen_app_types.h"

namespace CommandHandler {

enum class DecodeStatus : uint8_t {
    // out_command is complete and valid.
    DECODED,
    // The payload has a valid command_id, but other fields are missing or invalid.
    // Only out_command.command_id and out_command.kind are set. The caller sends a REJECTED ack.
    INVALID_FIELDS,
    // No usable command_id (bad frame, bad MsgPack, unknown topic), so no ack topic exists.
    UNADDRESSABLE,
};

// Decodes one inbound MQTT command. The command kind comes from the topic suffix.
// Has no side effects except logging.
DecodeStatus decode(
    const char* topic, const uint8_t* payload, uint16_t length, CommandEnvelope& out_command
);

}  // namespace CommandHandler

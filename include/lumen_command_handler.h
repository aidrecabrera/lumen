#pragma once

#include <stdint.h>

#include "lumen_app_types.h"

namespace CommandHandler {

enum class DecodeStatus : uint8_t {
    // out_command is complete and valid.
    DECODED,
    // The payload has a valid command_id, but other fields are missing or invalid.
    // Only out_command.command_id is set. The caller sends a REJECTED ack.
    INVALID_FIELDS,
    // No usable command_id (bad frame, bad MsgPack, missing or invalid id), so no ack topic
    // exists.
    UNADDRESSABLE,
};

// Decodes one inbound MQTT command. The MQTT client gets kind from the exact topic.
// Has no side effects except logging.
DecodeStatus decode(
    CommandKind kind, const uint8_t* payload, uint16_t length, CommandEnvelope& out_command
);

// for a frame of unknown kind, so it can still get a rejected ack.
bool readCommandId(const uint8_t* payload, uint16_t length, CommandId& out_command_id);

}  // namespace CommandHandler

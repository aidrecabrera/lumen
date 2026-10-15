#pragma once

#include <stddef.h>
#include <stdint.h>

#include "lumen_app_types.h"

// pure, so the native tests compare it byte for byte with test/fixtures/contract.
namespace WireCodec {

// 0 when the message does not fit.
size_t encodeTelemetry(const SensorReading& reading, uint8_t* out, size_t capacity);
size_t encodeStatus(const StatusMessage& status, uint8_t* out, size_t capacity);
size_t encodeEnergy(const EnergyMessage& energy, uint8_t* out, size_t capacity);
size_t encodeAck(const AckMessage& ack, uint8_t* out, size_t capacity);

// local frame: one msgpack map {"t": topic suffix, "p": payload as bin}.
size_t encodeFrame(
    const char* suffix, const uint8_t* payload, size_t length, uint8_t* out, size_t capacity
);

enum class FrameStatus : uint8_t { COMMAND, UNKNOWN_SUFFIX, MALFORMED, OVERSIZED };

struct InboundFrame {
    CommandKind kind;
    uint16_t length;
    uint8_t payload[COMMAND_PAYLOAD_MAX_BYTES];
};

// COMMAND and UNKNOWN_SUFFIX copy the payload; kind is set only for COMMAND.
FrameStatus decodeFrame(const uint8_t* bytes, size_t length, InboundFrame& out_frame);

bool commandKindForSuffix(const char* suffix, CommandKind& out_kind);

}  // namespace WireCodec

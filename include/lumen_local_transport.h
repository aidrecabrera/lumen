#pragma once

#include "lumen_app_types.h"

using RejectUnknownKindFn = void (*)(const CommandId& command_id);

namespace LocalTransport {

bool init(const char* hostname, const LocalPasswordHash& password_hash);

void poll(InboundCommandFn on_command, RejectUnknownKindFn on_unknown_kind);

// true once per new client, so the caller can send it status.
bool takeNewClient();

// best effort: a client that misses a message gets the latest state on connect.
void publishTelemetry(const SensorReading& reading);
void publishStatus(const StatusMessage& status);
void publishEnergy(const EnergyMessage& energy);
void publishAck(const AckMessage& ack);

}  // namespace LocalTransport

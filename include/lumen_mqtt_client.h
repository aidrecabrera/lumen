#pragma once

#include <Client.h>

#include "lumen_app_types.h"

// kind comes from an exact match on one of the subscribed command topics.
using MqttInboundCallback = void (*)(CommandKind kind, const uint8_t* payload, uint16_t length);
using WifiConnectedFn = bool (*)();

namespace MqttClient {

bool init(const char* device_id);
bool init(const char* device_id, Client& net_client, WifiConnectedFn wifi_connected);

bool connectOrPoll();
bool publishTelemetry(const SensorReading& reading);
bool publishStatus(const StatusMessage& status);
bool publishEnergy(const EnergyMessage& energy);
bool publishAck(const AckMessage& ack);
void registerInboundCallback(MqttInboundCallback callback);

}  // namespace MqttClient
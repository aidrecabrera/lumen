#pragma once

#include <Client.h>

#include "lumen_app_types.h"

using MqttInboundCallback = void (*)(const char* topic, const uint8_t* payload, uint16_t length);
using WifiConnectedFn = bool (*)();

namespace MqttClient {

bool init(const char* device_id);
bool init(const char* device_id, Client& net_client, WifiConnectedFn wifi_connected);

bool connectOrPoll();
bool publishTelemetry(const SensorReading& reading);
bool publishStatus(const StatusMessage& status);
bool publishEnergy(const EnergyMessage& energy);
bool publishAck(const AckMessage& ack);
bool setupSubscriptions();
void registerInboundCallback(MqttInboundCallback callback);
bool isConnected();

}  // namespace MqttClient
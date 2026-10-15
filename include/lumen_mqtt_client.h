#pragma once

#include <Client.h>

#include "lumen_app_types.h"
#include "lumen_cloud_config.h"

using WifiConnectedFn = bool (*)();

namespace MqttClient {

// config must outlive the client: the tls client keeps pointers into its pem strings.
bool init(const char* device_id, const CloudConfig& config);
bool init(
    const char* device_id,
    const CloudConfig& config,
    Client& net_client,
    WifiConnectedFn wifi_connected
);
void disable(CloudState reason);
CloudState state();

bool connectOrPoll();
bool publishTelemetry(const SensorReading& reading);
bool publishStatus(const StatusMessage& status);
bool publishEnergy(const EnergyMessage& energy);
bool publishAck(const AckMessage& ack);
void registerInboundCallback(InboundCommandFn callback);

}  // namespace MqttClient

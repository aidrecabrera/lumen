#include "mqtt_client.h"

#include <stdio.h>
#include <string.h>

#include <PubSubClient.h>
#include <WiFiClient.h>
#include <esp_log.h>

#include "config.h"

namespace {
static const char* TAG = "mqtt_client";

static constexpr size_t TOPIC_LEN = 96;
static constexpr size_t CLIENT_ID_LEN = 40;

static WiFiClient wifi_client;
static PubSubClient broker_client(wifi_client);

static bool is_initialized = false;
static char device_id_buf[DEVICE_ID_MAX_LEN] = {};
static char client_id_buf[CLIENT_ID_LEN] = {};
static char telemetry_topic[TOPIC_LEN] = {};
static char status_topic[TOPIC_LEN] = {};
static char energy_topic[TOPIC_LEN] = {};
static char cmd_led_topic[TOPIC_LEN] = {};
static char cmd_config_topic[TOPIC_LEN] = {};
static char cmd_mode_topic[TOPIC_LEN] = {};

static const char* WILL_PAYLOAD = "offline";


bool hasText(const char* text)
{
    if (text == nullptr) {
        return false;
    }

    return text[0] != '\0';
}


void copyText(char* dest, size_t dest_len, const char* src)
{
    if (dest == nullptr || dest_len == 0) {
        return;
    }

    if (src == nullptr) {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, dest_len - 1);
    dest[dest_len - 1] = '\0';
}


bool buildTopic(char* out_topic, size_t out_len, const char* suffix)
{
    int written = snprintf(
        out_topic,
        out_len,
        "%s/%s/%s",
        MQTT_TOPIC_ROOT,
        device_id_buf,
        suffix);

    if (written <= 0) {
        return false;
    }

    if (static_cast<size_t>(written) >= out_len) {
        return false;
    }

    return true;
}


bool buildClientId()
{
    int written = snprintf(
        client_id_buf,
        sizeof(client_id_buf),
        "spot-fw-%s",
        device_id_buf);

    if (written <= 0) {
        return false;
    }

    if (static_cast<size_t>(written) >= sizeof(client_id_buf)) {
        return false;
    }

    return true;
}


bool buildTopics()
{
    bool has_telemetry = buildTopic(
        telemetry_topic,
        sizeof(telemetry_topic),
        MQTT_TOPIC_TELEMETRY_SUFFIX);
    bool has_status = buildTopic(
        status_topic,
        sizeof(status_topic),
        MQTT_TOPIC_STATUS_SUFFIX);
    bool has_energy = buildTopic(
        energy_topic,
        sizeof(energy_topic),
        MQTT_TOPIC_ENERGY_SUFFIX);
    bool has_cmd_led = buildTopic(
        cmd_led_topic,
        sizeof(cmd_led_topic),
        MQTT_TOPIC_CMD_LED_SUFFIX);
    bool has_cmd_config = buildTopic(
        cmd_config_topic,
        sizeof(cmd_config_topic),
        MQTT_TOPIC_CMD_CONFIG_SUFFIX);
    bool has_cmd_mode = buildTopic(
        cmd_mode_topic,
        sizeof(cmd_mode_topic),
        MQTT_TOPIC_CMD_MODE_SUFFIX);

    return has_telemetry &&
        has_status &&
        has_energy &&
        has_cmd_led &&
        has_cmd_config &&
        has_cmd_mode;
}
}  // namespace


namespace MqttClient {
bool init(const char* device_id)
{
    if (!hasText(device_id)) {
        ESP_LOGE(TAG, "device id missing");
        return false;
    }

    copyText(device_id_buf, sizeof(device_id_buf), device_id);

    bool has_client_id = buildClientId();
    bool has_topics = buildTopics();

    if (!has_client_id || !has_topics) {
        ESP_LOGE(TAG, "topic init failed");
        return false;
    }

    broker_client.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT);
    broker_client.setKeepAlive(MQTT_KEEPALIVE_SEC);
    broker_client.setBufferSize(COMMAND_DOC_SIZE);

    is_initialized = true;
    ESP_LOGI(TAG, "mqtt initialized for %s", device_id_buf);
    return true;
}


bool connectOrPoll()
{
    return false;
}


bool publishTelemetry(const SensorReading& reading)
{
    (void)reading;
    return false;
}


bool publishStatus(const StatusMessage& status)
{
    (void)status;
    return false;
}


bool publishEnergy(const EnergyMessage& energy)
{
    (void)energy;
    return false;
}


bool publishAck(const AckMessage& ack)
{
    (void)ack;
    return false;
}


bool setupSubscriptions()
{
    return false;
}


void registerInboundCallback(MqttInboundCallback callback)
{
    (void)callback;
}


bool isConnected()
{
    if (!is_initialized) {
        return false;
    }

    return broker_client.connected();
}
}  // namespace MqttClient

#include "mqtt_client.h"

#include <stdio.h>
#include <string.h>

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "config.h"
#include "wifi_manager.h"

namespace {
static const char* TAG = "mqtt_client";

static constexpr size_t TOPIC_LEN = 96;
static constexpr size_t CLIENT_ID_LEN = 40;
static constexpr size_t ACK_TOPIC_LEN = 128;

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
static uint32_t reconnect_backoff_ms = WIFI_BACKOFF_BASE_MS;
static uint64_t next_connect_ms = 0;
static MqttInboundCallback inbound_callback = nullptr;  // typedef is global scope


uint64_t getNowMs()
{
    return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
}


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


bool buildAckTopic(
    char* out_topic,
    size_t out_len,
    const char* command_id)
{
    int written = snprintf(
        out_topic,
        out_len,
        "%s/%s/%s/%s",
        MQTT_TOPIC_ROOT,
        device_id_buf,
        MQTT_TOPIC_ACK_PREFIX,
        command_id);

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


uint32_t nextBackoffMs(uint32_t current_backoff_ms)
{
    if (current_backoff_ms >= WIFI_BACKOFF_MAX_MS) {
        return WIFI_BACKOFF_MAX_MS;
    }

    uint32_t doubled_backoff_ms = current_backoff_ms * 2U;
    if (doubled_backoff_ms > WIFI_BACKOFF_MAX_MS) {
        return WIFI_BACKOFF_MAX_MS;
    }

    return doubled_backoff_ms;
}


void resetReconnectState()
{
    reconnect_backoff_ms = WIFI_BACKOFF_BASE_MS;
    next_connect_ms = 0;
}


bool connectBroker()
{
    bool was_connected = broker_client.connect(
        client_id_buf,
        status_topic,
        MQTT_QOS_STATUS,
        true,
        WILL_PAYLOAD);

    if (!was_connected) {
        ESP_LOGW(TAG, "broker connect failed");
        return false;
    }

    resetReconnectState();
    ESP_LOGI(TAG, "broker connected");
    return true;
}


void fillLedObject(JsonObject led_obj, const LedState& led)
{
    led_obj["power"] = led.power;
    led_obj["brightness_pct"] = led.brightness_pct;
    led_obj["red_enabled"] = led.red_enabled;
    led_obj["blue_enabled"] = led.blue_enabled;
    led_obj["far_red_enabled"] = led.far_red_enabled;
    led_obj["red_dist_pct"] = led.red_dist_pct;
    led_obj["blue_dist_pct"] = led.blue_dist_pct;
    led_obj["far_red_dist_pct"] = led.far_red_dist_pct;
}


bool publishPayload(
    const char* topic,
    const uint8_t* payload_buf,
    size_t payload_len)
{
    if (!broker_client.connected()) {
        ESP_LOGW(TAG, "publish while offline");
        return false;
    }

    bool was_published = broker_client.publish(
        topic,
        payload_buf,
        payload_len,
        false);

    if (!was_published) {
        ESP_LOGW(TAG, "publish failed %s", topic);
        return false;
    }

    return true;
}


bool encodeTelemetry(
    const SensorReading& reading,
    uint8_t* payload_buf,
    size_t payload_len,
    size_t& encoded_len)
{
    StaticJsonDocument<TELEMETRY_DOC_SIZE> doc;
    doc["timestamp_ms"] = reading.timestamp_ms;
    doc["light_lux"] = reading.light_lux;
    doc["temperature_c"] = reading.temperature_c;
    doc["humidity_pct"] = reading.humidity_pct;
    doc["sequence"] = reading.sequence;

    encoded_len = serializeMsgPack(doc, payload_buf, payload_len);
    return encoded_len > 0;
}


bool encodeStatus(
    const StatusMessage& status,
    uint8_t* payload_buf,
    size_t payload_len,
    size_t& encoded_len)
{
    StaticJsonDocument<STATUS_DOC_SIZE> doc;
    doc["timestamp_ms"] = status.timestamp_ms;
    doc["mode"] = static_cast<uint8_t>(status.mode);
    doc["degraded_tier"] = status.degraded_tier;
    doc["uptime_sec"] = status.uptime_sec;

    JsonObject led_obj = doc.createNestedObject("led");
    fillLedObject(led_obj, status.led);

    encoded_len = serializeMsgPack(doc, payload_buf, payload_len);
    return encoded_len > 0;
}


bool encodeEnergy(
    const EnergyMessage& energy,
    uint8_t* payload_buf,
    size_t payload_len,
    size_t& encoded_len)
{
    StaticJsonDocument<ENERGY_DOC_SIZE> doc;
    doc["timestamp_ms"] = energy.timestamp_ms;
    doc["total_wh"] = energy.total_wh;
    doc["session_wh"] = energy.session_wh;
    doc["light_on_sec"] = energy.light_on_sec;
    doc["max_brightness_sec"] = energy.max_brightness_sec;

    encoded_len = serializeMsgPack(doc, payload_buf, payload_len);
    return encoded_len > 0;
}


bool encodeAck(
    const AckMessage& ack,
    uint8_t* payload_buf,
    size_t payload_len,
    size_t& encoded_len)
{
    StaticJsonDocument<STATUS_DOC_SIZE> doc;
    doc["command_id"] = ack.command_id;
    doc["result"] = static_cast<uint8_t>(ack.result);
    doc["timestamp_ms"] = ack.timestamp_ms;
    doc["mode"] = static_cast<uint8_t>(ack.mode);
    doc["reason"] = ack.reason;

    JsonObject led_obj = doc.createNestedObject("led");
    fillLedObject(led_obj, ack.led);

    encoded_len = serializeMsgPack(doc, payload_buf, payload_len);
    return encoded_len > 0;
}


bool isCommandTopic(const char* topic)
{
    if (topic == nullptr) {
        return false;
    }

    if (strcmp(topic, cmd_led_topic) == 0) {
        return true;
    }

    if (strcmp(topic, cmd_config_topic) == 0) {
        return true;
    }

    if (strcmp(topic, cmd_mode_topic) == 0) {
        return true;
    }

    return false;
}


void mqttCallback(char* topic, uint8_t* payload, unsigned int length)
{
    if (inbound_callback == nullptr) {
        return;
    }

    if (!isCommandTopic(topic)) {
        return;
    }

    if (length > UINT16_MAX) {
        ESP_LOGW(TAG, "payload too large");
        return;
    }

    inbound_callback(
        topic,
        payload,
        static_cast<uint16_t>(length));
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
    bool has_buffer = broker_client.setBufferSize(COMMAND_DOC_SIZE);
    if (!has_buffer) {
        ESP_LOGW(TAG, "mqtt buffer size set failed");
    }
    broker_client.setCallback(mqttCallback);

    resetReconnectState();
    is_initialized = true;

    ESP_LOGI(TAG, "mqtt initialized for %s", device_id_buf);
    return true;
}


bool connectOrPoll()
{
    if (!is_initialized) {
        ESP_LOGW(TAG, "poll before init");
        return false;
    }

    if (!WifiManager::isConnected()) {
        return false;
    }

    if (broker_client.connected()) {
        broker_client.loop();
        return true;
    }

    uint64_t now_ms = getNowMs();
    if (now_ms < next_connect_ms) {
        return false;
    }

    bool was_connected = connectBroker();
    if (!was_connected) {
        next_connect_ms = now_ms + reconnect_backoff_ms;
        reconnect_backoff_ms = nextBackoffMs(reconnect_backoff_ms);
        return false;
    }

    bool has_subscriptions = setupSubscriptions();
    if (!has_subscriptions) {
        ESP_LOGW(TAG, "subscription setup failed");
    }

    return true;
}


bool publishTelemetry(const SensorReading& reading)
{
    uint8_t payload_buf[TELEMETRY_DOC_SIZE] = {};
    size_t encoded_len = 0;

    bool was_encoded = encodeTelemetry(
        reading,
        payload_buf,
        sizeof(payload_buf),
        encoded_len);

    if (!was_encoded) {
        ESP_LOGW(TAG, "telemetry encode failed");
        return false;
    }

    return publishPayload(telemetry_topic, payload_buf, encoded_len);
}


bool publishStatus(const StatusMessage& status)
{
    uint8_t payload_buf[STATUS_DOC_SIZE] = {};
    size_t encoded_len = 0;

    bool was_encoded = encodeStatus(
        status,
        payload_buf,
        sizeof(payload_buf),
        encoded_len);

    if (!was_encoded) {
        ESP_LOGW(TAG, "status encode failed");
        return false;
    }

    return publishPayload(status_topic, payload_buf, encoded_len);
}


bool publishEnergy(const EnergyMessage& energy)
{
    uint8_t payload_buf[ENERGY_DOC_SIZE] = {};
    size_t encoded_len = 0;

    bool was_encoded = encodeEnergy(
        energy,
        payload_buf,
        sizeof(payload_buf),
        encoded_len);

    if (!was_encoded) {
        ESP_LOGW(TAG, "energy encode failed");
        return false;
    }

    return publishPayload(energy_topic, payload_buf, encoded_len);
}


bool publishAck(const AckMessage& ack)
{
    uint8_t payload_buf[STATUS_DOC_SIZE] = {};
    char ack_topic[ACK_TOPIC_LEN] = {};
    size_t encoded_len = 0;

    bool has_topic = buildAckTopic(
        ack_topic,
        sizeof(ack_topic),
        ack.command_id);
    if (!has_topic) {
        ESP_LOGW(TAG, "ack topic build failed");
        return false;
    }

    bool was_encoded = encodeAck(
        ack,
        payload_buf,
        sizeof(payload_buf),
        encoded_len);

    if (!was_encoded) {
        ESP_LOGW(TAG, "ack encode failed");
        return false;
    }

    return publishPayload(ack_topic, payload_buf, encoded_len);
}


bool setupSubscriptions()
{
    if (!broker_client.connected()) {
        return false;
    }

    bool has_led = broker_client.subscribe(
        cmd_led_topic,
        MQTT_QOS_COMMAND);
    bool has_config = broker_client.subscribe(
        cmd_config_topic,
        MQTT_QOS_COMMAND);
    bool has_mode = broker_client.subscribe(
        cmd_mode_topic,
        MQTT_QOS_COMMAND);

    return has_led && has_config && has_mode;
}


void registerInboundCallback(MqttInboundCallback callback)
{
    inbound_callback = callback;
}


bool isConnected()
{
    if (!is_initialized) {
        return false;
    }

    return broker_client.connected();
}
}  // namespace MqttClient
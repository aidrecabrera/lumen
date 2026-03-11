#include "lumen_mqtt_client.h"

#include <ArduinoJson.h>
#include <Client.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <string.h>

#include "lumen_board_config.h"
#include "lumen_system_utils.h"
#include "lumen_wifi_manager.h"

namespace {
static const char* TAG = "mqtt_client";

static constexpr uint32_t MQTT_BACKOFF_BASE_MS = 1000U;
static constexpr uint32_t MQTT_BACKOFF_MAX_MS = 30000U;

static constexpr size_t TOPIC_LEN = 96U;
static constexpr size_t ACK_TOPIC_LEN = 128U;
static constexpr size_t CLIENT_ID_LEN = sizeof("lumen-fw-") + DEVICE_ID_MAX_LEN;
static constexpr size_t ACK_DOC_SIZE = STATUS_DOC_SIZE + 128U;

static constexpr size_t INBOUND_QUEUE_LENGTH = 4U;
static constexpr size_t MQTT_FIXED_HEADER_SLACK = 16U;

constexpr size_t maxSize(size_t a, size_t b) { return (a > b) ? a : b; }

static constexpr size_t MQTT_MAX_PAYLOAD_SIZE = maxSize(
    COMMAND_DOC_SIZE,
    maxSize(TELEMETRY_DOC_SIZE, maxSize(STATUS_DOC_SIZE, maxSize(ENERGY_DOC_SIZE, ACK_DOC_SIZE)))
);

static constexpr uint16_t MQTT_PACKET_BUFFER_SIZE =
    static_cast<uint16_t>(ACK_TOPIC_LEN + MQTT_MAX_PAYLOAD_SIZE + MQTT_FIXED_HEADER_SLACK);

static const char* const kAvailabilityOnline = "online";
static const char* const kAvailabilityOffline = "offline";

struct InboundMessage {
    char topic[TOPIC_LEN];
    uint16_t length;
    uint8_t payload[COMMAND_DOC_SIZE];
};

class LockGuard {
   public:
    explicit LockGuard(SemaphoreHandle_t mutex, TickType_t timeout_ticks = pdMS_TO_TICKS(1000))
        : mutex_(mutex), locked_(false) {
        if (mutex_ != nullptr) {
            locked_ = (xSemaphoreTake(mutex_, timeout_ticks) == pdTRUE);
        }
    }

    ~LockGuard() {
        if (locked_) {
            xSemaphoreGive(mutex_);
        }
    }

    bool locked() const { return locked_; }

   private:
    SemaphoreHandle_t mutex_;
    bool locked_;
};

static WiFiClient default_wifi_client;
static Client* network_client = &default_wifi_client;
static PubSubClient broker_client;

static StaticSemaphore_t mqtt_mutex_storage;
static SemaphoreHandle_t mqtt_mutex = nullptr;

static StaticQueue_t inbound_queue_storage;
static uint8_t inbound_queue_buffer[INBOUND_QUEUE_LENGTH * sizeof(InboundMessage)];
static QueueHandle_t inbound_queue = nullptr;

static bool is_initialized = false;
static char device_id_buf[DEVICE_ID_MAX_LEN] = {};
static char client_id_buf[CLIENT_ID_LEN] = {};
static char telemetry_topic[TOPIC_LEN] = {};
static char status_topic[TOPIC_LEN] = {};
static char energy_topic[TOPIC_LEN] = {};
static char availability_topic[TOPIC_LEN] = {};
static char cmd_led_topic[TOPIC_LEN] = {};
static char cmd_config_topic[TOPIC_LEN] = {};
static char cmd_mode_topic[TOPIC_LEN] = {};

static uint32_t reconnect_backoff_ms = MQTT_BACKOFF_BASE_MS;
static uint64_t next_connect_ms = 0;
static bool (*wifi_connected_fn)() = nullptr;
static MqttInboundCallback inbound_callback = nullptr;

bool defaultWifiConnected() { return WifiManager::isConnected(); }

bool hasText(const char* text) { return (text != nullptr) && (text[0] != '\0'); }

bool ensureInfrastructure() {
    if (mqtt_mutex == nullptr) {
        mqtt_mutex = xSemaphoreCreateMutexStatic(&mqtt_mutex_storage);
        if (mqtt_mutex == nullptr) {
            ESP_LOGE(TAG, "mutex create failed");
            return false;
        }
    }

    if (inbound_queue == nullptr) {
        inbound_queue = xQueueCreateStatic(
            INBOUND_QUEUE_LENGTH,
            sizeof(InboundMessage),
            inbound_queue_buffer,
            &inbound_queue_storage
        );
        if (inbound_queue == nullptr) {
            ESP_LOGE(TAG, "queue create failed");
            return false;
        }
    }

    return true;
}

void clearInboundQueue() {
    if (inbound_queue != nullptr) {
        xQueueReset(inbound_queue);
    }
}

bool buildTopic(char* out_topic, size_t out_len, const char* suffix) {
    const int written =
        snprintf(out_topic, out_len, "%s/%s/%s", MQTT_TOPIC_ROOT, device_id_buf, suffix);

    return (written > 0) && (static_cast<size_t>(written) < out_len);
}

bool buildAckTopic(char* out_topic, size_t out_len, const char* command_id) {
    const int written = snprintf(
        out_topic,
        out_len,
        "%s/%s/%s/%s",
        MQTT_TOPIC_ROOT,
        device_id_buf,
        MQTT_TOPIC_ACK_PREFIX,
        command_id
    );

    return (written > 0) && (static_cast<size_t>(written) < out_len);
}

bool buildClientId() {
    const int written =
        snprintf(client_id_buf, sizeof(client_id_buf), "lumen-fw-%s", device_id_buf);
    return (written > 0) && (static_cast<size_t>(written) < sizeof(client_id_buf));
}

bool buildTopics() {
    const bool has_telemetry =
        buildTopic(telemetry_topic, sizeof(telemetry_topic), MQTT_TOPIC_TELEMETRY_SUFFIX);
    const bool has_status =
        buildTopic(status_topic, sizeof(status_topic), MQTT_TOPIC_STATUS_SUFFIX);
    const bool has_energy =
        buildTopic(energy_topic, sizeof(energy_topic), MQTT_TOPIC_ENERGY_SUFFIX);
    const bool has_availability =
        buildTopic(availability_topic, sizeof(availability_topic), MQTT_TOPIC_AVAILABILITY_SUFFIX);
    const bool has_cmd_led =
        buildTopic(cmd_led_topic, sizeof(cmd_led_topic), MQTT_TOPIC_CMD_LED_SUFFIX);
    const bool has_cmd_config =
        buildTopic(cmd_config_topic, sizeof(cmd_config_topic), MQTT_TOPIC_CMD_CONFIG_SUFFIX);
    const bool has_cmd_mode =
        buildTopic(cmd_mode_topic, sizeof(cmd_mode_topic), MQTT_TOPIC_CMD_MODE_SUFFIX);

    return has_telemetry && has_status && has_energy && has_availability && has_cmd_led &&
           has_cmd_config && has_cmd_mode;
}

uint32_t nextBackoffMs(uint32_t current_backoff_ms) {
    if (current_backoff_ms >= MQTT_BACKOFF_MAX_MS) {
        return MQTT_BACKOFF_MAX_MS;
    }

    const uint32_t doubled_backoff_ms = current_backoff_ms * 2U;
    return (doubled_backoff_ms > MQTT_BACKOFF_MAX_MS) ? MQTT_BACKOFF_MAX_MS : doubled_backoff_ms;
}

void resetReconnectState() {
    reconnect_backoff_ms = MQTT_BACKOFF_BASE_MS;
    next_connect_ms = 0;
}

void scheduleReconnect(uint64_t now_ms) {
    const uint32_t jitter_window_ms = reconnect_backoff_ms / 4U;
    const uint32_t jitter_ms =
        (jitter_window_ms == 0U) ? 0U : (esp_random() % (jitter_window_ms + 1U));

    next_connect_ms = now_ms + reconnect_backoff_ms + jitter_ms;
    reconnect_backoff_ms = nextBackoffMs(reconnect_backoff_ms);
}

void fillLedObject(JsonObject led_obj, const LedState& led) {
    led_obj["power"] = led.power;
    led_obj["brightness_pct"] = led.brightness_pct;
    led_obj["red_enabled"] = led.red_enabled;
    led_obj["blue_enabled"] = led.blue_enabled;
    led_obj["far_red_enabled"] = led.far_red_enabled;
    led_obj["red_dist_pct"] = led.red_dist_pct;
    led_obj["blue_dist_pct"] = led.blue_dist_pct;
    led_obj["far_red_dist_pct"] = led.far_red_dist_pct;
}

bool publishPayloadLocked(
    const char* topic, const uint8_t* payload_buf, size_t payload_len, bool retained
) {
    if (!broker_client.connected()) {
        ESP_LOGW(TAG, "publish while offline");
        return false;
    }

    const bool was_published = broker_client.publish(topic, payload_buf, payload_len, retained);
    if (!was_published) {
        ESP_LOGW(TAG, "publish failed %s", topic);
        return false;
    }

    return true;
}

bool publishPayload(const char* topic, const uint8_t* payload_buf, size_t payload_len) {
    if (!is_initialized || topic == nullptr || payload_buf == nullptr || payload_len == 0U) {
        return false;
    }

    LockGuard lock(mqtt_mutex);
    if (!lock.locked()) {
        ESP_LOGW(TAG, "publish lock timeout");
        return false;
    }

    return publishPayloadLocked(topic, payload_buf, payload_len, false);
}

bool encodeTelemetry(
    const SensorReading& reading, uint8_t* payload_buf, size_t payload_len, size_t& encoded_len
) {
    StaticJsonDocument<TELEMETRY_DOC_SIZE> doc;
    doc["timestamp_ms"] = reading.timestamp_ms;
    doc["light_lux"] = reading.light_lux;
    doc["temperature_c"] = reading.temperature_c;
    doc["humidity_pct"] = reading.humidity_pct;
    doc["sequence"] = reading.sequence;

    if (doc.overflowed()) {
        return false;
    }

    encoded_len = serializeMsgPack(doc, payload_buf, payload_len);
    return encoded_len > 0U;
}

bool encodeStatus(
    const StatusMessage& status, uint8_t* payload_buf, size_t payload_len, size_t& encoded_len
) {
    StaticJsonDocument<STATUS_DOC_SIZE> doc;
    doc["timestamp_ms"] = status.timestamp_ms;
    doc["mode"] = static_cast<uint8_t>(status.mode);
    doc["degraded_tier"] = status.degraded_tier;
    doc["uptime_sec"] = status.uptime_sec;

    JsonObject led_obj = doc.createNestedObject("led");
    fillLedObject(led_obj, status.led);

    if (doc.overflowed()) {
        return false;
    }

    encoded_len = serializeMsgPack(doc, payload_buf, payload_len);
    return encoded_len > 0U;
}

bool encodeEnergy(
    const EnergyMessage& energy, uint8_t* payload_buf, size_t payload_len, size_t& encoded_len
) {
    StaticJsonDocument<ENERGY_DOC_SIZE> doc;
    doc["timestamp_ms"] = energy.timestamp_ms;
    doc["total_wh"] = energy.total_wh;
    doc["session_wh"] = energy.session_wh;
    doc["light_on_sec"] = energy.light_on_sec;
    doc["max_brightness_sec"] = energy.max_brightness_sec;

    if (doc.overflowed()) {
        return false;
    }

    encoded_len = serializeMsgPack(doc, payload_buf, payload_len);
    return encoded_len > 0U;
}

bool encodeAck(
    const AckMessage& ack, uint8_t* payload_buf, size_t payload_len, size_t& encoded_len
) {
    StaticJsonDocument<ACK_DOC_SIZE> doc;
    doc["command_id"] = ack.command_id;
    doc["result"] = static_cast<uint8_t>(ack.result);
    doc["timestamp_ms"] = ack.timestamp_ms;
    doc["mode"] = static_cast<uint8_t>(ack.mode);
    doc["reason"] = ack.reason;

    JsonObject led_obj = doc.createNestedObject("led");
    fillLedObject(led_obj, ack.led);

    if (doc.overflowed()) {
        return false;
    }

    encoded_len = serializeMsgPack(doc, payload_buf, payload_len);
    return encoded_len > 0U;
}

bool isCommandTopic(const char* topic) {
    if (topic == nullptr) {
        return false;
    }

    return (strcmp(topic, cmd_led_topic) == 0) || (strcmp(topic, cmd_config_topic) == 0) ||
           (strcmp(topic, cmd_mode_topic) == 0);
}

bool setupSubscriptionsLocked() {
    if (!broker_client.connected()) {
        return false;
    }

    const bool has_led = broker_client.subscribe(cmd_led_topic, MQTT_QOS_COMMAND);
    const bool has_config = broker_client.subscribe(cmd_config_topic, MQTT_QOS_COMMAND);
    const bool has_mode = broker_client.subscribe(cmd_mode_topic, MQTT_QOS_COMMAND);

    return has_led && has_config && has_mode;
}

bool publishAvailabilityOnlineLocked() {
    return publishPayloadLocked(
        availability_topic,
        reinterpret_cast<const uint8_t*>(kAvailabilityOnline),
        strlen(kAvailabilityOnline),
        true
    );
}

bool connectBrokerLocked() {
    const bool was_connected = broker_client.connect(
        client_id_buf, availability_topic, MQTT_QOS_STATUS, true, kAvailabilityOffline
    );

    if (!was_connected) {
        ESP_LOGW(TAG, "broker connect failed state=%d", broker_client.state());
        return false;
    }

    if (!setupSubscriptionsLocked()) {
        ESP_LOGW(TAG, "subscription setup failed");
        broker_client.disconnect();
        return false;
    }

    if (!publishAvailabilityOnlineLocked()) {
        ESP_LOGW(TAG, "availability publish failed");
        broker_client.disconnect();
        return false;
    }

    resetReconnectState();
    ESP_LOGI(TAG, "broker connected");
    return true;
}

void processInboundQueue() {
    if (inbound_queue == nullptr || inbound_callback == nullptr) {
        return;
    }

    InboundMessage msg = {};
    while (xQueueReceive(inbound_queue, &msg, 0) == pdTRUE) {
        inbound_callback(msg.topic, msg.payload, msg.length);
    }
}

void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
    if (inbound_queue == nullptr || inbound_callback == nullptr) {
        return;
    }

    if (!isCommandTopic(topic)) {
        return;
    }

    if (length == 0U || length > COMMAND_DOC_SIZE || length > UINT16_MAX) {
        ESP_LOGW(TAG, "command payload invalid");
        return;
    }

    InboundMessage msg = {};
    if (!copyText(msg.topic, sizeof(msg.topic), topic)) {
        ESP_LOGW(TAG, "topic copy failed");
        return;
    }

    msg.length = static_cast<uint16_t>(length);
    memcpy(msg.payload, payload, msg.length);

    if (xQueueSend(inbound_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "inbound queue full, dropped topic=%s len=%u", msg.topic, msg.length);
    }
}
}  // namespace

namespace MqttClient {
bool init(const char* device_id, Client& net_client, bool (*wifi_connected)()) {
    if (!hasText(device_id)) {
        ESP_LOGE(TAG, "device id missing");
        return false;
    }

    if (wifi_connected == nullptr) {
        ESP_LOGE(TAG, "wifi callback missing");
        return false;
    }

    if (!ensureInfrastructure()) {
        return false;
    }

    if (!copyText(device_id_buf, sizeof(device_id_buf), device_id)) {
        ESP_LOGE(TAG, "device id too long");
        return false;
    }

    if (!buildClientId() || !buildTopics()) {
        ESP_LOGE(TAG, "topic init failed");
        return false;
    }

    network_client = &net_client;
    wifi_connected_fn = wifi_connected;

    LockGuard lock(mqtt_mutex);
    if (!lock.locked()) {
        ESP_LOGE(TAG, "init lock timeout");
        return false;
    }

    if (broker_client.connected()) {
        broker_client.disconnect();
    }

    broker_client.setClient(*network_client);
    broker_client.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT);
    broker_client.setKeepAlive(MQTT_KEEPALIVE_SEC);
    broker_client.setCallback(mqttCallback);

    if (!broker_client.setBufferSize(MQTT_PACKET_BUFFER_SIZE)) {
        ESP_LOGE(TAG, "mqtt buffer size set failed");
        return false;
    }

    clearInboundQueue();
    resetReconnectState();
    is_initialized = true;

    ESP_LOGI(TAG, "mqtt initialized for %s", device_id_buf);
    return true;
}

bool init(const char* device_id) {
    return init(device_id, default_wifi_client, defaultWifiConnected);
}

bool connectOrPoll() {
    if (!is_initialized || wifi_connected_fn == nullptr) {
        ESP_LOGW(TAG, "poll before init");
        return false;
    }

    if (!wifi_connected_fn()) {
        LockGuard lock(mqtt_mutex);
        if (lock.locked() && broker_client.connected()) {
            broker_client.disconnect();
        }
        resetReconnectState();
        processInboundQueue();
        return false;
    }

    {
        LockGuard lock(mqtt_mutex);
        if (!lock.locked()) {
            ESP_LOGW(TAG, "poll lock timeout");
            return false;
        }

        if (broker_client.connected()) {
            broker_client.loop();
        }
    }

    processInboundQueue();

    {
        LockGuard lock(mqtt_mutex);
        if (!lock.locked()) {
            ESP_LOGW(TAG, "state lock timeout");
            return false;
        }

        if (broker_client.connected()) {
            return true;
        }
    }

    const uint64_t now_ms = getNowMs();
    if (now_ms < next_connect_ms) {
        return false;
    }

    {
        LockGuard lock(mqtt_mutex);
        if (!lock.locked()) {
            ESP_LOGW(TAG, "connect lock timeout");
            return false;
        }

        if (broker_client.connected()) {
            return true;
        }

        if (!connectBrokerLocked()) {
            scheduleReconnect(now_ms);
            return false;
        }
    }

    return true;
}

bool publishTelemetry(const SensorReading& reading) {
    uint8_t payload_buf[TELEMETRY_DOC_SIZE] = {};
    size_t encoded_len = 0;

    if (!encodeTelemetry(reading, payload_buf, sizeof(payload_buf), encoded_len)) {
        ESP_LOGW(TAG, "telemetry encode failed");
        return false;
    }

    return publishPayload(telemetry_topic, payload_buf, encoded_len);
}

bool publishStatus(const StatusMessage& status) {
    uint8_t payload_buf[STATUS_DOC_SIZE] = {};
    size_t encoded_len = 0;

    if (!encodeStatus(status, payload_buf, sizeof(payload_buf), encoded_len)) {
        ESP_LOGW(TAG, "status encode failed");
        return false;
    }

    return publishPayload(status_topic, payload_buf, encoded_len);
}

bool publishEnergy(const EnergyMessage& energy) {
    uint8_t payload_buf[ENERGY_DOC_SIZE] = {};
    size_t encoded_len = 0;

    if (!encodeEnergy(energy, payload_buf, sizeof(payload_buf), encoded_len)) {
        ESP_LOGW(TAG, "energy encode failed");
        return false;
    }

    return publishPayload(energy_topic, payload_buf, encoded_len);
}

bool publishAck(const AckMessage& ack) {
    uint8_t payload_buf[ACK_DOC_SIZE] = {};
    char ack_topic[ACK_TOPIC_LEN] = {};
    size_t encoded_len = 0;

    if (!buildAckTopic(ack_topic, sizeof(ack_topic), ack.command_id)) {
        ESP_LOGW(TAG, "ack topic build failed");
        return false;
    }

    if (!encodeAck(ack, payload_buf, sizeof(payload_buf), encoded_len)) {
        ESP_LOGW(TAG, "ack encode failed");
        return false;
    }

    return publishPayload(ack_topic, payload_buf, encoded_len);
}

bool setupSubscriptions() {
    if (!is_initialized) {
        return false;
    }

    LockGuard lock(mqtt_mutex);
    if (!lock.locked()) {
        ESP_LOGW(TAG, "subscribe lock timeout");
        return false;
    }

    return setupSubscriptionsLocked();
}

void registerInboundCallback(MqttInboundCallback callback) { inbound_callback = callback; }

bool isConnected() {
    if (!is_initialized) {
        return false;
    }

    LockGuard lock(mqtt_mutex, pdMS_TO_TICKS(10));
    if (!lock.locked()) {
        return false;
    }

    return broker_client.connected();
}
}  // namespace MqttClient
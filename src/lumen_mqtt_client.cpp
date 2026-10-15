#include "lumen_mqtt_client.h"

#include <Client.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

#include <atomic>

#include "lumen_board_config.h"
#include "lumen_system_utils.h"
#include "lumen_typed_queue.h"
#include "lumen_wifi_manager.h"
#include "lumen_wire.h"

namespace {
static const char* TAG = "mqtt_client";

static constexpr uint32_t MQTT_BACKOFF_BASE_MS = 1000U;
static constexpr uint32_t MQTT_BACKOFF_MAX_MS = 30000U;

// tcp connect plus tls handshake must stay inside the 10 s task watchdog.
static constexpr uint32_t CONNECT_TIMEOUT_SEC = 4U;

static constexpr size_t TOPIC_LEN = 96U;
static constexpr size_t ACK_TOPIC_LEN = 128U;
static constexpr char CLIENT_ID_PREFIX[] = "lumen-fw-";
static constexpr size_t CLIENT_ID_LEN = sizeof(CLIENT_ID_PREFIX) + DEVICE_ID_MAX_LEN;

static constexpr size_t INBOUND_QUEUE_LENGTH = 4U;
static constexpr size_t MQTT_FIXED_HEADER_SLACK = 16U;

static constexpr uint16_t MQTT_PACKET_BUFFER_SIZE =
    static_cast<uint16_t>(ACK_TOPIC_LEN + STATUS_PAYLOAD_MAX + MQTT_FIXED_HEADER_SLACK);

static const char* const kAvailabilityOnline = "online";
static const char* const kAvailabilityOffline = "offline";

struct InboundMessage {
    CommandKind kind;
    uint16_t length;
    uint8_t payload[COMMAND_PAYLOAD_MAX_BYTES];
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

static WiFiClient plain_client;
static WiFiClientSecure secure_client;
static PubSubClient broker_client;
static const CloudConfig* cloud_config = nullptr;

static StaticSemaphore_t mqtt_mutex_storage;
static SemaphoreHandle_t mqtt_mutex = nullptr;

static StaticTypedQueue<InboundMessage, INBOUND_QUEUE_LENGTH> inbound_queue;

static std::atomic<CloudState> cloud_state{CloudState::NOT_CONFIGURED};
static char device_id_buf[DEVICE_ID_MAX_LEN] = {};
static char client_id_buf[CLIENT_ID_LEN] = {};
static char topic_prefix[TOPIC_LEN] = {};
static char telemetry_topic[TOPIC_LEN] = {};
static char status_topic[TOPIC_LEN] = {};
static char energy_topic[TOPIC_LEN] = {};
static char availability_topic[TOPIC_LEN] = {};
static char cmd_led_topic[TOPIC_LEN] = {};
static char cmd_config_topic[TOPIC_LEN] = {};
static char cmd_mode_topic[TOPIC_LEN] = {};

static uint32_t reconnect_backoff_ms = MQTT_BACKOFF_BASE_MS;
static uint64_t next_connect_ms = 0;
static WifiConnectedFn wifi_connected_fn = nullptr;
static InboundCommandFn inbound_callback = nullptr;

bool defaultWifiConnected() { return WifiManager::isConnected(); }

bool isEnabled() {
    const CloudState state = cloud_state.load();
    return state == CloudState::CONNECTING || state == CloudState::CONNECTED;
}

// Static creation with static buffers cannot return NULL (IDF 4.4 freertos/semphr.h:793 and
// freertos/queue.h:202: NULL only when the buffer is NULL), so the handles need no null check.
void ensureInfrastructure() {
    if (mqtt_mutex == nullptr) {
        mqtt_mutex = xSemaphoreCreateMutexStatic(&mqtt_mutex_storage);
    }

    if (!inbound_queue.isCreated()) {
        inbound_queue.create();
    }
}

bool buildTopic(char* out_topic, size_t out_len, const char* suffix) {
    const int written = snprintf(out_topic, out_len, "%s%s", topic_prefix, suffix);
    return (written > 0) && (static_cast<size_t>(written) < out_len);
}

// parseCommandId() guarantees that command_id is one valid topic level.
bool buildAckTopic(char* out_topic, size_t out_len, const CommandId& command_id) {
    const int written = snprintf(
        out_topic, out_len, "%s%s/%s", topic_prefix, MQTT_TOPIC_ACK_PREFIX, command_id.c_str()
    );
    return (written > 0) && (static_cast<size_t>(written) < out_len);
}

bool buildTopics() {
    const int prefix_written =
        snprintf(topic_prefix, sizeof(topic_prefix), "%s/%s/", MQTT_TOPIC_ROOT, device_id_buf);
    const int client_written =
        snprintf(client_id_buf, sizeof(client_id_buf), "%s%s", CLIENT_ID_PREFIX, device_id_buf);
    if (prefix_written <= 0 || static_cast<size_t>(prefix_written) >= sizeof(topic_prefix) ||
        client_written <= 0 || static_cast<size_t>(client_written) >= sizeof(client_id_buf)) {
        return false;
    }

    return buildTopic(telemetry_topic, sizeof(telemetry_topic), MQTT_TOPIC_TELEMETRY_SUFFIX) &&
           buildTopic(status_topic, sizeof(status_topic), MQTT_TOPIC_STATUS_SUFFIX) &&
           buildTopic(energy_topic, sizeof(energy_topic), MQTT_TOPIC_ENERGY_SUFFIX) &&
           buildTopic(
               availability_topic, sizeof(availability_topic), MQTT_TOPIC_AVAILABILITY_SUFFIX
           ) &&
           buildTopic(cmd_led_topic, sizeof(cmd_led_topic), MQTT_TOPIC_CMD_LED_SUFFIX) &&
           buildTopic(cmd_config_topic, sizeof(cmd_config_topic), MQTT_TOPIC_CMD_CONFIG_SUFFIX) &&
           buildTopic(cmd_mode_topic, sizeof(cmd_mode_topic), MQTT_TOPIC_CMD_MODE_SUFFIX);
}

void resetReconnectState() {
    reconnect_backoff_ms = MQTT_BACKOFF_BASE_MS;
    next_connect_ms = 0;
}

void scheduleReconnect(uint64_t now_ms) {
    next_connect_ms = now_ms + backoffWithJitterMs(reconnect_backoff_ms, esp_random());
    reconnect_backoff_ms = nextBackoffMs(reconnect_backoff_ms, MQTT_BACKOFF_MAX_MS);
}

bool publishPayload(const char* topic, const uint8_t* payload, size_t length, bool retained) {
    if (!isEnabled() || length == 0U) {
        return false;
    }

    LockGuard lock(mqtt_mutex);
    if (!lock.locked()) {
        ESP_LOGW(TAG, "publish lock timeout");
        return false;
    }

    if (!broker_client.connected()) {
        return false;
    }

    if (!broker_client.publish(topic, payload, length, retained)) {
        ESP_LOGW(TAG, "publish failed %s", topic);
        return false;
    }

    return true;
}

// retained, so a new dashboard tab shows values at once.
template <typename Message, size_t Capacity>
bool publishRetained(
    const char* topic, const Message& message, size_t (*encode)(const Message&, uint8_t*, size_t)
) {
    uint8_t payload[Capacity] = {};
    const size_t length = encode(message, payload, sizeof(payload));
    if (length == 0U) {
        ESP_LOGE(TAG, "encode failed for %s", topic);
        return false;
    }

    return publishPayload(topic, payload, length, true);
}

struct ConnectWithAuth {
    bool operator()(const X509Auth&) const {
        return broker_client.connect(
            client_id_buf, availability_topic, MQTT_QOS_STATUS, true, kAvailabilityOffline
        );
    }

    bool operator()(const PasswordAuth& auth) const {
        return broker_client.connect(
            client_id_buf,
            auth.username,
            auth.password,
            availability_topic,
            MQTT_QOS_STATUS,
            true,
            kAvailabilityOffline
        );
    }
};

bool connectBrokerLocked() {
    if (!std::visit(ConnectWithAuth{}, cloud_config->auth)) {
        ESP_LOGW(TAG, "broker connect failed state=%d", broker_client.state());
        return false;
    }

    const bool subscribed = broker_client.subscribe(cmd_led_topic, MQTT_QOS_COMMAND) &&
                            broker_client.subscribe(cmd_config_topic, MQTT_QOS_COMMAND) &&
                            broker_client.subscribe(cmd_mode_topic, MQTT_QOS_COMMAND);
    // plain text, not msgpack, to match the retained "offline" will message.
    if (!subscribed || !broker_client.publish(availability_topic, kAvailabilityOnline, true)) {
        ESP_LOGW(TAG, "subscribe or availability publish failed");
        broker_client.disconnect();
        return false;
    }

    resetReconnectState();
    ESP_LOGI(TAG, "broker connected");
    return true;
}

void processInboundQueue() {
    InboundMessage msg = {};
    while (inbound_queue.receive(msg, 0)) {
        if (inbound_callback == nullptr) {
            ESP_LOGE(TAG, "no inbound callback, dropped kind=%u", static_cast<unsigned>(msg.kind));
            continue;
        }

        inbound_callback(CommandSource::CLOUD, msg.kind, msg.payload, msg.length);
    }
}

// runs inside broker_client.loop(); pubsubclient passes a pointer into its own buffer.
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
    const size_t prefix_len = strlen(topic_prefix);
    InboundMessage msg = {};
    if (strncmp(topic, topic_prefix, prefix_len) != 0 ||
        !WireCodec::commandKindForSuffix(topic + prefix_len, msg.kind)) {
        return;
    }

    if (length == 0U || length > sizeof(msg.payload)) {
        ESP_LOGW(TAG, "command payload invalid");
        return;
    }

    msg.length = static_cast<uint16_t>(length);
    memcpy(msg.payload, payload, msg.length);

    if (!inbound_queue.send(msg, 0)) {
        ESP_LOGW(TAG, "inbound queue full, dropped topic=%s len=%u", topic, msg.length);
    }
}

void configureTls(const CloudConfig& config) {
    secure_client.setCACert(config.ca_pem);
    if (const X509Auth* x509 = std::get_if<X509Auth>(&config.auth)) {
        secure_client.setCertificate(x509->cert_pem);
        secure_client.setPrivateKey(x509->key_pem);
    }
    secure_client.setTimeout(CONNECT_TIMEOUT_SEC);
    secure_client.setHandshakeTimeout(CONNECT_TIMEOUT_SEC);
}
}  // namespace

namespace MqttClient {
bool init(
    const char* device_id,
    const CloudConfig& config,
    Client& net_client,
    WifiConnectedFn wifi_connected
) {
    if (device_id == nullptr || device_id[0] == '\0' || wifi_connected == nullptr) {
        ESP_LOGE(TAG, "device id or wifi callback missing");
        return false;
    }

    ensureInfrastructure();

    if (!copyText(device_id_buf, sizeof(device_id_buf), device_id) || !buildTopics()) {
        ESP_LOGE(TAG, "device id too long for topics");
        return false;
    }

    LockGuard lock(mqtt_mutex);
    if (!lock.locked()) {
        ESP_LOGE(TAG, "init lock timeout");
        return false;
    }

    if (broker_client.connected()) {
        broker_client.disconnect();
    }

    cloud_config = &config;
    wifi_connected_fn = wifi_connected;
    broker_client.setClient(net_client);
    broker_client.setServer(config.endpoint, config.port);
    broker_client.setKeepAlive(MQTT_KEEPALIVE_SEC);
    broker_client.setSocketTimeout(CONNECT_TIMEOUT_SEC);
    broker_client.setCallback(mqttCallback);

    if (!broker_client.setBufferSize(MQTT_PACKET_BUFFER_SIZE)) {
        ESP_LOGE(TAG, "mqtt buffer size set failed");
        return false;
    }

    inbound_queue.reset();
    resetReconnectState();
    cloud_state.store(CloudState::CONNECTING);

    ESP_LOGI(TAG, "mqtt initialized for %s at %s:%u", device_id_buf, config.endpoint, config.port);
    return true;
}

bool init(const char* device_id, const CloudConfig& config) {
    if (config.ca_pem[0] == '\0') {
        ESP_LOGW(TAG, "cloud config without tls, use only on a trusted lan");
        return init(device_id, config, plain_client, defaultWifiConnected);
    }

    configureTls(config);
    return init(device_id, config, secure_client, defaultWifiConnected);
}

void disable(CloudState reason) { cloud_state.store(reason); }

CloudState state() { return cloud_state.load(); }

bool connectOrPoll() {
    if (!isEnabled()) {
        return false;
    }

    if (!wifi_connected_fn()) {
        LockGuard lock(mqtt_mutex);
        if (lock.locked() && broker_client.connected()) {
            broker_client.disconnect();
        }
        cloud_state.store(CloudState::CONNECTING);
        resetReconnectState();
        return false;
    }

    bool connected = false;
    {
        LockGuard lock(mqtt_mutex);
        if (!lock.locked()) {
            ESP_LOGW(TAG, "poll lock timeout");
            return false;
        }

        connected = broker_client.connected() && broker_client.loop();
        const uint64_t now_ms = getNowMs();
        if (!connected && now_ms >= next_connect_ms) {
            connected = connectBrokerLocked();
            if (!connected) {
                scheduleReconnect(now_ms);
            }
        }
    }

    processInboundQueue();
    cloud_state.store(connected ? CloudState::CONNECTED : CloudState::CONNECTING);
    return connected;
}

bool publishTelemetry(const SensorReading& reading) {
    return publishRetained<SensorReading, TELEMETRY_PAYLOAD_MAX>(
        telemetry_topic, reading, WireCodec::encodeTelemetry
    );
}

bool publishStatus(const StatusMessage& status) {
    return publishRetained<StatusMessage, STATUS_PAYLOAD_MAX>(
        status_topic, status, WireCodec::encodeStatus
    );
}

bool publishEnergy(const EnergyMessage& energy) {
    return publishRetained<EnergyMessage, ENERGY_PAYLOAD_MAX>(
        energy_topic, energy, WireCodec::encodeEnergy
    );
}

bool publishAck(const AckMessage& ack) {
    uint8_t payload[ACK_PAYLOAD_MAX] = {};
    char ack_topic[ACK_TOPIC_LEN] = {};
    const size_t length = WireCodec::encodeAck(ack, payload, sizeof(payload));
    if (length == 0U || !buildAckTopic(ack_topic, sizeof(ack_topic), ack.command_id)) {
        ESP_LOGW(TAG, "ack encode failed");
        return false;
    }

    return publishPayload(ack_topic, payload, length, false);
}

void registerInboundCallback(InboundCommandFn callback) { inbound_callback = callback; }
}  // namespace MqttClient

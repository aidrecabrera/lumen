#include "lumen_local_transport.h"

#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <FFat.h>
#include <esp_log.h>
#include <stdio.h>

#include <atomic>

#include "lumen_board_config.h"
#include "lumen_command_handler.h"
#include "lumen_typed_queue.h"
#include "lumen_wire.h"

namespace {
static const char* TAG = "local_transport";

static constexpr size_t FRAME_MAX_BYTES = COMMAND_PAYLOAD_MAX_BYTES + 64U;
static constexpr size_t FRAME_QUEUE_LENGTH = 4U;
static constexpr size_t OUTBOUND_FRAME_MAX_BYTES = STATUS_PAYLOAD_MAX + 64U;

struct RawFrame {
    uint16_t length;
    uint8_t bytes[FRAME_MAX_BYTES];
};

static AsyncWebServer server(80);
static AsyncWebSocket socket("/ws");
static AsyncAuthenticationMiddleware auth;
static StaticTypedQueue<RawFrame, FRAME_QUEUE_LENGTH> inbound_frames;
static std::atomic<bool> new_client{false};
static bool is_initialized = false;

bool isWholeBinaryFrame(const AwsFrameInfo& info, size_t length) {
    return info.final && info.index == 0U && info.len == length && info.opcode == WS_BINARY;
}

// runs on the async_tcp task, so it only copies the frame for the network task.
void onSocketEvent(
    AsyncWebSocket*,
    AsyncWebSocketClient*,
    AwsEventType type,
    void* arg,
    uint8_t* data,
    size_t length
) {
    if (type == WS_EVT_CONNECT) {
        new_client.store(true);
        return;
    }

    if (type != WS_EVT_DATA) {
        return;
    }

    const AwsFrameInfo& info = *static_cast<AwsFrameInfo*>(arg);
    if (!isWholeBinaryFrame(info, length) || length > FRAME_MAX_BYTES) {
        ESP_LOGW(TAG, "dropped frame: not one binary frame of at most %u bytes", FRAME_MAX_BYTES);
        return;
    }

    RawFrame frame = {};
    frame.length = static_cast<uint16_t>(length);
    memcpy(frame.bytes, data, length);
    if (!inbound_frames.send(frame, 0)) {
        ESP_LOGW(TAG, "inbound frame queue full");
    }
}

template <typename Message, size_t Capacity>
void publishFrame(
    const char* suffix, const Message& message, size_t (*encode)(const Message&, uint8_t*, size_t)
) {
    if (!is_initialized || socket.count() == 0U) {
        return;
    }

    uint8_t payload[Capacity] = {};
    uint8_t frame[OUTBOUND_FRAME_MAX_BYTES] = {};
    const size_t payload_len = encode(message, payload, sizeof(payload));
    const size_t frame_len =
        WireCodec::encodeFrame(suffix, payload, payload_len, frame, sizeof(frame));
    if (payload_len == 0U || frame_len == 0U) {
        ESP_LOGE(TAG, "frame encode failed for %s", suffix);
        return;
    }

    socket.binaryAll(frame, frame_len);
}
}  // namespace

namespace LocalTransport {
bool init(const char* hostname, const LocalPasswordHash& password_hash) {
    inbound_frames.create();

    if (!MDNS.begin(hostname)) {
        ESP_LOGW(TAG, "mdns start failed, use the ip address");
    } else {
        MDNS.addService("http", "tcp", 80);
    }

    auth.setUsername(LOCAL_AUTH_USERNAME);
    auth.setRealm(LOCAL_AUTH_REALM);
    auth.setPasswordHash(password_hash.c_str());
    auth.setAuthType(AsyncAuthType::AUTH_DIGEST);

    socket.onEvent(onSocketEvent);
    socket.addMiddleware(&auth);
    server.addHandler(&socket);
    server.serveStatic("/", FFat, "/www/").setDefaultFile("index.html").addMiddleware(&auth);
    server.begin();

    is_initialized = true;
    ESP_LOGI(TAG, "local dashboard at http://%s.local", hostname);
    return true;
}

void poll(InboundCommandFn on_command, RejectUnknownKindFn on_unknown_kind) {
    if (!is_initialized) {
        return;
    }

    socket.cleanupClients(LOCAL_MAX_CLIENTS);

    RawFrame raw = {};
    WireCodec::InboundFrame frame = {};
    while (inbound_frames.receive(raw, 0)) {
        switch (WireCodec::decodeFrame(raw.bytes, raw.length, frame)) {
            case WireCodec::FrameStatus::COMMAND:
                on_command(CommandSource::LOCAL, frame.kind, frame.payload, frame.length);
                break;
            case WireCodec::FrameStatus::UNKNOWN_SUFFIX: {
                CommandId command_id;
                if (CommandHandler::readCommandId(frame.payload, frame.length, command_id)) {
                    on_unknown_kind(command_id);
                }
                break;
            }
            case WireCodec::FrameStatus::MALFORMED:
            case WireCodec::FrameStatus::OVERSIZED:
                ESP_LOGW(TAG, "dropped malformed or oversized frame");
                break;
        }
    }
}

bool takeNewClient() { return new_client.exchange(false); }

void publishTelemetry(const SensorReading& reading) {
    publishFrame<SensorReading, TELEMETRY_PAYLOAD_MAX>(
        MQTT_TOPIC_TELEMETRY_SUFFIX, reading, WireCodec::encodeTelemetry
    );
}

void publishStatus(const StatusMessage& status) {
    publishFrame<StatusMessage, STATUS_PAYLOAD_MAX>(
        MQTT_TOPIC_STATUS_SUFFIX, status, WireCodec::encodeStatus
    );
}

void publishEnergy(const EnergyMessage& energy) {
    publishFrame<EnergyMessage, ENERGY_PAYLOAD_MAX>(
        MQTT_TOPIC_ENERGY_SUFFIX, energy, WireCodec::encodeEnergy
    );
}

void publishAck(const AckMessage& ack) {
    char suffix[COMMAND_ID_MAX_LEN + 8] = {};
    snprintf(suffix, sizeof(suffix), "%s/%s", MQTT_TOPIC_ACK_PREFIX, ack.command_id.c_str());
    publishFrame<AckMessage, ACK_PAYLOAD_MAX>(suffix, ack, WireCodec::encodeAck);
}
}  // namespace LocalTransport

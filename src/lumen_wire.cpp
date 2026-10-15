#include "lumen_wire.h"

#include <ArduinoJson.h>
#include <string.h>

#include "lumen_board_config.h"

namespace {
size_t serializeInto(const JsonDocument& doc, uint8_t* out, size_t capacity) {
    if (doc.overflowed() || measureMsgPack(doc) > capacity) {
        return 0U;
    }

    return serializeMsgPack(doc, out, capacity);
}

void fillLed(JsonObject led_obj, const LedState& led) {
    led_obj["power"] = led.power;
    led_obj["brightness_pct"] = led.brightness_pct;
    led_obj["red_enabled"] = led.red_enabled;
    led_obj["blue_enabled"] = led.blue_enabled;
    led_obj["far_red_enabled"] = led.far_red_enabled;
    led_obj["red_dist_pct"] = led.red_dist_pct;
    led_obj["blue_dist_pct"] = led.blue_dist_pct;
    led_obj["far_red_dist_pct"] = led.far_red_dist_pct;
}

void fillThresholds(JsonObject thresholds_obj, const ThresholdConfig& thresholds) {
    thresholds_obj["temp_min_c"] = thresholds.temp_min_c;
    thresholds_obj["temp_max_c"] = thresholds.temp_max_c;
    thresholds_obj["humidity_min_pct"] = thresholds.humidity_min_pct;
    thresholds_obj["humidity_max_pct"] = thresholds.humidity_max_pct;
}

struct SuffixKind {
    const char* suffix;
    CommandKind kind;
};

constexpr SuffixKind COMMAND_SUFFIXES[] = {
    {MQTT_TOPIC_CMD_LED_SUFFIX, CommandKind::LED_CONTROL},
    {MQTT_TOPIC_CMD_CONFIG_SUFFIX, CommandKind::CONFIG_UPDATE},
    {MQTT_TOPIC_CMD_MODE_SUFFIX, CommandKind::MODE_CHANGE},
};
}  // namespace

namespace WireCodec {
size_t encodeTelemetry(const SensorReading& reading, uint8_t* out, size_t capacity) {
    JsonDocument doc;
    doc["timestamp_ms"] = reading.timestamp_ms;
    doc["light_lux"] = reading.light_lux;
    doc["temperature_c"] = reading.temperature_c;
    doc["humidity_pct"] = reading.humidity_pct;
    doc["sequence"] = reading.sequence;
    return serializeInto(doc, out, capacity);
}

size_t encodeStatus(const StatusMessage& status, uint8_t* out, size_t capacity) {
    JsonDocument doc;
    doc["timestamp_ms"] = status.timestamp_ms;
    doc["mode"] = static_cast<uint8_t>(status.mode);
    doc["degraded_tier"] = static_cast<uint8_t>(status.degraded_tier);
    doc["uptime_sec"] = status.uptime_sec;
    fillLed(doc["led"].to<JsonObject>(), status.led);
    doc["auto_state"] = autoStateText(status.auto_state);
    doc["cloud"] = cloudStateText(status.cloud);
    fillThresholds(doc["thresholds"].to<JsonObject>(), status.thresholds);
    doc["lux_ceiling"] = status.lux_ceiling;
    return serializeInto(doc, out, capacity);
}

size_t encodeEnergy(const EnergyMessage& energy, uint8_t* out, size_t capacity) {
    JsonDocument doc;
    doc["timestamp_ms"] = energy.timestamp_ms;
    doc["total_wh"] = energy.total_wh;
    doc["session_wh"] = energy.session_wh;
    doc["light_on_sec"] = energy.light_on_sec;
    doc["max_brightness_sec"] = energy.max_brightness_sec;
    return serializeInto(doc, out, capacity);
}

size_t encodeAck(const AckMessage& ack, uint8_t* out, size_t capacity) {
    JsonDocument doc;
    doc["command_id"] = ack.command_id.c_str();
    doc["result"] = static_cast<uint8_t>(ack.result);
    doc["timestamp_ms"] = ack.timestamp_ms;
    doc["mode"] = static_cast<uint8_t>(ack.mode);
    doc["reason"] = ackReasonText(ack.reason);
    fillLed(doc["led"].to<JsonObject>(), ack.led);
    return serializeInto(doc, out, capacity);
}

size_t encodeFrame(
    const char* suffix, const uint8_t* payload, size_t length, uint8_t* out, size_t capacity
) {
    JsonDocument doc;
    doc["t"] = suffix;
    doc["p"] = MsgPackBinary(payload, length);
    return serializeInto(doc, out, capacity);
}

FrameStatus decodeFrame(const uint8_t* bytes, size_t length, InboundFrame& out_frame) {
    JsonDocument doc;
    if (deserializeMsgPack(doc, bytes, length)) {
        return FrameStatus::MALFORMED;
    }

    const char* suffix = doc["t"];
    const MsgPackBinary payload = doc["p"].as<MsgPackBinary>();
    if (suffix == nullptr || payload.data() == nullptr || payload.size() == 0U) {
        return FrameStatus::MALFORMED;
    }

    if (payload.size() > sizeof(out_frame.payload)) {
        return FrameStatus::OVERSIZED;
    }

    out_frame.length = static_cast<uint16_t>(payload.size());
    memcpy(out_frame.payload, payload.data(), payload.size());
    return commandKindForSuffix(suffix, out_frame.kind) ? FrameStatus::COMMAND
                                                        : FrameStatus::UNKNOWN_SUFFIX;
}

bool commandKindForSuffix(const char* suffix, CommandKind& out_kind) {
    for (const SuffixKind& entry : COMMAND_SUFFIXES) {
        if (strcmp(suffix, entry.suffix) == 0) {
            out_kind = entry.kind;
            return true;
        }
    }

    return false;
}
}  // namespace WireCodec

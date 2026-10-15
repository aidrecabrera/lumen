#include "lumen_command_handler.h"

#include <ArduinoJson.h>
#include <esp_log.h>
#include <math.h>

#include "lumen_type_validation.h"

namespace {
static const char* TAG = "command_handler";

bool readBoolField(JsonVariantConst value, bool& out_value) {
    if (!value.is<bool>()) {
        return false;
    }

    out_value = value.as<bool>();
    return true;
}

bool readU8Field(JsonVariantConst value, uint8_t& out_value) {
    if (!value.is<uint32_t>()) {
        return false;
    }

    const uint32_t raw_value = value.as<uint32_t>();
    if (raw_value > UINT8_MAX) {
        ESP_LOGW(TAG, "u8 field out of range: %lu", static_cast<unsigned long>(raw_value));
        return false;
    }

    out_value = static_cast<uint8_t>(raw_value);
    return true;
}

bool readPercentField(JsonVariantConst value, uint8_t& out_value) {
    if (!readU8Field(value, out_value)) {
        return false;
    }

    if (out_value > 100U) {
        ESP_LOGW(TAG, "percent field out of range: %u", out_value);
        return false;
    }

    return true;
}

bool readFloatField(JsonVariantConst value, float& out_value) {
    // is<float>() accepts integer values too (arduinojson NumberBit check).
    if (!value.is<float>()) {
        return false;
    }

    const float raw_value = value.as<float>();
    if (!isfinite(raw_value)) {
        ESP_LOGW(TAG, "float field not finite");
        return false;
    }

    out_value = raw_value;
    return true;
}

bool readModeField(JsonVariantConst value, DeviceMode& out_mode) {
    uint8_t raw_mode = 0;
    if (!readU8Field(value, raw_mode)) {
        return false;
    }

    if (!parseDeviceMode(raw_mode, out_mode)) {
        ESP_LOGW(TAG, "mode invalid: %u", raw_mode);
        return false;
    }

    return true;
}

// Deserializes the frame and parses "command_id". This is the only place that turns an
// untrusted id into a CommandId. A bad id cannot form an ack topic, so the caller cannot ack it.
bool decodeBaseDocument(
    const uint8_t* payload, uint16_t length, JsonDocument& doc, CommandId& out_command_id
) {
    // CommandHandler::decode() already rejected a null or empty payload.
    const DeserializationError error = deserializeMsgPack(doc, payload, length);
    if (error) {
        ESP_LOGW(TAG, "decode failed: %s", error.c_str());
        return false;
    }

    const JsonVariantConst command_id_value = doc["command_id"];
    if (!command_id_value.is<const char*>()) {
        ESP_LOGW(TAG, "command id missing");
        return false;
    }

    // JsonString keeps the MsgPack byte count, so parseCommandId() sees an embedded NUL.
    const JsonString command_id = command_id_value.as<JsonString>();
    if (!parseCommandId(command_id.c_str(), command_id.size(), out_command_id)) {
        ESP_LOGW(TAG, "command id invalid (length %u)", static_cast<unsigned>(command_id.size()));
        return false;
    }

    return true;
}

using CommandHandler::DecodeStatus;

DecodeStatus decodeLedPayload(
    const uint8_t* payload, uint16_t length, CommandEnvelope& out_command
) {
    JsonDocument doc;
    out_command = CommandEnvelope{};

    if (!decodeBaseDocument(payload, length, doc, out_command.command_id)) {
        return DecodeStatus::UNADDRESSABLE;
    }

    LedState led = {};
    const bool has_power = readBoolField(doc["power"], led.power);
    const bool has_red_enabled = readBoolField(doc["red_enabled"], led.red_enabled);
    const bool has_blue_enabled = readBoolField(doc["blue_enabled"], led.blue_enabled);
    const bool has_far_red_enabled = readBoolField(doc["far_red_enabled"], led.far_red_enabled);
    const bool has_brightness = readPercentField(doc["brightness_pct"], led.brightness_pct);
    const bool has_red_pct = readPercentField(doc["red_dist_pct"], led.red_dist_pct);
    const bool has_blue_pct = readPercentField(doc["blue_dist_pct"], led.blue_dist_pct);
    const bool has_far_red_pct = readPercentField(doc["far_red_dist_pct"], led.far_red_dist_pct);

    if (!has_power || !has_red_enabled || !has_blue_enabled || !has_far_red_enabled ||
        !has_brightness || !has_red_pct || !has_blue_pct || !has_far_red_pct) {
        ESP_LOGW(TAG, "led payload incomplete");
        return DecodeStatus::INVALID_FIELDS;
    }

    const std::optional<ValidLedState> valid_led = ValidLedState::parse(led);
    if (!valid_led) {
        ESP_LOGW(TAG, "led payload invalid");
        return DecodeStatus::INVALID_FIELDS;
    }

    out_command.payload = LedCommand{*valid_led};
    return DecodeStatus::DECODED;
}

DecodeStatus decodeConfigPayload(
    const uint8_t* payload, uint16_t length, CommandEnvelope& out_command
) {
    JsonDocument doc;
    out_command = CommandEnvelope{};

    if (!decodeBaseDocument(payload, length, doc, out_command.command_id)) {
        return DecodeStatus::UNADDRESSABLE;
    }

    const JsonVariantConst thresholds_value = doc["thresholds"];
    if (!thresholds_value.is<JsonObjectConst>()) {
        ESP_LOGW(TAG, "thresholds missing");
        return DecodeStatus::INVALID_FIELDS;
    }

    ThresholdConfig thresholds = {};
    const bool has_temp_min = readFloatField(thresholds_value["temp_min_c"], thresholds.temp_min_c);
    const bool has_temp_max = readFloatField(thresholds_value["temp_max_c"], thresholds.temp_max_c);
    const bool has_humidity_min =
        readFloatField(thresholds_value["humidity_min_pct"], thresholds.humidity_min_pct);
    const bool has_humidity_max =
        readFloatField(thresholds_value["humidity_max_pct"], thresholds.humidity_max_pct);

    if (!has_temp_min || !has_temp_max || !has_humidity_min || !has_humidity_max) {
        ESP_LOGW(TAG, "threshold payload incomplete");
        return DecodeStatus::INVALID_FIELDS;
    }

    const std::optional<ValidThresholds> valid_thresholds = ValidThresholds::parse(thresholds);
    if (!valid_thresholds) {
        ESP_LOGW(TAG, "threshold payload invalid");
        return DecodeStatus::INVALID_FIELDS;
    }

    ConfigCommand command = {*valid_thresholds, std::nullopt};
    const JsonVariantConst lux_ceiling_value = doc["lux_ceiling"];
    if (!lux_ceiling_value.isNull()) {
        float lux_ceiling = 0.0f;
        if (readFloatField(lux_ceiling_value, lux_ceiling)) {
            command.desired_lux_ceiling = ValidLuxCeiling::parse(lux_ceiling);
        }

        if (!command.desired_lux_ceiling) {
            ESP_LOGW(TAG, "lux ceiling invalid");
            return DecodeStatus::INVALID_FIELDS;
        }
    }

    out_command.payload = command;
    return DecodeStatus::DECODED;
}

DecodeStatus decodeModePayload(
    const uint8_t* payload, uint16_t length, CommandEnvelope& out_command
) {
    JsonDocument doc;
    out_command = CommandEnvelope{};

    if (!decodeBaseDocument(payload, length, doc, out_command.command_id)) {
        return DecodeStatus::UNADDRESSABLE;
    }

    DeviceMode mode = DeviceMode::AUTONOMOUS;
    if (!readModeField(doc["mode"], mode)) {
        return DecodeStatus::INVALID_FIELDS;
    }

    out_command.payload = ModeCommand{mode};
    return DecodeStatus::DECODED;
}
}  // namespace

namespace CommandHandler {
bool readCommandId(const uint8_t* payload, uint16_t length, CommandId& out_command_id) {
    JsonDocument doc;
    return (payload != nullptr) && (length > 0U) &&
           decodeBaseDocument(payload, length, doc, out_command_id);
}

DecodeStatus decode(
    CommandKind kind, const uint8_t* payload, uint16_t length, CommandEnvelope& out_command
) {
    if (payload == nullptr || length == 0U) {
        ESP_LOGW(TAG, "invalid inbound command");
        return DecodeStatus::UNADDRESSABLE;
    }

    switch (kind) {
        case CommandKind::LED_CONTROL:
            return decodeLedPayload(payload, length, out_command);
        case CommandKind::CONFIG_UPDATE:
            return decodeConfigPayload(payload, length, out_command);
        case CommandKind::MODE_CHANGE:
            return decodeModePayload(payload, length, out_command);
    }

    // Only a value outside the enum reaches this line. The switch has no default, so -Wswitch
    // flags a new CommandKind.
    ESP_LOGE(TAG, "unknown command kind: %u", static_cast<unsigned>(kind));
    return DecodeStatus::UNADDRESSABLE;
}
}  // namespace CommandHandler

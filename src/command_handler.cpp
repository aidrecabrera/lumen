#include "command_handler.h"

#include <ArduinoJson.h>
#include <esp_log.h>
#include <math.h>
#include <string.h>

#include "config.h"
#include "task_manager.h"
#include "utils.h"
#include "validation.h"

namespace {
static const char* TAG = "command_handler";

static constexpr size_t COMMAND_ID_DOC_SLACK = COMMAND_ID_MAX_LEN + 32U;
static constexpr size_t LED_COMMAND_DOC_SIZE = JSON_OBJECT_SIZE(9) + COMMAND_ID_DOC_SLACK;
static constexpr size_t CONFIG_COMMAND_DOC_SIZE =
    JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + COMMAND_ID_DOC_SLACK;
static constexpr size_t MODE_COMMAND_DOC_SIZE = JSON_OBJECT_SIZE(2) + COMMAND_ID_DOC_SLACK;

bool readBoolField(JsonVariantConst value, bool& out_value) {
    if (value.isNull()) {
        return false;
    }

    out_value = value.as<bool>();
    return true;
}

bool readU8Field(JsonVariantConst value, uint8_t& out_value) {
    if (value.isNull()) {
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
    if (value.isNull()) {
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

    if (raw_mode == static_cast<uint8_t>(DeviceMode::AUTONOMOUS)) {
        out_mode = DeviceMode::AUTONOMOUS;
        return true;
    }

    if (raw_mode == static_cast<uint8_t>(DeviceMode::MANUAL)) {
        out_mode = DeviceMode::MANUAL;
        return true;
    }

    ESP_LOGW(TAG, "mode invalid: %u", raw_mode);
    return false;
}

template <size_t DocSize>
bool decodeBaseDocument(
    const uint8_t* payload,
    uint16_t length,
    StaticJsonDocument<DocSize>& doc,
    char* out_command_id,
    size_t out_command_id_len
) {
    if (payload == nullptr || length == 0U || out_command_id == nullptr || out_command_id_len == 0U) {
        ESP_LOGW(TAG, "invalid command frame");
        return false;
    }

    const DeserializationError error = deserializeMsgPack(doc, payload, length);
    if (error) {
        ESP_LOGW(TAG, "decode failed: %s", error.c_str());
        return false;
    }

    const char* command_id = doc["command_id"] | nullptr;
    if (command_id == nullptr || command_id[0] == '\0') {
        ESP_LOGW(TAG, "command id missing");
        return false;
    }

    if (!copyText(out_command_id, out_command_id_len, command_id)) {
        ESP_LOGW(TAG, "command id too long");
        return false;
    }

    return true;
}

void setEnvelopeBase(CommandEnvelope& out_command, const char* command_id, CommandKind kind) {
    memset(&out_command, 0, sizeof(out_command));
    copyText(out_command.command_id, sizeof(out_command.command_id), command_id);
    out_command.kind = kind;
    out_command.received_timestamp_ms = getNowMs();
}

bool decodeLedPayload(const uint8_t* payload, uint16_t length, CommandEnvelope& out_command) {
    StaticJsonDocument<LED_COMMAND_DOC_SIZE> doc;
    char command_id[COMMAND_ID_MAX_LEN] = {};

    if (!decodeBaseDocument(payload, length, doc, command_id, sizeof(command_id))) {
        return false;
    }

    setEnvelopeBase(out_command, command_id, CommandKind::LED_CONTROL);

    const bool has_power = readBoolField(doc["power"], out_command.desired_led.power);
    const bool has_red_enabled =
        readBoolField(doc["red_enabled"], out_command.desired_led.red_enabled);
    const bool has_blue_enabled =
        readBoolField(doc["blue_enabled"], out_command.desired_led.blue_enabled);
    const bool has_far_red_enabled =
        readBoolField(doc["far_red_enabled"], out_command.desired_led.far_red_enabled);
    const bool has_brightness =
        readPercentField(doc["brightness_pct"], out_command.desired_led.brightness_pct);
    const bool has_red_pct =
        readPercentField(doc["red_dist_pct"], out_command.desired_led.red_dist_pct);
    const bool has_blue_pct =
        readPercentField(doc["blue_dist_pct"], out_command.desired_led.blue_dist_pct);
    const bool has_far_red_pct =
        readPercentField(doc["far_red_dist_pct"], out_command.desired_led.far_red_dist_pct);

    if (!has_power || !has_red_enabled || !has_blue_enabled || !has_far_red_enabled ||
        !has_brightness || !has_red_pct || !has_blue_pct || !has_far_red_pct) {
        ESP_LOGW(TAG, "led payload incomplete");
        return false;
    }

    if (!isLedStateValid(out_command.desired_led)) {
        ESP_LOGW(TAG, "led payload invalid");
        return false;
    }

    return true;
}

bool decodeConfigPayload(const uint8_t* payload, uint16_t length, CommandEnvelope& out_command) {
    StaticJsonDocument<CONFIG_COMMAND_DOC_SIZE> doc;
    char command_id[COMMAND_ID_MAX_LEN] = {};

    if (!decodeBaseDocument(payload, length, doc, command_id, sizeof(command_id))) {
        return false;
    }

    setEnvelopeBase(out_command, command_id, CommandKind::CONFIG_UPDATE);

    const JsonVariantConst thresholds = doc["thresholds"];
    if (thresholds.isNull()) {
        ESP_LOGW(TAG, "thresholds missing");
        return false;
    }

    const bool has_temp_min =
        readFloatField(thresholds["temp_min_c"], out_command.desired_thresholds.temp_min_c);
    const bool has_temp_max =
        readFloatField(thresholds["temp_max_c"], out_command.desired_thresholds.temp_max_c);
    const bool has_humidity_min = readFloatField(
        thresholds["humidity_min_pct"], out_command.desired_thresholds.humidity_min_pct
    );
    const bool has_humidity_max = readFloatField(
        thresholds["humidity_max_pct"], out_command.desired_thresholds.humidity_max_pct
    );

    if (!has_temp_min || !has_temp_max || !has_humidity_min || !has_humidity_max) {
        ESP_LOGW(TAG, "threshold payload incomplete");
        return false;
    }

    if (!isThresholdConfigValid(out_command.desired_thresholds)) {
        ESP_LOGW(TAG, "threshold payload invalid");
        return false;
    }

    return true;
}

bool decodeModePayload(const uint8_t* payload, uint16_t length, CommandEnvelope& out_command) {
    StaticJsonDocument<MODE_COMMAND_DOC_SIZE> doc;
    char command_id[COMMAND_ID_MAX_LEN] = {};

    if (!decodeBaseDocument(payload, length, doc, command_id, sizeof(command_id))) {
        return false;
    }

    setEnvelopeBase(out_command, command_id, CommandKind::MODE_CHANGE);

    return readModeField(doc["mode"], out_command.desired_mode);
}

bool enqueueCommand(const CommandEnvelope& command) {
    if (!TaskManager::dispatchCommand(command)) {
        ESP_LOGW(TAG, "command dispatch failed");
        return false;
    }

    return true;
}
}  // namespace

namespace CommandHandler {
bool init() { return true; }

bool handleInbound(const char* topic, const uint8_t* payload, uint16_t length) {
    if (topic == nullptr || payload == nullptr || length == 0U) {
        ESP_LOGW(TAG, "invalid inbound command");
        return false;
    }

    CommandEnvelope command = {};
    bool was_decoded = false;

    if (endsWith(topic, MQTT_TOPIC_CMD_LED_SUFFIX)) {
        was_decoded = decodeLedPayload(payload, length, command);
    } else if (endsWith(topic, MQTT_TOPIC_CMD_CONFIG_SUFFIX)) {
        was_decoded = decodeConfigPayload(payload, length, command);
    } else if (endsWith(topic, MQTT_TOPIC_CMD_MODE_SUFFIX)) {
        was_decoded = decodeModePayload(payload, length, command);
    } else {
        ESP_LOGW(TAG, "unknown command topic: %s", topic);
        return false;
    }

    if (!was_decoded) {
        return false;
    }

    return enqueueCommand(command);
}
}  // namespace CommandHandler
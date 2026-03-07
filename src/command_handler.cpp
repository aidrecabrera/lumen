#include "command_handler.h"

#include <string.h>

#include <ArduinoJson.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "config.h"
#include "task_manager.h"

namespace {
static const char* TAG = "command_handler";


uint64_t getNowMs()
{
    return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
}


bool endsWith(const char* text, const char* suffix)
{
    if (text == nullptr || suffix == nullptr) {
        return false;
    }

    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);

    if (text_len < suffix_len) {
        return false;
    }

    return strcmp(text + text_len - suffix_len, suffix) == 0;
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


bool readBoolField(
    JsonVariantConst value,
    bool& out_value)
{
    if (value.isNull()) {
        return false;
    }

    out_value = value.as<bool>();
    return true;
}


bool readU8Field(
    JsonVariantConst value,
    uint8_t& out_value)
{
    if (value.isNull()) {
        return false;
    }

    out_value = value.as<uint8_t>();
    return true;
}


bool decodeLedPayload(
    const uint8_t* payload,
    uint16_t length,
    CommandEnvelope& out_command)
{
    StaticJsonDocument<COMMAND_DOC_SIZE> doc;
    DeserializationError error = deserializeMsgPack(doc, payload, length);

    if (error) {
        ESP_LOGW(TAG, "led decode failed");
        return false;
    }

    const char* command_id = doc["command_id"];
    if (command_id == nullptr || command_id[0] == '\0') {
        ESP_LOGW(TAG, "command id missing");
        return false;
    }

    bool has_power = readBoolField(
        doc["power"],
        out_command.desired_led.power);
    bool has_red_enabled = readBoolField(
        doc["red_enabled"],
        out_command.desired_led.red_enabled);
    bool has_blue_enabled = readBoolField(
        doc["blue_enabled"],
        out_command.desired_led.blue_enabled);
    bool has_far_red_enabled = readBoolField(
        doc["far_red_enabled"],
        out_command.desired_led.far_red_enabled);
    bool has_brightness = readU8Field(
        doc["brightness_pct"],
        out_command.desired_led.brightness_pct);
    bool has_red_pct = readU8Field(
        doc["red_dist_pct"],
        out_command.desired_led.red_dist_pct);
    bool has_blue_pct = readU8Field(
        doc["blue_dist_pct"],
        out_command.desired_led.blue_dist_pct);
    bool has_far_red_pct = readU8Field(
        doc["far_red_dist_pct"],
        out_command.desired_led.far_red_dist_pct);

    if (!has_power ||
        !has_red_enabled ||
        !has_blue_enabled ||
        !has_far_red_enabled ||
        !has_brightness ||
        !has_red_pct ||
        !has_blue_pct ||
        !has_far_red_pct) {
        ESP_LOGW(TAG, "led payload incomplete");
        return false;
    }

    memset(&out_command, 0, sizeof(out_command));
    copyText(
        out_command.command_id,
        sizeof(out_command.command_id),
        command_id);
    out_command.kind = CommandKind::LED_CONTROL;
    out_command.received_timestamp_ms = getNowMs();
    return true;
}
}  // namespace


namespace CommandHandler {
bool init()
{
    if (command_dispatch_queue == nullptr) {
        ESP_LOGW(TAG, "command queue missing");
        return false;
    }

    return true;
}


bool handleInbound(
    const char* topic,
    const uint8_t* payload,
    uint16_t length)
{
    if (command_dispatch_queue == nullptr) {
        ESP_LOGW(TAG, "command queue missing");
        return false;
    }

    if (!endsWith(topic, MQTT_TOPIC_CMD_LED_SUFFIX)) {
        return false;
    }

    CommandEnvelope command = {};
    bool was_decoded = decodeLedPayload(payload, length, command);

    if (!was_decoded) {
        return false;
    }

    BaseType_t send_result = xQueueSend(
        command_dispatch_queue,
        &command,
        0);

    if (send_result != pdTRUE) {
        ESP_LOGW(TAG, "command queue full");
        return false;
    }

    return true;
}
}  // namespace CommandHandler

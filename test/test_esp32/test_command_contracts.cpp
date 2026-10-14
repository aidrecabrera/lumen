#include <limits>
#include <variant>
#include <vector>
#include <unity.h>
#include <cstring>

#include "lumen_app_types.h"
#include "lumen_command_handler.h"
#include "lumen_config_manager.h"

namespace {

static bool s_prepared = false;

void packMap(std::vector<uint8_t>& out, uint8_t size) {
    out.push_back(static_cast<uint8_t>(0x80U | (size & 0x0FU)));
}

void packStr(std::vector<uint8_t>& out, const char* text) {
    const size_t len = std::strlen(text);
    TEST_ASSERT_TRUE(len < 32U);
    out.push_back(static_cast<uint8_t>(0xA0U | static_cast<uint8_t>(len)));
    out.insert(out.end(), text, text + len);
}

void packBool(std::vector<uint8_t>& out, bool value) {
    out.push_back(value ? 0xC3U : 0xC2U);
}

void packUInt(std::vector<uint8_t>& out, uint8_t value) {
    TEST_ASSERT_TRUE(value < 128U);
    out.push_back(value);
}

void packFloat32(std::vector<uint8_t>& out, float value) {
    out.push_back(0xCAU);
    uint32_t bits = 0U;
    static_assert(sizeof(bits) == sizeof(value), "float32 size mismatch");
    std::memcpy(&bits, &value, sizeof(bits));
    out.push_back(static_cast<uint8_t>((bits >> 24) & 0xFFU));
    out.push_back(static_cast<uint8_t>((bits >> 16) & 0xFFU));
    out.push_back(static_cast<uint8_t>((bits >> 8) & 0xFFU));
    out.push_back(static_cast<uint8_t>(bits & 0xFFU));
}

std::vector<uint8_t> makeValidLedPayload() {
    std::vector<uint8_t> out;
    packMap(out, 9);
    packStr(out, "command_id");
    packStr(out, "cmd-led-1");
    packStr(out, "power");
    packBool(out, true);
    packStr(out, "brightness_pct");
    packUInt(out, 55);
    packStr(out, "red_enabled");
    packBool(out, true);
    packStr(out, "blue_enabled");
    packBool(out, true);
    packStr(out, "far_red_enabled");
    packBool(out, true);
    packStr(out, "red_dist_pct");
    packUInt(out, 34);
    packStr(out, "blue_dist_pct");
    packUInt(out, 33);
    packStr(out, "far_red_dist_pct");
    packUInt(out, 33);
    return out;
}

std::vector<uint8_t> makeLedPayloadMissingCommandId() {
    std::vector<uint8_t> out;
    packMap(out, 8);
    packStr(out, "power");
    packBool(out, true);
    packStr(out, "brightness_pct");
    packUInt(out, 55);
    packStr(out, "red_enabled");
    packBool(out, true);
    packStr(out, "blue_enabled");
    packBool(out, true);
    packStr(out, "far_red_enabled");
    packBool(out, true);
    packStr(out, "red_dist_pct");
    packUInt(out, 34);
    packStr(out, "blue_dist_pct");
    packUInt(out, 33);
    packStr(out, "far_red_dist_pct");
    packUInt(out, 33);
    return out;
}

std::vector<uint8_t> makeLedPayloadBrightness101() {
    std::vector<uint8_t> out = makeValidLedPayload();
    out.clear();
    packMap(out, 9);
    packStr(out, "command_id");
    packStr(out, "cmd-led-2");
    packStr(out, "power");
    packBool(out, true);
    packStr(out, "brightness_pct");
    packUInt(out, 101);
    packStr(out, "red_enabled");
    packBool(out, true);
    packStr(out, "blue_enabled");
    packBool(out, true);
    packStr(out, "far_red_enabled");
    packBool(out, true);
    packStr(out, "red_dist_pct");
    packUInt(out, 34);
    packStr(out, "blue_dist_pct");
    packUInt(out, 33);
    packStr(out, "far_red_dist_pct");
    packUInt(out, 33);
    return out;
}

std::vector<uint8_t> makeLedPayloadSum99() {
    std::vector<uint8_t> out;
    packMap(out, 9);
    packStr(out, "command_id");
    packStr(out, "cmd-led-3");
    packStr(out, "power");
    packBool(out, true);
    packStr(out, "brightness_pct");
    packUInt(out, 50);
    packStr(out, "red_enabled");
    packBool(out, true);
    packStr(out, "blue_enabled");
    packBool(out, true);
    packStr(out, "far_red_enabled");
    packBool(out, true);
    packStr(out, "red_dist_pct");
    packUInt(out, 33);
    packStr(out, "blue_dist_pct");
    packUInt(out, 33);
    packStr(out, "far_red_dist_pct");
    packUInt(out, 33);
    return out;
}

std::vector<uint8_t> makeLedPayloadIncomplete() {
    std::vector<uint8_t> out;
    packMap(out, 8);
    packStr(out, "command_id");
    packStr(out, "cmd-led-4");
    packStr(out, "power");
    packBool(out, true);
    packStr(out, "brightness_pct");
    packUInt(out, 50);
    packStr(out, "red_enabled");
    packBool(out, true);
    packStr(out, "blue_enabled");
    packBool(out, true);
    packStr(out, "far_red_enabled");
    packBool(out, true);
    packStr(out, "red_dist_pct");
    packUInt(out, 34);
    packStr(out, "blue_dist_pct");
    packUInt(out, 33);
    // far_red_dist_pct intentionally omitted
    return out;
}

std::vector<uint8_t> makeLedPayloadStringBrightness() {
    std::vector<uint8_t> out;
    packMap(out, 9);
    packStr(out, "command_id");
    packStr(out, "cmd-led-5");
    packStr(out, "power");
    packBool(out, true);
    packStr(out, "brightness_pct");
    packStr(out, "55");
    packStr(out, "red_enabled");
    packBool(out, true);
    packStr(out, "blue_enabled");
    packBool(out, true);
    packStr(out, "far_red_enabled");
    packBool(out, true);
    packStr(out, "red_dist_pct");
    packUInt(out, 34);
    packStr(out, "blue_dist_pct");
    packUInt(out, 33);
    packStr(out, "far_red_dist_pct");
    packUInt(out, 33);
    return out;
}

std::vector<uint8_t> makeLedPayloadStringPower() {
    std::vector<uint8_t> out = makeValidLedPayload();
    out.clear();
    packMap(out, 9);
    packStr(out, "command_id");
    packStr(out, "cmd-led-6");
    packStr(out, "power");
    packStr(out, "true");
    packStr(out, "brightness_pct");
    packUInt(out, 55);
    packStr(out, "red_enabled");
    packBool(out, true);
    packStr(out, "blue_enabled");
    packBool(out, true);
    packStr(out, "far_red_enabled");
    packBool(out, true);
    packStr(out, "red_dist_pct");
    packUInt(out, 34);
    packStr(out, "blue_dist_pct");
    packUInt(out, 33);
    packStr(out, "far_red_dist_pct");
    packUInt(out, 33);
    return out;
}

std::vector<uint8_t> makeMalformedLedPayload() {
    std::vector<uint8_t> out = makeValidLedPayload();
    out.pop_back();
    return out;
}

std::vector<uint8_t> makeConfigPayloadMissingThresholds() {
    std::vector<uint8_t> out;
    packMap(out, 1);
    packStr(out, "command_id");
    packStr(out, "cmd-cfg-1");
    return out;
}

std::vector<uint8_t> makeConfigPayloadNaNThresholds() {
    std::vector<uint8_t> out;
    packMap(out, 2);
    packStr(out, "command_id");
    packStr(out, "cmd-cfg-2");
    packStr(out, "thresholds");
    packMap(out, 4);
    packStr(out, "temp_min_c");
    packFloat32(out, std::numeric_limits<float>::quiet_NaN());
    packStr(out, "temp_max_c");
    packFloat32(out, 30.0f);
    packStr(out, "humidity_min_pct");
    packFloat32(out, 40.0f);
    packStr(out, "humidity_max_pct");
    packFloat32(out, 70.0f);
    return out;
}

std::vector<uint8_t> makeConfigPayloadMinGreaterThanMax() {
    std::vector<uint8_t> out;
    packMap(out, 2);
    packStr(out, "command_id");
    packStr(out, "cmd-cfg-3");
    packStr(out, "thresholds");
    packMap(out, 4);
    packStr(out, "temp_min_c");
    packFloat32(out, 31.0f);
    packStr(out, "temp_max_c");
    packFloat32(out, 20.0f);
    packStr(out, "humidity_min_pct");
    packFloat32(out, 40.0f);
    packStr(out, "humidity_max_pct");
    packFloat32(out, 70.0f);
    return out;
}

std::vector<uint8_t> makeConfigPayloadStringThresholds() {
    std::vector<uint8_t> out;
    packMap(out, 2);
    packStr(out, "command_id");
    packStr(out, "cmd-cfg-4");
    packStr(out, "thresholds");
    packMap(out, 4);
    packStr(out, "temp_min_c");
    packStr(out, "18.0");
    packStr(out, "temp_max_c");
    packFloat32(out, 30.0f);
    packStr(out, "humidity_min_pct");
    packFloat32(out, 40.0f);
    packStr(out, "humidity_max_pct");
    packFloat32(out, 70.0f);
    return out;
}

std::vector<uint8_t> makeValidConfigPayload() {
    std::vector<uint8_t> out;
    packMap(out, 2);
    packStr(out, "command_id");
    packStr(out, "cmd-cfg-ok");
    packStr(out, "thresholds");
    packMap(out, 4);
    packStr(out, "temp_min_c");
    packFloat32(out, 18.5f);
    packStr(out, "temp_max_c");
    packUInt(out, 30);
    packStr(out, "humidity_min_pct");
    packFloat32(out, 40.0f);
    packStr(out, "humidity_max_pct");
    packFloat32(out, 70.0f);
    return out;
}

std::vector<uint8_t> makeValidModePayload() {
    std::vector<uint8_t> out;
    packMap(out, 2);
    packStr(out, "command_id");
    packStr(out, "cmd-mode-ok");
    packStr(out, "mode");
    packUInt(out, static_cast<uint8_t>(DeviceMode::MANUAL));
    return out;
}

std::vector<uint8_t> makeModePayloadMissingMode() {
    std::vector<uint8_t> out;
    packMap(out, 1);
    packStr(out, "command_id");
    packStr(out, "cmd-mode-1");
    return out;
}

std::vector<uint8_t> makeModePayloadStringMode() {
    std::vector<uint8_t> out;
    packMap(out, 2);
    packStr(out, "command_id");
    packStr(out, "cmd-mode-3");
    packStr(out, "mode");
    packStr(out, "1");
    return out;
}

std::vector<uint8_t> makeModePayloadUnsupportedMode() {
    std::vector<uint8_t> out;
    packMap(out, 2);
    packStr(out, "command_id");
    packStr(out, "cmd-mode-2");
    packStr(out, "mode");
    packUInt(out, 2);
    return out;
}

void prepareCommandHandler() {
    if (s_prepared) {
        return;
    }

    TEST_ASSERT_TRUE(ConfigManager::init());
    TEST_ASSERT_TRUE(ConfigManager::loadDefaults());

    const RuntimeConfig cfg = ConfigManager::getConfigSnapshot();
    TEST_ASSERT_NOT_EQUAL('\0', cfg.device_id[0]);

    s_prepared = true;
}

bool decodeCommand(CommandKind kind, const uint8_t* payload, uint16_t length) {
    CommandEnvelope command = {};
    return CommandHandler::decode(kind, payload, length, command) ==
           CommandHandler::DecodeStatus::DECODED;
}

std::vector<uint8_t> makeModePayloadWithCommandId(const char* command_id) {
    std::vector<uint8_t> out;
    packMap(out, 2);
    packStr(out, "command_id");
    packStr(out, command_id);
    packStr(out, "mode");
    packUInt(out, 1);
    return out;
}

CommandHandler::DecodeStatus decodeModeWithCommandId(const char* command_id) {
    const auto payload = makeModePayloadWithCommandId(command_id);
    CommandEnvelope command = {};
    return CommandHandler::decode(
        CommandKind::MODE_CHANGE, payload.data(), static_cast<uint16_t>(payload.size()), command
    );
}

}  // namespace

// A '/' in the id would add a level to lumen/<device>/ack/<command_id>.
void test_command_handler_rejects_command_id_with_topic_separator() {
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CommandHandler::DecodeStatus::UNADDRESSABLE),
        static_cast<uint8_t>(decodeModeWithCommandId("cmd/mode"))
    );
}

// MQTT 3.1.1 section 4.7.1: '+' and '#' must not appear in a publish topic.
void test_command_handler_rejects_command_id_with_wildcard() {
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CommandHandler::DecodeStatus::UNADDRESSABLE),
        static_cast<uint8_t>(decodeModeWithCommandId("cmd+mode"))
    );
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CommandHandler::DecodeStatus::UNADDRESSABLE),
        static_cast<uint8_t>(decodeModeWithCommandId("cmd#"))
    );
}

void test_command_handler_rejects_null_payload() {
    TEST_ASSERT_FALSE(decodeCommand(CommandKind::LED_CONTROL, nullptr, 1));
}

void test_command_handler_rejects_zero_length_payload() {
    const uint8_t payload[] = {0x80U};
    TEST_ASSERT_FALSE(decodeCommand(CommandKind::LED_CONTROL, payload, 0));
}

void test_command_handler_rejects_malformed_led_payload() {
    prepareCommandHandler();
    const auto payload = makeMalformedLedPayload();
    const CommandKind kind = CommandKind::LED_CONTROL;

    TEST_ASSERT_FALSE(
        decodeCommand(
            kind,
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_led_missing_command_id() {
    prepareCommandHandler();
    const auto payload = makeLedPayloadMissingCommandId();
    const CommandKind kind = CommandKind::LED_CONTROL;

    TEST_ASSERT_FALSE(
        decodeCommand(
            kind,
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_invalid_fields_keep_command_id_for_ack() {
    prepareCommandHandler();
    const auto payload = makeLedPayloadBrightness101();
    const CommandKind kind = CommandKind::LED_CONTROL;
    CommandEnvelope command = {};

    const CommandHandler::DecodeStatus status = CommandHandler::decode(
        kind, payload.data(), static_cast<uint16_t>(payload.size()), command
    );

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CommandHandler::DecodeStatus::INVALID_FIELDS),
        static_cast<uint8_t>(status)
    );
    TEST_ASSERT_EQUAL_STRING("cmd-led-2", command.command_id.c_str());
}

void test_command_handler_missing_command_id_is_unaddressable() {
    prepareCommandHandler();
    const auto payload = makeLedPayloadMissingCommandId();
    const CommandKind kind = CommandKind::LED_CONTROL;
    CommandEnvelope command = {};

    const CommandHandler::DecodeStatus status = CommandHandler::decode(
        kind, payload.data(), static_cast<uint16_t>(payload.size()), command
    );

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CommandHandler::DecodeStatus::UNADDRESSABLE),
        static_cast<uint8_t>(status)
    );
}

void test_command_handler_rejects_led_brightness_over_100() {
    prepareCommandHandler();
    const auto payload = makeLedPayloadBrightness101();
    const CommandKind kind = CommandKind::LED_CONTROL;

    TEST_ASSERT_FALSE(
        decodeCommand(
            kind,
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_led_distribution_sum_not_100() {
    prepareCommandHandler();
    const auto payload = makeLedPayloadSum99();
    const CommandKind kind = CommandKind::LED_CONTROL;

    TEST_ASSERT_FALSE(
        decodeCommand(
            kind,
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_led_incomplete_payload() {
    prepareCommandHandler();
    const auto payload = makeLedPayloadIncomplete();
    const CommandKind kind = CommandKind::LED_CONTROL;

    TEST_ASSERT_FALSE(
        decodeCommand(
            kind,
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_led_string_brightness() {
    prepareCommandHandler();
    const auto payload = makeLedPayloadStringBrightness();
    const CommandKind kind = CommandKind::LED_CONTROL;

    TEST_ASSERT_FALSE(
        decodeCommand(
            kind,
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_led_string_power() {
    prepareCommandHandler();
    const auto payload = makeLedPayloadStringPower();
    const CommandKind kind = CommandKind::LED_CONTROL;

    TEST_ASSERT_FALSE(
        decodeCommand(
            kind,
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_config_missing_thresholds() {
    prepareCommandHandler();
    const auto payload = makeConfigPayloadMissingThresholds();
    const CommandKind kind = CommandKind::CONFIG_UPDATE;

    TEST_ASSERT_FALSE(
        decodeCommand(
            kind,
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_config_nan_thresholds() {
    prepareCommandHandler();
    const auto payload = makeConfigPayloadNaNThresholds();
    const CommandKind kind = CommandKind::CONFIG_UPDATE;

    TEST_ASSERT_FALSE(
        decodeCommand(
            kind,
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_config_min_greater_than_max() {
    prepareCommandHandler();
    const auto payload = makeConfigPayloadMinGreaterThanMax();
    const CommandKind kind = CommandKind::CONFIG_UPDATE;

    TEST_ASSERT_FALSE(
        decodeCommand(
            kind,
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_config_string_threshold() {
    prepareCommandHandler();
    const auto payload = makeConfigPayloadStringThresholds();
    const CommandKind kind = CommandKind::CONFIG_UPDATE;

    TEST_ASSERT_FALSE(
        decodeCommand(
            kind,
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_mode_missing_mode() {
    prepareCommandHandler();
    const auto payload = makeModePayloadMissingMode();
    const CommandKind kind = CommandKind::MODE_CHANGE;

    TEST_ASSERT_FALSE(
        decodeCommand(
            kind,
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_mode_unsupported_value() {
    prepareCommandHandler();
    const auto payload = makeModePayloadUnsupportedMode();
    const CommandKind kind = CommandKind::MODE_CHANGE;

    TEST_ASSERT_FALSE(
        decodeCommand(
            kind,
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_decodes_valid_led_payload() {
    prepareCommandHandler();
    const auto payload = makeValidLedPayload();
    const CommandKind kind = CommandKind::LED_CONTROL;
    CommandEnvelope command = {};

    TEST_ASSERT_TRUE(
        CommandHandler::decode(
            kind,
            payload.data(),
            static_cast<uint16_t>(payload.size()),
            command
        ) == CommandHandler::DecodeStatus::DECODED
    );
    TEST_ASSERT_EQUAL_STRING("cmd-led-1", command.command_id.c_str());
    const LedCommand* led = std::get_if<LedCommand>(&command.payload);
    TEST_ASSERT_NOT_NULL(led);
    TEST_ASSERT_TRUE(led->desired_led.raw().power);
    TEST_ASSERT_EQUAL_UINT8(55, led->desired_led.raw().brightness_pct);
    TEST_ASSERT_TRUE(led->desired_led.raw().red_enabled);
    TEST_ASSERT_TRUE(led->desired_led.raw().blue_enabled);
    TEST_ASSERT_TRUE(led->desired_led.raw().far_red_enabled);
    TEST_ASSERT_EQUAL_UINT8(34, led->desired_led.raw().red_dist_pct);
    TEST_ASSERT_EQUAL_UINT8(33, led->desired_led.raw().blue_dist_pct);
    TEST_ASSERT_EQUAL_UINT8(33, led->desired_led.raw().far_red_dist_pct);
}

void test_command_handler_decodes_valid_config_payload() {
    prepareCommandHandler();
    const auto payload = makeValidConfigPayload();
    const CommandKind kind = CommandKind::CONFIG_UPDATE;
    CommandEnvelope command = {};

    TEST_ASSERT_TRUE(
        CommandHandler::decode(
            kind,
            payload.data(),
            static_cast<uint16_t>(payload.size()),
            command
        ) == CommandHandler::DecodeStatus::DECODED
    );
    TEST_ASSERT_EQUAL_STRING("cmd-cfg-ok", command.command_id.c_str());
    const ConfigCommand* config = std::get_if<ConfigCommand>(&command.payload);
    TEST_ASSERT_NOT_NULL(config);
    TEST_ASSERT_EQUAL_FLOAT(18.5f, config->desired_thresholds.raw().temp_min_c);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, config->desired_thresholds.raw().temp_max_c);
    TEST_ASSERT_EQUAL_FLOAT(40.0f, config->desired_thresholds.raw().humidity_min_pct);
    TEST_ASSERT_EQUAL_FLOAT(70.0f, config->desired_thresholds.raw().humidity_max_pct);
}

void test_command_handler_decodes_valid_mode_payload() {
    prepareCommandHandler();
    const auto payload = makeValidModePayload();
    const CommandKind kind = CommandKind::MODE_CHANGE;
    CommandEnvelope command = {};

    TEST_ASSERT_TRUE(
        CommandHandler::decode(
            kind,
            payload.data(),
            static_cast<uint16_t>(payload.size()),
            command
        ) == CommandHandler::DecodeStatus::DECODED
    );
    TEST_ASSERT_EQUAL_STRING("cmd-mode-ok", command.command_id.c_str());
    const ModeCommand* mode = std::get_if<ModeCommand>(&command.payload);
    TEST_ASSERT_NOT_NULL(mode);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DeviceMode::MANUAL), static_cast<uint8_t>(mode->desired_mode)
    );
}

void test_command_handler_rejects_mode_string_value() {
    prepareCommandHandler();
    const auto payload = makeModePayloadStringMode();
    const CommandKind kind = CommandKind::MODE_CHANGE;

    TEST_ASSERT_FALSE(
        decodeCommand(
            kind,
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

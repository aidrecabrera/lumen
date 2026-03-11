#include <limits>
#include <string>
#include <vector>
#include <unity.h>

#include "lumen_app_types.h"
#include "lumen_command_handler.h"
#include "lumen_config_manager.h"
#include "lumen_task_manager.h"

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

std::vector<uint8_t> makeModePayloadMissingMode() {
    std::vector<uint8_t> out;
    packMap(out, 1);
    packStr(out, "command_id");
    packStr(out, "cmd-mode-1");
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
    TEST_ASSERT_TRUE(TaskManager::createQueues());
    TEST_ASSERT_TRUE(CommandHandler::init());

    const RuntimeConfig& cfg = ConfigManager::getConfig();
    TEST_ASSERT_NOT_EQUAL('\0', cfg.device_id[0]);

    s_prepared = true;
}

std::string makeTopic(const char* leaf) {
    const RuntimeConfig& cfg = ConfigManager::getConfig();
    std::string topic = "lumen/";
    topic += cfg.device_id;
    topic += "/command/";
    topic += leaf;
    return topic;
}

}  // namespace

void test_command_handler_rejects_null_topic() {
    const uint8_t payload[] = {0x80U};
    TEST_ASSERT_FALSE(CommandHandler::handleInbound(nullptr, payload, sizeof(payload)));
}

void test_command_handler_rejects_null_payload() {
    TEST_ASSERT_FALSE(CommandHandler::handleInbound("lumen/x/command/led", nullptr, 1));
}

void test_command_handler_rejects_zero_length_payload() {
    const uint8_t payload[] = {0x80U};
    TEST_ASSERT_FALSE(CommandHandler::handleInbound("lumen/x/command/led", payload, 0));
}

void test_command_handler_rejects_unknown_topic() {
    prepareCommandHandler();
    const auto payload = makeValidLedPayload();
    const std::string topic = makeTopic("unsupported");

    TEST_ASSERT_FALSE(
        CommandHandler::handleInbound(
            topic.c_str(),
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_malformed_led_payload() {
    prepareCommandHandler();
    const auto payload = makeMalformedLedPayload();
    const std::string topic = makeTopic("led");

    TEST_ASSERT_FALSE(
        CommandHandler::handleInbound(
            topic.c_str(),
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_led_missing_command_id() {
    prepareCommandHandler();
    const auto payload = makeLedPayloadMissingCommandId();
    const std::string topic = makeTopic("led");

    TEST_ASSERT_FALSE(
        CommandHandler::handleInbound(
            topic.c_str(),
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_led_brightness_over_100() {
    prepareCommandHandler();
    const auto payload = makeLedPayloadBrightness101();
    const std::string topic = makeTopic("led");

    TEST_ASSERT_FALSE(
        CommandHandler::handleInbound(
            topic.c_str(),
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_led_distribution_sum_not_100() {
    prepareCommandHandler();
    const auto payload = makeLedPayloadSum99();
    const std::string topic = makeTopic("led");

    TEST_ASSERT_FALSE(
        CommandHandler::handleInbound(
            topic.c_str(),
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_led_incomplete_payload() {
    prepareCommandHandler();
    const auto payload = makeLedPayloadIncomplete();
    const std::string topic = makeTopic("led");

    TEST_ASSERT_FALSE(
        CommandHandler::handleInbound(
            topic.c_str(),
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_config_missing_thresholds() {
    prepareCommandHandler();
    const auto payload = makeConfigPayloadMissingThresholds();
    const std::string topic = makeTopic("config");

    TEST_ASSERT_FALSE(
        CommandHandler::handleInbound(
            topic.c_str(),
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_config_nan_thresholds() {
    prepareCommandHandler();
    const auto payload = makeConfigPayloadNaNThresholds();
    const std::string topic = makeTopic("config");

    TEST_ASSERT_FALSE(
        CommandHandler::handleInbound(
            topic.c_str(),
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_config_min_greater_than_max() {
    prepareCommandHandler();
    const auto payload = makeConfigPayloadMinGreaterThanMax();
    const std::string topic = makeTopic("config");

    TEST_ASSERT_FALSE(
        CommandHandler::handleInbound(
            topic.c_str(),
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_mode_missing_mode() {
    prepareCommandHandler();
    const auto payload = makeModePayloadMissingMode();
    const std::string topic = makeTopic("mode");

    TEST_ASSERT_FALSE(
        CommandHandler::handleInbound(
            topic.c_str(),
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}

void test_command_handler_rejects_mode_unsupported_value() {
    prepareCommandHandler();
    const auto payload = makeModePayloadUnsupportedMode();
    const std::string topic = makeTopic("mode");

    TEST_ASSERT_FALSE(
        CommandHandler::handleInbound(
            topic.c_str(),
            payload.data(),
            static_cast<uint16_t>(payload.size())
        )
    );
}


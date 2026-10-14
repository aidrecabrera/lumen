#include <string.h>
#include <unity.h>

#include <limits>
#include <string>

#include "lumen_app_types.h"
#include "lumen_type_validation.h"

static ThresholdConfig makeValidThresholds() {
    return ThresholdConfig{18.0f, 28.0f, 40.0f, 70.0f};
}

static LedState makeValidLed() {
    return LedState{
        true,
        50,
        true,
        true,
        true,
        34,
        33,
        33
    };
}

void test_threshold_valid_normal_range() {
    const ThresholdConfig thresholds = makeValidThresholds();
    TEST_ASSERT_TRUE(ValidThresholds::parse(thresholds).has_value());
}

void test_threshold_valid_equal_bounds() {
    const ThresholdConfig thresholds{20.0f, 20.0f, 55.0f, 55.0f};
    TEST_ASSERT_TRUE(ValidThresholds::parse(thresholds).has_value());
}

void test_threshold_invalid_temp_min_gt_max() {
    const ThresholdConfig thresholds{30.0f, 20.0f, 40.0f, 60.0f};
    TEST_ASSERT_FALSE(ValidThresholds::parse(thresholds).has_value());
}

void test_threshold_invalid_humidity_min_gt_max() {
    const ThresholdConfig thresholds{20.0f, 30.0f, 70.0f, 60.0f};
    TEST_ASSERT_FALSE(ValidThresholds::parse(thresholds).has_value());
}

void test_threshold_invalid_nan_temp_min() {
    ThresholdConfig thresholds = makeValidThresholds();
    thresholds.temp_min_c = std::numeric_limits<float>::quiet_NaN();
    TEST_ASSERT_FALSE(ValidThresholds::parse(thresholds).has_value());
}

void test_threshold_invalid_nan_temp_max() {
    ThresholdConfig thresholds = makeValidThresholds();
    thresholds.temp_max_c = std::numeric_limits<float>::quiet_NaN();
    TEST_ASSERT_FALSE(ValidThresholds::parse(thresholds).has_value());
}

void test_threshold_invalid_nan_humidity_min() {
    ThresholdConfig thresholds = makeValidThresholds();
    thresholds.humidity_min_pct = std::numeric_limits<float>::quiet_NaN();
    TEST_ASSERT_FALSE(ValidThresholds::parse(thresholds).has_value());
}

void test_threshold_invalid_nan_humidity_max() {
    ThresholdConfig thresholds = makeValidThresholds();
    thresholds.humidity_max_pct = std::numeric_limits<float>::quiet_NaN();
    TEST_ASSERT_FALSE(ValidThresholds::parse(thresholds).has_value());
}

void test_threshold_invalid_inf_temp_min() {
    ThresholdConfig thresholds = makeValidThresholds();
    thresholds.temp_min_c = std::numeric_limits<float>::infinity();
    TEST_ASSERT_FALSE(ValidThresholds::parse(thresholds).has_value());
}

void test_threshold_invalid_inf_temp_max() {
    ThresholdConfig thresholds = makeValidThresholds();
    thresholds.temp_max_c = std::numeric_limits<float>::infinity();
    TEST_ASSERT_FALSE(ValidThresholds::parse(thresholds).has_value());
}

void test_threshold_invalid_inf_humidity_min() {
    ThresholdConfig thresholds = makeValidThresholds();
    thresholds.humidity_min_pct = std::numeric_limits<float>::infinity();
    TEST_ASSERT_FALSE(ValidThresholds::parse(thresholds).has_value());
}

void test_threshold_invalid_inf_humidity_max() {
    ThresholdConfig thresholds = makeValidThresholds();
    thresholds.humidity_max_pct = std::numeric_limits<float>::infinity();
    TEST_ASSERT_FALSE(ValidThresholds::parse(thresholds).has_value());
}

void test_led_valid_brightness_and_sum() {
    const LedState led = makeValidLed();
    TEST_ASSERT_TRUE(ValidLedState::parse(led).has_value());
}

void test_led_valid_brightness_100() {
    LedState led = makeValidLed();
    led.brightness_pct = 100;
    TEST_ASSERT_TRUE(ValidLedState::parse(led).has_value());
}

void test_led_invalid_brightness_101() {
    LedState led = makeValidLed();
    led.brightness_pct = 101;
    TEST_ASSERT_FALSE(ValidLedState::parse(led).has_value());
}

void test_led_invalid_sum_99() {
    LedState led = makeValidLed();
    led.red_dist_pct = 33;
    led.blue_dist_pct = 33;
    led.far_red_dist_pct = 33;
    TEST_ASSERT_FALSE(ValidLedState::parse(led).has_value());
}

void test_led_invalid_sum_101() {
    LedState led = makeValidLed();
    led.red_dist_pct = 34;
    led.blue_dist_pct = 34;
    led.far_red_dist_pct = 33;
    TEST_ASSERT_FALSE(ValidLedState::parse(led).has_value());
}

void test_energy_total_zero_valid() {
    TEST_ASSERT_TRUE(EnergyTotalWh::parse(0.0f).has_value());
}

void test_energy_total_positive_valid() {
    TEST_ASSERT_TRUE(EnergyTotalWh::parse(123.45f).has_value());
}

void test_energy_total_negative_invalid() {
    TEST_ASSERT_FALSE(EnergyTotalWh::parse(-0.001f).has_value());
}

void test_energy_total_nan_invalid() {
    TEST_ASSERT_FALSE(EnergyTotalWh::parse(std::numeric_limits<float>::quiet_NaN()).has_value());
}

void test_energy_total_inf_invalid() {
    TEST_ASSERT_FALSE(EnergyTotalWh::parse(std::numeric_limits<float>::infinity()).has_value());
}

// raw() must give back the exact value that parse() accepted.
void test_threshold_parse_keeps_raw_values() {
    const ThresholdConfig thresholds{19.5f, 27.25f, 42.0f, 68.0f};
    const std::optional<ValidThresholds> parsed = ValidThresholds::parse(thresholds);
    TEST_ASSERT_TRUE(parsed.has_value());
    TEST_ASSERT_EQUAL_FLOAT(19.5f, parsed->raw().temp_min_c);
    TEST_ASSERT_EQUAL_FLOAT(27.25f, parsed->raw().temp_max_c);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, parsed->raw().humidity_min_pct);
    TEST_ASSERT_EQUAL_FLOAT(68.0f, parsed->raw().humidity_max_pct);
}

void test_led_parse_keeps_raw_values() {
    const LedState led{false, 0, false, true, false, 0, 100, 0};
    const std::optional<ValidLedState> parsed = ValidLedState::parse(led);
    TEST_ASSERT_TRUE(parsed.has_value());
    TEST_ASSERT_FALSE(parsed->raw().power);
    TEST_ASSERT_EQUAL_UINT8(0, parsed->raw().brightness_pct);
    TEST_ASSERT_FALSE(parsed->raw().red_enabled);
    TEST_ASSERT_TRUE(parsed->raw().blue_enabled);
    TEST_ASSERT_FALSE(parsed->raw().far_red_enabled);
    TEST_ASSERT_EQUAL_UINT8(0, parsed->raw().red_dist_pct);
    TEST_ASSERT_EQUAL_UINT8(100, parsed->raw().blue_dist_pct);
    TEST_ASSERT_EQUAL_UINT8(0, parsed->raw().far_red_dist_pct);
}

// 200 + 56 + 100 = 356 overflows uint8_t to 100; the sum must use a wider type.
void test_led_parse_rejects_uint8_wrapping_sum() {
    const LedState led{true, 50, true, true, true, 200, 56, 100};
    TEST_ASSERT_FALSE(ValidLedState::parse(led).has_value());
}

void test_energy_total_parse_keeps_raw_value() {
    const std::optional<EnergyTotalWh> parsed = EnergyTotalWh::parse(8.5f);
    TEST_ASSERT_TRUE(parsed.has_value());
    TEST_ASSERT_EQUAL_FLOAT(8.5f, parsed->raw());
}
void test_parseDeviceMode_accepts_known_modes() {
    DeviceMode mode = DeviceMode::MANUAL;
    TEST_ASSERT_TRUE(parseDeviceMode(0U, mode));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DeviceMode::AUTONOMOUS), static_cast<uint8_t>(mode));
    TEST_ASSERT_TRUE(parseDeviceMode(1U, mode));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DeviceMode::MANUAL), static_cast<uint8_t>(mode));
}

void test_parseDeviceMode_rejects_unknown_mode() {
    DeviceMode mode = DeviceMode::MANUAL;
    TEST_ASSERT_FALSE(parseDeviceMode(2U, mode));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DeviceMode::MANUAL), static_cast<uint8_t>(mode));
}

static bool parseIdText(const char* text, CommandId& out_id) {
    return parseCommandId(text, strlen(text), out_id);
}

void test_parseCommandId_accepts_uuid_and_marks() {
    CommandId id;
    TEST_ASSERT_TRUE(parseIdText("123e4567-e89b-12d3-a456-426614174000", id));
    TEST_ASSERT_EQUAL_STRING("123e4567-e89b-12d3-a456-426614174000", id.c_str());
    TEST_ASSERT_TRUE(parseIdText("cmd_LED-1", id));
    TEST_ASSERT_EQUAL_STRING("cmd_LED-1", id.c_str());
}

void test_parseCommandId_accepts_max_length() {
    const std::string text(COMMAND_ID_MAX_LEN - 1U, 'a');
    CommandId id;
    TEST_ASSERT_TRUE(parseCommandId(text.c_str(), text.size(), id));
    TEST_ASSERT_EQUAL_STRING(text.c_str(), id.c_str());
}

void test_parseCommandId_rejects_empty_and_too_long() {
    const std::string too_long(COMMAND_ID_MAX_LEN, 'a');
    CommandId id;
    TEST_ASSERT_FALSE(parseIdText("", id));
    TEST_ASSERT_FALSE(parseCommandId(too_long.c_str(), too_long.size(), id));
    TEST_ASSERT_FALSE(parseCommandId(nullptr, 3U, id));
}

void test_parseCommandId_rejects_topic_characters() {
    CommandId id;
    TEST_ASSERT_FALSE(parseIdText("a/b", id));
    TEST_ASSERT_FALSE(parseIdText("a+b", id));
    TEST_ASSERT_FALSE(parseIdText("a#", id));
    TEST_ASSERT_FALSE(parseIdText("a b", id));
    TEST_ASSERT_FALSE(parseIdText("caf\xc3\xa9", id));
}

void test_parseCommandId_rejects_embedded_nul() {
    const char text[] = {'a', '\0', 'b'};
    CommandId id;
    TEST_ASSERT_FALSE(parseCommandId(text, sizeof(text), id));
}

void test_parseCommandId_failure_keeps_previous_id() {
    CommandId id;
    TEST_ASSERT_TRUE(parseIdText("first", id));
    TEST_ASSERT_FALSE(parseIdText("bad/id", id));
    TEST_ASSERT_EQUAL_STRING("first", id.c_str());
}

// Pins the ack "reason" wire text, because clients may match on it.
void test_ackReasonText_matches_wire_text() {
    TEST_ASSERT_EQUAL_STRING("mode applied", ackReasonText(AckReason::MODE_APPLIED));
    TEST_ASSERT_EQUAL_STRING("config applied", ackReasonText(AckReason::CONFIG_APPLIED));
    TEST_ASSERT_EQUAL_STRING("led applied", ackReasonText(AckReason::LED_APPLIED));
    TEST_ASSERT_EQUAL_STRING("not ready", ackReasonText(AckReason::NOT_READY));
    TEST_ASSERT_EQUAL_STRING("persist failed", ackReasonText(AckReason::PERSIST_FAILED));
    TEST_ASSERT_EQUAL_STRING("manual locked", ackReasonText(AckReason::MANUAL_LOCKED));
    TEST_ASSERT_EQUAL_STRING("invalid payload", ackReasonText(AckReason::INVALID_PAYLOAD));
    TEST_ASSERT_EQUAL_STRING("command queue full", ackReasonText(AckReason::COMMAND_QUEUE_FULL));
}

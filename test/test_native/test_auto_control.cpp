#include <unity.h>

#include "lumen_auto_control.h"

namespace {
const ThresholdConfig THRESHOLDS = {18.0f, 28.0f, 60.0f, 80.0f};

SensorReading reading(float lux, float temperature_c = 22.0f) {
    return SensorReading{0, lux, temperature_c, 70.0f, 0};
}

ValidLuxCeiling ceiling(float lux) { return ValidLuxCeiling::parse(lux).value(); }

AutoDecision decide(AutoState previous, const SensorReading& sample) {
    return decideAutonomous(previous, sample, THRESHOLDS, ceiling(10000.0f));
}
}  // namespace

void test_auto_turns_off_below_night_level() {
    const AutoDecision decision = decide(AutoState::ACTIVE, reading(9.0f));
    TEST_ASSERT_EQUAL(static_cast<int>(AutoState::NIGHT_OFF), static_cast<int>(decision.state));
    TEST_ASSERT_EQUAL_UINT8(0, decision.brightness_pct.value());
}

void test_auto_stays_off_inside_hysteresis_gap() {
    TEST_ASSERT_EQUAL(
        static_cast<int>(AutoState::NIGHT_OFF),
        static_cast<int>(decide(AutoState::NIGHT_OFF, reading(15.0f)).state)
    );
    TEST_ASSERT_EQUAL(
        static_cast<int>(AutoState::ACTIVE),
        static_cast<int>(decide(AutoState::ACTIVE, reading(15.0f)).state)
    );
}

void test_auto_comes_back_above_day_level() {
    const AutoDecision decision = decide(AutoState::NIGHT_OFF, reading(21.0f));
    TEST_ASSERT_EQUAL(static_cast<int>(AutoState::ACTIVE), static_cast<int>(decision.state));
    TEST_ASSERT_EQUAL_UINT8(100, decision.brightness_pct.value());
}

void test_auto_holds_brightness_outside_thresholds() {
    const AutoDecision decision = decide(AutoState::ACTIVE, reading(500.0f, 35.0f));
    TEST_ASSERT_EQUAL(static_cast<int>(AutoState::PAUSED_OUT_OF_RANGE), static_cast<int>(decision.state));
    TEST_ASSERT_FALSE(decision.brightness_pct.has_value());
}

void test_auto_brightness_follows_lux_ceiling() {
    TEST_ASSERT_EQUAL_UINT8(50, decide(AutoState::ACTIVE, reading(5000.0f)).brightness_pct.value());
    TEST_ASSERT_EQUAL_UINT8(0, decide(AutoState::ACTIVE, reading(12000.0f)).brightness_pct.value());
    const AutoDecision low_ceiling =
        decideAutonomous(AutoState::ACTIVE, reading(500.0f), THRESHOLDS, ceiling(1000.0f));
    TEST_ASSERT_EQUAL_UINT8(50, low_ceiling.brightness_pct.value());
}

void test_lux_ceiling_parse_range() {
    TEST_ASSERT_TRUE(ValidLuxCeiling::parse(100.0f).has_value());
    TEST_ASSERT_TRUE(ValidLuxCeiling::parse(65535.0f).has_value());
    TEST_ASSERT_FALSE(ValidLuxCeiling::parse(99.0f).has_value());
    TEST_ASSERT_FALSE(ValidLuxCeiling::parse(70000.0f).has_value());
}

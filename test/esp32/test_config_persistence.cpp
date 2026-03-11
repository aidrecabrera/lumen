#include <unity.h>

#include "lumen_app_types.h"
#include "lumen_config_manager.h"

static ThresholdConfig makeThresholdsA() {
    return ThresholdConfig{19.5f, 27.25f, 42.0f, 68.0f};
}

static LedState makeLedA() {
    return LedState{
        true,
        65,
        true,
        true,
        true,
        40,
        35,
        25
    };
}

static void assertThresholdsEqual(const ThresholdConfig& expected, const ThresholdConfig& actual) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected.temp_min_c, actual.temp_min_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected.temp_max_c, actual.temp_max_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected.humidity_min_pct, actual.humidity_min_pct);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected.humidity_max_pct, actual.humidity_max_pct);
}

static void assertLedEqual(const LedState& expected, const LedState& actual) {
    TEST_ASSERT_EQUAL(expected.power, actual.power);
    TEST_ASSERT_EQUAL_UINT8(expected.brightness_pct, actual.brightness_pct);
    TEST_ASSERT_EQUAL(expected.red_enabled, actual.red_enabled);
    TEST_ASSERT_EQUAL(expected.blue_enabled, actual.blue_enabled);
    TEST_ASSERT_EQUAL(expected.far_red_enabled, actual.far_red_enabled);
    TEST_ASSERT_EQUAL_UINT8(expected.red_dist_pct, actual.red_dist_pct);
    TEST_ASSERT_EQUAL_UINT8(expected.blue_dist_pct, actual.blue_dist_pct);
    TEST_ASSERT_EQUAL_UINT8(expected.far_red_dist_pct, actual.far_red_dist_pct);
}

void test_config_save_and_readback_mode() {
    TEST_ASSERT_TRUE(ConfigManager::init());
    TEST_ASSERT_TRUE(ConfigManager::loadDefaults());
    TEST_ASSERT_TRUE(ConfigManager::saveMode(DeviceMode::MANUAL));

    const RuntimeConfig& cfg = ConfigManager::getConfig();
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DeviceMode::MANUAL),
        static_cast<uint8_t>(cfg.mode)
    );
}

void test_config_save_and_readback_led_state() {
    const LedState led = makeLedA();

    TEST_ASSERT_TRUE(ConfigManager::init());
    TEST_ASSERT_TRUE(ConfigManager::loadDefaults());
    TEST_ASSERT_TRUE(ConfigManager::saveLedState(led));

    const RuntimeConfig& cfg = ConfigManager::getConfig();
    assertLedEqual(led, cfg.led);
}

void test_config_save_and_readback_thresholds() {
    const ThresholdConfig thresholds = makeThresholdsA();

    TEST_ASSERT_TRUE(ConfigManager::init());
    TEST_ASSERT_TRUE(ConfigManager::loadDefaults());
    TEST_ASSERT_TRUE(ConfigManager::saveThresholds(thresholds));

    const RuntimeConfig& cfg = ConfigManager::getConfig();
    assertThresholdsEqual(thresholds, cfg.thresholds);
}

void test_config_save_and_readback_energy_total() {
    constexpr float expected_wh = 12.75f;

    TEST_ASSERT_TRUE(ConfigManager::init());
    TEST_ASSERT_TRUE(ConfigManager::loadDefaults());
    TEST_ASSERT_TRUE(ConfigManager::saveEnergyTotal(expected_wh));

    const RuntimeConfig& cfg = ConfigManager::getConfig();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_wh, cfg.energy_total_wh);
}

void test_config_values_survive_reinit_equivalent() {
    const ThresholdConfig thresholds = makeThresholdsA();
    const LedState led = makeLedA();
    constexpr float expected_wh = 8.5f;
    constexpr DeviceMode expected_mode = DeviceMode::MANUAL;

    TEST_ASSERT_TRUE(ConfigManager::init());
    TEST_ASSERT_TRUE(ConfigManager::loadDefaults());
    TEST_ASSERT_TRUE(ConfigManager::saveMode(expected_mode));
    TEST_ASSERT_TRUE(ConfigManager::saveLedState(led));
    TEST_ASSERT_TRUE(ConfigManager::saveThresholds(thresholds));
    TEST_ASSERT_TRUE(ConfigManager::saveEnergyTotal(expected_wh));

    TEST_ASSERT_TRUE_MESSAGE(
        ConfigManager::init(),
        "Re-init equivalent failed; full reboot remains a manual checklist item."
    );

    const RuntimeConfig& cfg = ConfigManager::getConfig();
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(expected_mode),
        static_cast<uint8_t>(cfg.mode)
    );
    assertLedEqual(led, cfg.led);
    assertThresholdsEqual(thresholds, cfg.thresholds);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_wh, cfg.energy_total_wh);
}
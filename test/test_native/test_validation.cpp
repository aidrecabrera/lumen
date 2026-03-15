#include <limits>
#include <unity.h>

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
    TEST_ASSERT_TRUE(isThresholdConfigValid(thresholds));
}

void test_threshold_valid_equal_bounds() {
    const ThresholdConfig thresholds{20.0f, 20.0f, 55.0f, 55.0f};
    TEST_ASSERT_TRUE(isThresholdConfigValid(thresholds));
}

void test_threshold_invalid_temp_min_gt_max() {
    const ThresholdConfig thresholds{30.0f, 20.0f, 40.0f, 60.0f};
    TEST_ASSERT_FALSE(isThresholdConfigValid(thresholds));
}

void test_threshold_invalid_humidity_min_gt_max() {
    const ThresholdConfig thresholds{20.0f, 30.0f, 70.0f, 60.0f};
    TEST_ASSERT_FALSE(isThresholdConfigValid(thresholds));
}

void test_threshold_invalid_nan_temp_min() {
    ThresholdConfig thresholds = makeValidThresholds();
    thresholds.temp_min_c = std::numeric_limits<float>::quiet_NaN();
    TEST_ASSERT_FALSE(isThresholdConfigValid(thresholds));
}

void test_threshold_invalid_nan_temp_max() {
    ThresholdConfig thresholds = makeValidThresholds();
    thresholds.temp_max_c = std::numeric_limits<float>::quiet_NaN();
    TEST_ASSERT_FALSE(isThresholdConfigValid(thresholds));
}

void test_threshold_invalid_nan_humidity_min() {
    ThresholdConfig thresholds = makeValidThresholds();
    thresholds.humidity_min_pct = std::numeric_limits<float>::quiet_NaN();
    TEST_ASSERT_FALSE(isThresholdConfigValid(thresholds));
}

void test_threshold_invalid_nan_humidity_max() {
    ThresholdConfig thresholds = makeValidThresholds();
    thresholds.humidity_max_pct = std::numeric_limits<float>::quiet_NaN();
    TEST_ASSERT_FALSE(isThresholdConfigValid(thresholds));
}

void test_threshold_invalid_inf_temp_min() {
    ThresholdConfig thresholds = makeValidThresholds();
    thresholds.temp_min_c = std::numeric_limits<float>::infinity();
    TEST_ASSERT_FALSE(isThresholdConfigValid(thresholds));
}

void test_threshold_invalid_inf_temp_max() {
    ThresholdConfig thresholds = makeValidThresholds();
    thresholds.temp_max_c = std::numeric_limits<float>::infinity();
    TEST_ASSERT_FALSE(isThresholdConfigValid(thresholds));
}

void test_threshold_invalid_inf_humidity_min() {
    ThresholdConfig thresholds = makeValidThresholds();
    thresholds.humidity_min_pct = std::numeric_limits<float>::infinity();
    TEST_ASSERT_FALSE(isThresholdConfigValid(thresholds));
}

void test_threshold_invalid_inf_humidity_max() {
    ThresholdConfig thresholds = makeValidThresholds();
    thresholds.humidity_max_pct = std::numeric_limits<float>::infinity();
    TEST_ASSERT_FALSE(isThresholdConfigValid(thresholds));
}

void test_led_valid_brightness_and_sum() {
    const LedState led = makeValidLed();
    TEST_ASSERT_TRUE(isLedStateValid(led));
}

void test_led_valid_brightness_100() {
    LedState led = makeValidLed();
    led.brightness_pct = 100;
    TEST_ASSERT_TRUE(isLedStateValid(led));
}

void test_led_invalid_brightness_101() {
    LedState led = makeValidLed();
    led.brightness_pct = 101;
    TEST_ASSERT_FALSE(isLedStateValid(led));
}

void test_led_invalid_sum_99() {
    LedState led = makeValidLed();
    led.red_dist_pct = 33;
    led.blue_dist_pct = 33;
    led.far_red_dist_pct = 33;
    TEST_ASSERT_FALSE(isLedStateValid(led));
}

void test_led_invalid_sum_101() {
    LedState led = makeValidLed();
    led.red_dist_pct = 34;
    led.blue_dist_pct = 34;
    led.far_red_dist_pct = 33;
    TEST_ASSERT_FALSE(isLedStateValid(led));
}

void test_energy_total_zero_valid() {
    TEST_ASSERT_TRUE(isEnergyTotalValid(0.0f));
}

void test_energy_total_positive_valid() {
    TEST_ASSERT_TRUE(isEnergyTotalValid(123.45f));
}

void test_energy_total_negative_invalid() {
    TEST_ASSERT_FALSE(isEnergyTotalValid(-0.001f));
}

void test_energy_total_nan_invalid() {
    TEST_ASSERT_FALSE(isEnergyTotalValid(std::numeric_limits<float>::quiet_NaN()));
}

void test_energy_total_inf_invalid() {
    TEST_ASSERT_FALSE(isEnergyTotalValid(std::numeric_limits<float>::infinity()));
}
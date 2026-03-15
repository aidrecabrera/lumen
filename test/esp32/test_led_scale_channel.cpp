#include <unity.h>


/// @brief duplicated to test lumen_led_control.cpp, i dont want to expose the function just for testing
static uint8_t scaleChannel(uint8_t brightness_pct, uint8_t channel_pct) {
    const uint32_t scaled =
        static_cast<uint32_t>(brightness_pct) * static_cast<uint32_t>(channel_pct) * 255U;

    return static_cast<uint8_t>(scaled / 10000U);
}

void test_scale_channel_full_brightness_full_channel() {
    TEST_ASSERT_EQUAL_UINT8(255, scaleChannel(100, 100));
}

void test_scale_channel_default_red() {
    TEST_ASSERT_EQUAL_UINT8(51, scaleChannel(50, 40));
}

void test_scale_channel_default_blue() {
    TEST_ASSERT_EQUAL_UINT8(44, scaleChannel(50, 35));
}

void test_scale_channel_default_far_red() {
    TEST_ASSERT_EQUAL_UINT8(31, scaleChannel(50, 25));
}

void test_scale_channel_zero_brightness() {
    TEST_ASSERT_EQUAL_UINT8(0, scaleChannel(0, 100));
}

void test_scale_channel_zero_channel() {
    TEST_ASSERT_EQUAL_UINT8(0, scaleChannel(100, 0));
}

void test_scale_channel_both_zero() {
    TEST_ASSERT_EQUAL_UINT8(0, scaleChannel(0, 0));
}
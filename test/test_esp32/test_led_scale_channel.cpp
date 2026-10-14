#include <unity.h>

#include "lumen_led_control.h"

using LedControl::scaleChannel;

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

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

// test_validation.cpp
void test_threshold_valid_normal_range();
void test_threshold_valid_equal_bounds();
void test_threshold_invalid_temp_min_gt_max();
void test_threshold_invalid_humidity_min_gt_max();
void test_threshold_invalid_nan_temp_min();
void test_threshold_invalid_nan_temp_max();
void test_threshold_invalid_nan_humidity_min();
void test_threshold_invalid_nan_humidity_max();
void test_threshold_invalid_inf_temp_min();
void test_threshold_invalid_inf_temp_max();
void test_threshold_invalid_inf_humidity_min();
void test_threshold_invalid_inf_humidity_max();
void test_led_valid_brightness_and_sum();
void test_led_valid_brightness_100();
void test_led_invalid_brightness_101();
void test_led_invalid_sum_99();
void test_led_invalid_sum_101();
void test_energy_total_zero_valid();
void test_energy_total_positive_valid();
void test_energy_total_negative_invalid();
void test_energy_total_nan_invalid();
void test_energy_total_inf_invalid();

// test_system_utils.cpp
void test_copyText_normal_copy();
void test_copyText_exact_fit_copy();
void test_copyText_source_too_long();
void test_copyText_null_source();
void test_copyText_null_dest();
void test_copyText_zero_dest_length();
void test_copyText_null_termination_on_success();
void test_copyText_dest_cleared_on_overflow();
void test_endsWith_exact_suffix_match();
void test_endsWith_no_match();
void test_endsWith_suffix_longer_than_text();
void test_endsWith_empty_suffix();
void test_endsWith_null_text();
void test_endsWith_null_suffix();
void test_getNowMs_monotonic_non_decreasing();



int main() {
    UNITY_BEGIN();

    RUN_TEST(test_threshold_valid_normal_range);
    RUN_TEST(test_threshold_valid_equal_bounds);
    RUN_TEST(test_threshold_invalid_temp_min_gt_max);
    RUN_TEST(test_threshold_invalid_humidity_min_gt_max);
    RUN_TEST(test_threshold_invalid_nan_temp_min);
    RUN_TEST(test_threshold_invalid_nan_temp_max);
    RUN_TEST(test_threshold_invalid_nan_humidity_min);
    RUN_TEST(test_threshold_invalid_nan_humidity_max);
    RUN_TEST(test_threshold_invalid_inf_temp_min);
    RUN_TEST(test_threshold_invalid_inf_temp_max);
    RUN_TEST(test_threshold_invalid_inf_humidity_min);
    RUN_TEST(test_threshold_invalid_inf_humidity_max);
    RUN_TEST(test_led_valid_brightness_and_sum);
    RUN_TEST(test_led_valid_brightness_100);
    RUN_TEST(test_led_invalid_brightness_101);
    RUN_TEST(test_led_invalid_sum_99);
    RUN_TEST(test_led_invalid_sum_101);
    RUN_TEST(test_energy_total_zero_valid);
    RUN_TEST(test_energy_total_positive_valid);
    RUN_TEST(test_energy_total_negative_invalid);
    RUN_TEST(test_energy_total_nan_invalid);
    RUN_TEST(test_energy_total_inf_invalid);

    RUN_TEST(test_copyText_normal_copy);
    RUN_TEST(test_copyText_exact_fit_copy);
    RUN_TEST(test_copyText_source_too_long);
    RUN_TEST(test_copyText_null_source);
    RUN_TEST(test_copyText_null_dest);
    RUN_TEST(test_copyText_zero_dest_length);
    RUN_TEST(test_copyText_null_termination_on_success);
    RUN_TEST(test_copyText_dest_cleared_on_overflow);
    RUN_TEST(test_endsWith_exact_suffix_match);
    RUN_TEST(test_endsWith_no_match);
    RUN_TEST(test_endsWith_suffix_longer_than_text);
    RUN_TEST(test_endsWith_empty_suffix);
    RUN_TEST(test_endsWith_null_text);
    RUN_TEST(test_endsWith_null_suffix);
    RUN_TEST(test_getNowMs_monotonic_non_decreasing);

    return UNITY_END();
}

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
void test_threshold_parse_keeps_raw_values();
void test_led_parse_keeps_raw_values();
void test_led_parse_rejects_uint8_wrapping_sum();
void test_energy_total_parse_keeps_raw_value();

// test_system_utils.cpp
void test_copyText_normal_copy();
void test_copyText_exact_fit_copy();
void test_copyText_source_too_long();
void test_copyText_null_source();
void test_copyText_null_dest();
void test_copyText_zero_dest_length();
void test_copyText_null_termination_on_success();
void test_copyText_dest_cleared_on_overflow();
void test_getNowMs_monotonic_non_decreasing();
void test_nextBackoffMs_doubles_below_max();
void test_nextBackoffMs_caps_at_max();
void test_backoffWithJitterMs_stays_in_window();
void test_backoffWithJitterMs_no_jitter_for_tiny_backoff();
void test_parseDeviceMode_accepts_known_modes();
void test_parseDeviceMode_rejects_unknown_mode();
void test_parseCommandId_accepts_uuid_and_marks();
void test_parseCommandId_accepts_max_length();
void test_parseCommandId_rejects_empty_and_too_long();
void test_parseCommandId_rejects_topic_characters();
void test_parseCommandId_rejects_embedded_nul();
void test_parseCommandId_failure_keeps_previous_id();
void test_ackReasonText_matches_wire_text();

// test_wire.cpp
void test_wire_telemetry_matches_fixture();
void test_wire_status_matches_fixture();
void test_wire_energy_matches_fixture();
void test_wire_ack_matches_fixture();
void test_wire_encode_reports_overflow();
void test_wire_frame_matches_fixture();
void test_wire_frame_decodes_command();
void test_wire_frame_rejects_unknown_suffix_and_bad_bytes();
void test_wire_frame_rejects_oversized_payload();
void test_contract_led_command_fixture_decodes();
void test_contract_config_command_fixture_decodes();
void test_contract_mode_command_fixture_decodes();
void test_contract_read_command_id_for_unknown_kind();

// test_auto_control.cpp
void test_auto_turns_off_below_night_level();
void test_auto_stays_off_inside_hysteresis_gap();
void test_auto_comes_back_above_day_level();
void test_auto_holds_brightness_outside_thresholds();
void test_auto_brightness_follows_lux_ceiling();
void test_lux_ceiling_parse_range();
// test_cloud_config.cpp
void test_cloud_config_parses_x509();
void test_cloud_config_parses_password();
void test_cloud_config_allows_plain_tcp_for_password_only();
void test_cloud_config_rejects_unknown_auth_type();
void test_cloud_config_rejects_missing_field();
void test_cloud_config_rejects_bad_pem_marker();
void test_cloud_config_rejects_oversized_and_bad_json();

// test_validation.cpp (setup)
void test_wifi_credentials_parse_rules();
void test_local_password_hash_parse_rules();

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
    RUN_TEST(test_threshold_parse_keeps_raw_values);
    RUN_TEST(test_led_parse_keeps_raw_values);
    RUN_TEST(test_led_parse_rejects_uint8_wrapping_sum);
    RUN_TEST(test_energy_total_parse_keeps_raw_value);

    RUN_TEST(test_copyText_normal_copy);
    RUN_TEST(test_copyText_exact_fit_copy);
    RUN_TEST(test_copyText_source_too_long);
    RUN_TEST(test_copyText_null_source);
    RUN_TEST(test_copyText_null_dest);
    RUN_TEST(test_copyText_zero_dest_length);
    RUN_TEST(test_copyText_null_termination_on_success);
    RUN_TEST(test_copyText_dest_cleared_on_overflow);
    RUN_TEST(test_getNowMs_monotonic_non_decreasing);
    RUN_TEST(test_nextBackoffMs_doubles_below_max);
    RUN_TEST(test_nextBackoffMs_caps_at_max);
    RUN_TEST(test_backoffWithJitterMs_stays_in_window);
    RUN_TEST(test_backoffWithJitterMs_no_jitter_for_tiny_backoff);
    RUN_TEST(test_parseDeviceMode_accepts_known_modes);
    RUN_TEST(test_parseDeviceMode_rejects_unknown_mode);
    RUN_TEST(test_parseCommandId_accepts_uuid_and_marks);
    RUN_TEST(test_parseCommandId_accepts_max_length);
    RUN_TEST(test_parseCommandId_rejects_empty_and_too_long);
    RUN_TEST(test_parseCommandId_rejects_topic_characters);
    RUN_TEST(test_parseCommandId_rejects_embedded_nul);
    RUN_TEST(test_parseCommandId_failure_keeps_previous_id);
    RUN_TEST(test_ackReasonText_matches_wire_text);

    RUN_TEST(test_wire_telemetry_matches_fixture);
    RUN_TEST(test_wire_status_matches_fixture);
    RUN_TEST(test_wire_energy_matches_fixture);
    RUN_TEST(test_wire_ack_matches_fixture);
    RUN_TEST(test_wire_encode_reports_overflow);
    RUN_TEST(test_wire_frame_matches_fixture);
    RUN_TEST(test_wire_frame_decodes_command);
    RUN_TEST(test_wire_frame_rejects_unknown_suffix_and_bad_bytes);
    RUN_TEST(test_wire_frame_rejects_oversized_payload);
    RUN_TEST(test_contract_led_command_fixture_decodes);
    RUN_TEST(test_contract_config_command_fixture_decodes);
    RUN_TEST(test_contract_mode_command_fixture_decodes);
    RUN_TEST(test_contract_read_command_id_for_unknown_kind);

    RUN_TEST(test_auto_turns_off_below_night_level);
    RUN_TEST(test_auto_stays_off_inside_hysteresis_gap);
    RUN_TEST(test_auto_comes_back_above_day_level);
    RUN_TEST(test_auto_holds_brightness_outside_thresholds);
    RUN_TEST(test_auto_brightness_follows_lux_ceiling);
    RUN_TEST(test_lux_ceiling_parse_range);


    RUN_TEST(test_cloud_config_parses_x509);
    RUN_TEST(test_cloud_config_parses_password);
    RUN_TEST(test_cloud_config_allows_plain_tcp_for_password_only);
    RUN_TEST(test_cloud_config_rejects_unknown_auth_type);
    RUN_TEST(test_cloud_config_rejects_missing_field);
    RUN_TEST(test_cloud_config_rejects_bad_pem_marker);
    RUN_TEST(test_cloud_config_rejects_oversized_and_bad_json);

    RUN_TEST(test_wifi_credentials_parse_rules);
    RUN_TEST(test_local_password_hash_parse_rules);

    return UNITY_END();
}

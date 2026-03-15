#include <Arduino.h>
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

// test_module_init.cpp
void test_sensors_read_before_init_fails();
void test_energy_requestPersist_before_init_fails();
void test_mqtt_init_rejects_null_device_id();
void test_mqtt_init_rejects_empty_device_id();
void test_mqtt_init_rejects_oversized_device_id();
void test_mqtt_publish_functions_fail_before_connect();
void test_led_control_init_succeeds();
void test_led_control_rejects_manual_led_command_in_autonomous_mode();
void test_led_control_mode_change_command_applies();
void test_led_control_accepts_valid_led_command_in_manual_mode();
void test_led_control_rejects_invalid_config_command();
void test_led_control_buildStatusMessage_reflects_runtime_state();
void test_energy_tracker_init_succeeds();
void test_energy_tracker_init_resets_session_counters();
void test_energy_tracker_led_off_does_not_increase_session_energy();
void test_energy_tracker_full_brightness_updates_session_and_max_counter();
void test_wifi_manager_init_succeeds();
void test_mqtt_client_init_with_valid_device_id_succeeds();

// test_config_persistence.cpp
void test_config_save_and_readback_mode();
void test_config_save_and_readback_led_state();
void test_config_save_and_readback_thresholds();
void test_config_save_and_readback_energy_total();
void test_config_values_survive_reinit_equivalent();

// test_sensor_bringup.cpp
void test_sensors_init_succeeds();
void test_sensors_readCurrent_succeeds_and_sequence_increments();
void test_sensors_health_true_after_successful_read();

// test_command_contracts.cpp
void test_command_handler_rejects_null_topic();
void test_command_handler_rejects_null_payload();
void test_command_handler_rejects_zero_length_payload();
void test_command_handler_rejects_unknown_topic();
void test_command_handler_rejects_malformed_led_payload();
void test_command_handler_rejects_led_missing_command_id();
void test_command_handler_rejects_led_brightness_over_100();
void test_command_handler_rejects_led_distribution_sum_not_100();
void test_command_handler_rejects_led_incomplete_payload();
void test_command_handler_rejects_config_missing_thresholds();
void test_command_handler_rejects_config_nan_thresholds();
void test_command_handler_rejects_config_min_greater_than_max();
void test_command_handler_rejects_mode_missing_mode();
void test_command_handler_rejects_mode_unsupported_value();

// test_led_scale_channel.cpp
void test_scale_channel_full_brightness_full_channel();
void test_scale_channel_default_red();
void test_scale_channel_default_blue();
void test_scale_channel_default_far_red();
void test_scale_channel_zero_brightness();
void test_scale_channel_zero_channel();
void test_scale_channel_both_zero();

void setup() {
    Serial.begin(115200);
    delay(1500);

    UNITY_BEGIN();

    RUN_TEST(test_sensors_read_before_init_fails);
    RUN_TEST(test_energy_requestPersist_before_init_fails);
    RUN_TEST(test_mqtt_init_rejects_null_device_id);
    RUN_TEST(test_mqtt_init_rejects_empty_device_id);
    RUN_TEST(test_mqtt_init_rejects_oversized_device_id);
    RUN_TEST(test_mqtt_publish_functions_fail_before_connect);

    RUN_TEST(test_config_save_and_readback_mode);
    RUN_TEST(test_config_save_and_readback_led_state);
    RUN_TEST(test_config_save_and_readback_thresholds);
    RUN_TEST(test_config_save_and_readback_energy_total);
    RUN_TEST(test_config_values_survive_reinit_equivalent);

    RUN_TEST(test_led_control_init_succeeds);
    RUN_TEST(test_led_control_rejects_manual_led_command_in_autonomous_mode);
    RUN_TEST(test_led_control_mode_change_command_applies);
    RUN_TEST(test_led_control_accepts_valid_led_command_in_manual_mode);
    RUN_TEST(test_led_control_rejects_invalid_config_command);
    RUN_TEST(test_led_control_buildStatusMessage_reflects_runtime_state);

    RUN_TEST(test_energy_tracker_init_succeeds);
    RUN_TEST(test_energy_tracker_init_resets_session_counters);
    RUN_TEST(test_energy_tracker_led_off_does_not_increase_session_energy);
    RUN_TEST(test_energy_tracker_full_brightness_updates_session_and_max_counter);

    RUN_TEST(test_wifi_manager_init_succeeds);
    RUN_TEST(test_mqtt_client_init_with_valid_device_id_succeeds);

    RUN_TEST(test_command_handler_rejects_null_topic);
    RUN_TEST(test_command_handler_rejects_null_payload);
    RUN_TEST(test_command_handler_rejects_zero_length_payload);
    RUN_TEST(test_command_handler_rejects_unknown_topic);
    RUN_TEST(test_command_handler_rejects_malformed_led_payload);
    RUN_TEST(test_command_handler_rejects_led_missing_command_id);
    RUN_TEST(test_command_handler_rejects_led_brightness_over_100);
    RUN_TEST(test_command_handler_rejects_led_distribution_sum_not_100);
    RUN_TEST(test_command_handler_rejects_led_incomplete_payload);
    RUN_TEST(test_command_handler_rejects_config_missing_thresholds);
    RUN_TEST(test_command_handler_rejects_config_nan_thresholds);
    RUN_TEST(test_command_handler_rejects_config_min_greater_than_max);
    RUN_TEST(test_command_handler_rejects_mode_missing_mode);
    RUN_TEST(test_command_handler_rejects_mode_unsupported_value);

    RUN_TEST(test_sensors_init_succeeds);
    RUN_TEST(test_sensors_readCurrent_succeeds_and_sequence_increments);
    RUN_TEST(test_sensors_health_true_after_successful_read);

    RUN_TEST(test_led_control_buildStatusMessage_reflects_runtime_state);

    RUN_TEST(test_scale_channel_full_brightness_full_channel);
    RUN_TEST(test_scale_channel_default_red);
    RUN_TEST(test_scale_channel_default_blue);
    RUN_TEST(test_scale_channel_default_far_red);
    RUN_TEST(test_scale_channel_zero_brightness);
    RUN_TEST(test_scale_channel_zero_channel);
    RUN_TEST(test_scale_channel_both_zero);

    RUN_TEST(test_energy_tracker_init_succeeds);

    UNITY_END();
}

void loop() {}
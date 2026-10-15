#include <Arduino.h>
#include <cstring>
#include <unity.h>

#include "lumen_app_types.h"
#include "lumen_board_config.h"
#include "lumen_cloud_config.h"
#include "lumen_config_manager.h"
#include "lumen_energy_tracker.h"
#include "lumen_led_control.h"
#include "lumen_mqtt_client.h"
#include "lumen_sensor_manager.h"
#include "lumen_type_validation.h"
#include "lumen_wifi_manager.h"

static ThresholdConfig makeValidThresholds() {
    return ThresholdConfig{18.0f, 28.0f, 40.0f, 70.0f};
}

static LedState makeValidLed(uint8_t brightness_pct = 50U) {
    return LedState{
        true,
        brightness_pct,
        true,
        true,
        true,
        34,
        33,
        33
    };
}

static LedState makeOffLed() {
    LedState led = makeValidLed(50U);
    led.power = false;
    return led;
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

static ValidLedState requireValidLed(const LedState& led) {
    const std::optional<ValidLedState> parsed = ValidLedState::parse(led);
    TEST_ASSERT_TRUE(parsed.has_value());
    return *parsed;
}

static ValidThresholds requireValidThresholds(const ThresholdConfig& thresholds) {
    const std::optional<ValidThresholds> parsed = ValidThresholds::parse(thresholds);
    TEST_ASSERT_TRUE(parsed.has_value());
    return *parsed;
}

static EnergyTotalWh requireValidEnergyTotal(float total_wh) {
    const std::optional<EnergyTotalWh> parsed = EnergyTotalWh::parse(total_wh);
    TEST_ASSERT_TRUE(parsed.has_value());
    return *parsed;
}

static CommandEnvelope makeModeCommand(const char* id, DeviceMode desired_mode) {
    CommandEnvelope cmd{};
    TEST_ASSERT_TRUE(parseCommandId(id, std::strlen(id), cmd.command_id));
    cmd.payload = ModeCommand{desired_mode};
    return cmd;
}

static CommandEnvelope makeLedCommand(const char* id, const LedState& desired_led) {
    CommandEnvelope cmd{};
    TEST_ASSERT_TRUE(parseCommandId(id, std::strlen(id), cmd.command_id));
    cmd.payload = LedCommand{requireValidLed(desired_led)};
    return cmd;
}

static void prepareLedControl(DeviceMode initial_mode) {
    TEST_ASSERT_TRUE(ConfigManager::init());
    TEST_ASSERT_TRUE(ConfigManager::loadDefaults());
    TEST_ASSERT_TRUE(ConfigManager::saveThresholds(requireValidThresholds(makeValidThresholds())));
    TEST_ASSERT_TRUE(ConfigManager::saveLedState(requireValidLed(makeValidLed(25U))));
    TEST_ASSERT_TRUE(ConfigManager::saveMode(initial_mode));
    TEST_ASSERT_TRUE(LedControl::init());
}

// plain tcp to a local address: these tests check init and publish guards, not a broker.
static const CloudConfig& testCloudConfig() {
    static CloudConfig config = [] {
        CloudConfig built{};
        std::strcpy(built.endpoint, "127.0.0.1");
        built.port = 1883;
        PasswordAuth& auth = built.auth.emplace<PasswordAuth>();
        std::strcpy(auth.username, "test");
        std::strcpy(auth.password, "test");
        return built;
    }();
    return config;
}

static SensorReading makeReading(float light_lux, float temperature_c) {
    SensorReading reading{};
    reading.timestamp_ms = 1U;
    reading.light_lux = light_lux;
    reading.temperature_c = temperature_c;
    reading.humidity_pct = 55.0f;
    reading.sequence = 1U;
    return reading;
}

static void prepareEnergyTracker(float starting_total_wh) {
    TEST_ASSERT_TRUE(ConfigManager::init());
    TEST_ASSERT_TRUE(ConfigManager::loadDefaults());
    TEST_ASSERT_TRUE(ConfigManager::saveEnergyTotal(requireValidEnergyTotal(starting_total_wh)));
    TEST_ASSERT_TRUE(EnergyTracker::init());
}

void test_sensors_read_before_init_fails() {
    SensorReading reading{};
    TEST_ASSERT_FALSE(Sensors::readCurrent(reading));
}

void test_energy_requestPersist_before_init_fails() {
    TEST_ASSERT_FALSE(EnergyTracker::requestPersist());
}

void test_mqtt_init_rejects_null_device_id() {
    TEST_ASSERT_FALSE(MqttClient::init(nullptr, testCloudConfig()));
}

void test_mqtt_init_rejects_empty_device_id() {
    TEST_ASSERT_FALSE(MqttClient::init("", testCloudConfig()));
}

void test_mqtt_init_rejects_oversized_device_id() {
    char oversized[DEVICE_ID_MAX_LEN + 2U];
    std::memset(oversized, 'x', sizeof(oversized));
    oversized[sizeof(oversized) - 1U] = '\0';
    TEST_ASSERT_FALSE(MqttClient::init(oversized, testCloudConfig()));
}

void test_mqtt_publish_functions_fail_before_connect() {
    SensorReading reading{};
    StatusMessage status{};
    EnergyMessage energy{};
    AckMessage ack{};

    TEST_ASSERT_TRUE(MqttClient::init("testnode", testCloudConfig()));
    TEST_ASSERT_FALSE(MqttClient::publishTelemetry(reading));
    TEST_ASSERT_FALSE(MqttClient::publishStatus(status));
    TEST_ASSERT_FALSE(MqttClient::publishEnergy(energy));
    TEST_ASSERT_FALSE(MqttClient::publishAck(ack));
}

void test_led_control_init_succeeds() {
    prepareLedControl(DeviceMode::AUTONOMOUS);
    const DeviceMode mode = LedControl::getCurrentMode();
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DeviceMode::AUTONOMOUS),
        static_cast<uint8_t>(mode)
    );
}

void test_led_control_rejects_manual_led_command_in_autonomous_mode() {
    prepareLedControl(DeviceMode::AUTONOMOUS);
    const LedState desired = makeValidLed(60U);

    const AckMessage ack = LedControl::applyCommand(makeLedCommand("ack-led-auto", desired));

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AckResult::REJECTED),
        static_cast<uint8_t>(ack.result)
    );
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DeviceMode::AUTONOMOUS),
        static_cast<uint8_t>(LedControl::getCurrentMode())
    );
}

void test_led_control_mode_change_command_applies() {
    prepareLedControl(DeviceMode::AUTONOMOUS);

    const AckMessage ack = LedControl::applyCommand(
        makeModeCommand("ack-mode-manual", DeviceMode::MANUAL)
    );

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AckResult::APPLIED),
        static_cast<uint8_t>(ack.result)
    );
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DeviceMode::MANUAL),
        static_cast<uint8_t>(LedControl::getCurrentMode())
    );
}

void test_led_control_accepts_valid_led_command_in_manual_mode() {
    prepareLedControl(DeviceMode::MANUAL);
    const LedState desired = makeValidLed(72U);

    const AckMessage ack = LedControl::applyCommand(makeLedCommand("ack-led-manual", desired));
    const LedState actual = LedControl::getCurrentLedState();

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AckResult::APPLIED),
        static_cast<uint8_t>(ack.result)
    );
    assertLedEqual(desired, actual);
}

void test_led_control_buildStatusMessage_reflects_runtime_state() {
    prepareLedControl(DeviceMode::MANUAL);
    const LedState desired = makeValidLed(81U);

    const AckMessage ack = LedControl::applyCommand(makeLedCommand("ack-led-status", desired));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AckResult::APPLIED),
        static_cast<uint8_t>(ack.result)
    );

    const StatusMessage status = LedControl::buildStatusMessage();

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DeviceMode::MANUAL),
        static_cast<uint8_t>(status.mode)
    );
    TEST_ASSERT_EQUAL_FLOAT(makeValidThresholds().temp_max_c, status.thresholds.temp_max_c);
    TEST_ASSERT_EQUAL_FLOAT(DEFAULT_LUX_CEILING, status.lux_ceiling);
    assertLedEqual(desired, status.led);
}

void test_led_control_autonomous_holds_brightness_outside_thresholds() {
    prepareLedControl(DeviceMode::AUTONOMOUS);
    const LedState before = LedControl::getCurrentLedState();

    TEST_ASSERT_TRUE(LedControl::controlTick(makeReading(5000.0f, 35.0f)));

    const LedState after = LedControl::getCurrentLedState();
    assertLedEqual(before, after);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AutoState::PAUSED_OUT_OF_RANGE),
        static_cast<uint8_t>(LedControl::buildStatusMessage().auto_state)
    );
}

void test_led_control_autonomous_night_off_turns_lights_off() {
    prepareLedControl(DeviceMode::AUTONOMOUS);

    TEST_ASSERT_TRUE(LedControl::controlTick(makeReading(5.0f, 22.0f)));

    const StatusMessage status = LedControl::buildStatusMessage();
    TEST_ASSERT_EQUAL_UINT8(0U, status.led.brightness_pct);
    TEST_ASSERT_TRUE(status.led.power);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AutoState::NIGHT_OFF), static_cast<uint8_t>(status.auto_state)
    );
    TEST_ASSERT_FALSE(LedControl::controlTick(makeReading(15.0f, 22.0f)));
}

void test_led_control_ack_returns_on_command_source() {
    prepareLedControl(DeviceMode::MANUAL);

    CommandEnvelope local = makeModeCommand("ack-route-local", DeviceMode::MANUAL);
    local.source = CommandSource::LOCAL;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CommandSource::LOCAL),
        static_cast<uint8_t>(LedControl::applyCommand(local).source)
    );

    const CommandEnvelope cloud = makeModeCommand("ack-route-cloud", DeviceMode::MANUAL);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CommandSource::CLOUD),
        static_cast<uint8_t>(LedControl::applyCommand(cloud).source)
    );

    const AckMessage rejected = LedControl::buildRejectedAck(
        local.command_id, CommandSource::LOCAL, AckReason::INVALID_PAYLOAD
    );
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CommandSource::LOCAL), static_cast<uint8_t>(rejected.source)
    );
}

void test_led_control_config_command_sets_lux_ceiling() {
    prepareLedControl(DeviceMode::AUTONOMOUS);

    CommandEnvelope cmd{};
    TEST_ASSERT_TRUE(parseCommandId("cfg-lux", std::strlen("cfg-lux"), cmd.command_id));
    cmd.payload = ConfigCommand{
        requireValidThresholds(makeValidThresholds()), ValidLuxCeiling::parse(2000.0f)
    };

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AckResult::APPLIED),
        static_cast<uint8_t>(LedControl::applyCommand(cmd).result)
    );
    TEST_ASSERT_EQUAL_FLOAT(2000.0f, LedControl::buildStatusMessage().lux_ceiling);
    TEST_ASSERT_EQUAL_FLOAT(2000.0f, ConfigManager::getConfigSnapshot().lux_ceiling.raw());
}

void test_energy_tracker_init_succeeds() {
    prepareEnergyTracker(4.25f);
    const EnergyMessage snapshot = EnergyTracker::getSnapshot();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.25f, snapshot.total_wh);
}

void test_energy_tracker_init_resets_session_counters() {
    prepareEnergyTracker(7.0f);
    const EnergyMessage snapshot = EnergyTracker::getSnapshot();

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.0f, snapshot.total_wh);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, snapshot.session_wh);
    TEST_ASSERT_EQUAL_UINT32(0U, snapshot.light_on_sec);
    TEST_ASSERT_EQUAL_UINT32(0U, snapshot.max_brightness_sec);
}

void test_energy_tracker_led_off_does_not_increase_session_energy() {
    prepareEnergyTracker(1.5f);

    const EnergyMessage before = EnergyTracker::getSnapshot();
    delay(150);
    EnergyTracker::updateFromLedState(makeOffLed());
    const EnergyMessage after = EnergyTracker::getSnapshot();

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, before.total_wh, after.total_wh);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, before.session_wh, after.session_wh);
    TEST_ASSERT_EQUAL_UINT32(before.light_on_sec, after.light_on_sec);
    TEST_ASSERT_EQUAL_UINT32(before.max_brightness_sec, after.max_brightness_sec);
}

void test_energy_tracker_full_brightness_updates_session_and_max_counter() {
    prepareEnergyTracker(0.5f);
    const LedState led = makeValidLed(100U);

    const EnergyMessage before = EnergyTracker::getSnapshot();
    delay(1200);
    EnergyTracker::updateFromLedState(led);
    const EnergyMessage after = EnergyTracker::getSnapshot();

    TEST_ASSERT_TRUE(after.total_wh > before.total_wh);
    TEST_ASSERT_TRUE(after.session_wh > before.session_wh);
    TEST_ASSERT_TRUE(after.light_on_sec >= 1U);
    TEST_ASSERT_TRUE(after.max_brightness_sec >= 1U);
}

void test_wifi_manager_init_succeeds() {
    TEST_ASSERT_TRUE(WifiManager::init());
}

void test_mqtt_client_init_with_valid_device_id_succeeds() {
    TEST_ASSERT_TRUE(MqttClient::init("testnode", testCloudConfig()));
}

#include <string.h>
#include <unity.h>

#include <variant>

#include "../fixtures/contract/contract_fixtures.h"
#include "lumen_board_config.h"
#include "lumen_command_handler.h"
#include "lumen_type_validation.h"
#include "lumen_wire.h"

namespace {
const LedState FIXTURE_LED = {true, 72, true, true, false, 60, 30, 10};
const char* const FIXTURE_COMMAND_ID = "7c9e6679-7425-40de-944b-e07fc1f90ae7";

CommandId commandId(const char* text) {
    CommandId id;
    TEST_ASSERT_TRUE(parseCommandId(text, strlen(text), id));
    return id;
}

void assertEncodedAs(const uint8_t* expected, size_t expected_len, const uint8_t* actual, size_t actual_len) {
    TEST_ASSERT_EQUAL_UINT(expected_len, actual_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, expected_len);
}

CommandEnvelope decodeFixture(CommandKind kind, const uint8_t* bytes, size_t length) {
    CommandEnvelope command = {};
    const CommandHandler::DecodeStatus status =
        CommandHandler::decode(kind, bytes, static_cast<uint16_t>(length), command);
    TEST_ASSERT_EQUAL(static_cast<int>(CommandHandler::DecodeStatus::DECODED), static_cast<int>(status));
    TEST_ASSERT_EQUAL_STRING(FIXTURE_COMMAND_ID, command.command_id.c_str());
    return command;
}
}  // namespace

void test_wire_telemetry_matches_fixture() {
    const SensorReading reading = {123456, 412.5f, 23.5f, 61.25f, 42};
    uint8_t out[TELEMETRY_PAYLOAD_MAX] = {};
    const size_t length = WireCodec::encodeTelemetry(reading, out, sizeof(out));
    assertEncodedAs(FIXTURE_TELEMETRY, FIXTURE_TELEMETRY_LEN, out, length);
}

void test_wire_status_matches_fixture() {
    const StatusMessage status = {
        123500,
        DeviceMode::MANUAL,
        FIXTURE_LED,
        DegradedTier::NONE,
        123,
        AutoState::NIGHT_OFF,
        CloudState::NOT_CONFIGURED,
        {18.5f, 28.5f, 55.5f, 80.5f},
        9500.5f,
    };
    uint8_t out[STATUS_PAYLOAD_MAX] = {};
    const size_t length = WireCodec::encodeStatus(status, out, sizeof(out));
    assertEncodedAs(FIXTURE_STATUS, FIXTURE_STATUS_LEN, out, length);
}

void test_wire_energy_matches_fixture() {
    const EnergyMessage energy = {130000, 12.5f, 0.75f, 3600, 120};
    uint8_t out[ENERGY_PAYLOAD_MAX] = {};
    const size_t length = WireCodec::encodeEnergy(energy, out, sizeof(out));
    assertEncodedAs(FIXTURE_ENERGY, FIXTURE_ENERGY_LEN, out, length);
}

void test_wire_ack_matches_fixture() {
    AckMessage ack = {};
    ack.command_id = commandId("0f8fad5b-d9cb-469f-a165-70867728950e");
    ack.result = AckResult::REJECTED;
    ack.timestamp_ms = 123600;
    ack.mode = DeviceMode::AUTONOMOUS;
    ack.led = FIXTURE_LED;
    ack.reason = AckReason::MANUAL_LOCKED;
    uint8_t out[ACK_PAYLOAD_MAX] = {};
    const size_t length = WireCodec::encodeAck(ack, out, sizeof(out));
    assertEncodedAs(FIXTURE_ACK, FIXTURE_ACK_LEN, out, length);
}

void test_wire_encode_reports_overflow() {
    const SensorReading reading = {123456, 412.5f, 23.5f, 61.25f, 42};
    uint8_t out[16] = {};
    TEST_ASSERT_EQUAL_UINT(0, WireCodec::encodeTelemetry(reading, out, sizeof(out)));
}

void test_wire_frame_matches_fixture() {
    uint8_t out[TELEMETRY_PAYLOAD_MAX + 16] = {};
    const size_t length = WireCodec::encodeFrame(
        "telemetry", FIXTURE_TELEMETRY, FIXTURE_TELEMETRY_LEN, out, sizeof(out)
    );
    assertEncodedAs(FIXTURE_FRAME_TELEMETRY, FIXTURE_FRAME_TELEMETRY_LEN, out, length);
}

void test_wire_frame_decodes_command() {
    WireCodec::InboundFrame frame = {};
    const WireCodec::FrameStatus status =
        WireCodec::decodeFrame(FIXTURE_FRAME_COMMAND_MODE, FIXTURE_FRAME_COMMAND_MODE_LEN, frame);
    TEST_ASSERT_EQUAL(static_cast<int>(WireCodec::FrameStatus::COMMAND), static_cast<int>(status));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandKind::MODE_CHANGE), static_cast<int>(frame.kind));
    assertEncodedAs(FIXTURE_COMMAND_MODE, FIXTURE_COMMAND_MODE_LEN, frame.payload, frame.length);
}

void test_wire_frame_rejects_unknown_suffix_and_bad_bytes() {
    uint8_t out[64] = {};
    const uint8_t payload[] = {0x80};
    const size_t length = WireCodec::encodeFrame("command/other", payload, sizeof(payload), out, sizeof(out));
    WireCodec::InboundFrame frame = {};
    TEST_ASSERT_EQUAL(
        static_cast<int>(WireCodec::FrameStatus::UNKNOWN_SUFFIX),
        static_cast<int>(WireCodec::decodeFrame(out, length, frame))
    );

    const uint8_t not_msgpack[] = {0xc1};
    TEST_ASSERT_EQUAL(
        static_cast<int>(WireCodec::FrameStatus::MALFORMED),
        static_cast<int>(WireCodec::decodeFrame(not_msgpack, sizeof(not_msgpack), frame))
    );

    TEST_ASSERT_EQUAL(
        static_cast<int>(WireCodec::FrameStatus::MALFORMED),
        static_cast<int>(WireCodec::decodeFrame(FIXTURE_TELEMETRY, FIXTURE_TELEMETRY_LEN, frame))
    );
}

void test_wire_frame_rejects_oversized_payload() {
    static uint8_t payload[COMMAND_PAYLOAD_MAX_BYTES + 1] = {};
    static uint8_t out[COMMAND_PAYLOAD_MAX_BYTES + 64] = {};
    const size_t length = WireCodec::encodeFrame("command/led", payload, sizeof(payload), out, sizeof(out));
    WireCodec::InboundFrame frame = {};
    TEST_ASSERT_EQUAL(
        static_cast<int>(WireCodec::FrameStatus::OVERSIZED),
        static_cast<int>(WireCodec::decodeFrame(out, length, frame))
    );
}

void test_contract_led_command_fixture_decodes() {
    const CommandEnvelope command =
        decodeFixture(CommandKind::LED_CONTROL, FIXTURE_COMMAND_LED, FIXTURE_COMMAND_LED_LEN);
    const LedState& led = std::get<LedCommand>(command.payload).desired_led.raw();
    TEST_ASSERT_EQUAL_UINT8(72, led.brightness_pct);
    TEST_ASSERT_FALSE(led.far_red_enabled);
    TEST_ASSERT_EQUAL_UINT8(60, led.red_dist_pct);
}

void test_contract_config_command_fixture_decodes() {
    const CommandEnvelope command =
        decodeFixture(CommandKind::CONFIG_UPDATE, FIXTURE_COMMAND_CONFIG, FIXTURE_COMMAND_CONFIG_LEN);
    const ConfigCommand& config = std::get<ConfigCommand>(command.payload);
    TEST_ASSERT_EQUAL_FLOAT(18.5f, config.desired_thresholds.raw().temp_min_c);
    TEST_ASSERT_EQUAL_FLOAT(28.0f, config.desired_thresholds.raw().temp_max_c);
    TEST_ASSERT_TRUE(config.desired_lux_ceiling.has_value());
    TEST_ASSERT_EQUAL_FLOAT(8000.0f, config.desired_lux_ceiling->raw());
}

void test_contract_mode_command_fixture_decodes() {
    const CommandEnvelope command =
        decodeFixture(CommandKind::MODE_CHANGE, FIXTURE_COMMAND_MODE, FIXTURE_COMMAND_MODE_LEN);
    TEST_ASSERT_EQUAL(
        static_cast<int>(DeviceMode::MANUAL),
        static_cast<int>(std::get<ModeCommand>(command.payload).desired_mode)
    );
}

void test_contract_read_command_id_for_unknown_kind() {
    CommandId id;
    TEST_ASSERT_TRUE(CommandHandler::readCommandId(FIXTURE_COMMAND_MODE, FIXTURE_COMMAND_MODE_LEN, id));
    TEST_ASSERT_EQUAL_STRING(FIXTURE_COMMAND_ID, id.c_str());
}

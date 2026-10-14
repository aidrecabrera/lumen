#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <variant>

static constexpr size_t DEVICE_ID_MAX_LEN = 24U;
static constexpr size_t COMMAND_ID_MAX_LEN = 37U;

enum class DeviceMode : uint8_t { AUTONOMOUS = 0, MANUAL = 1 };

enum class CommandKind : uint8_t { LED_CONTROL = 0, CONFIG_UPDATE = 1, MODE_CHANGE = 2 };

enum class AckResult : uint8_t { APPLIED = 0, REJECTED = 1, DEFERRED = 2 };

// Why an ack has its result. The wire carries the text from ackReasonText().
enum class AckReason : uint8_t {
    MODE_APPLIED,
    CONFIG_APPLIED,
    LED_APPLIED,
    NOT_READY,
    PERSIST_FAILED,
    MANUAL_LOCKED,
    INVALID_PAYLOAD,
    COMMAND_QUEUE_FULL,
};

// The "reason" text in the ack payload. The switch has no default, so -Wswitch flags a new value.
inline const char* ackReasonText(AckReason reason) {
    switch (reason) {
        case AckReason::MODE_APPLIED:
            return "mode applied";
        case AckReason::CONFIG_APPLIED:
            return "config applied";
        case AckReason::LED_APPLIED:
            return "led applied";
        case AckReason::NOT_READY:
            return "not ready";
        case AckReason::PERSIST_FAILED:
            return "persist failed";
        case AckReason::MANUAL_LOCKED:
            return "manual locked";
        case AckReason::INVALID_PAYLOAD:
            return "invalid payload";
        case AckReason::COMMAND_QUEUE_FULL:
            return "command queue full";
    }

    return "unknown";
}

class CommandId;
bool parseCommandId(const char* text, size_t length, CommandId& out_id);

// A command id that is safe to use as the last level of an MQTT topic.
// Only parseCommandId() (lumen_type_validation) writes one. A default CommandId is empty and
// is only a receive buffer.
class CommandId {
   public:
    const char* c_str() const { return text_; }

   private:
    friend bool parseCommandId(const char* text, size_t length, CommandId& out_id);

    char text_[COMMAND_ID_MAX_LEN] = {};
};

// Wi-Fi link health, published as the status "degraded_tier" byte.
enum class DegradedTier : uint8_t {
    NONE = 0,              // no failed connect attempt since the last good link
    WIFI_RETRYING = 1,     // fewer than WIFI_FAILURE_THRESHOLD failed attempts
    WIFI_RETRY_LIMIT = 2,  // WIFI_FAILURE_THRESHOLD or more failed attempts
};

struct ThresholdConfig {
    float temp_min_c;
    float temp_max_c;
    float humidity_min_pct;
    float humidity_max_pct;
};

struct LedState {
    bool power;
    uint8_t brightness_pct;
    bool red_enabled;
    bool blue_enabled;
    bool far_red_enabled;
    uint8_t red_dist_pct;
    uint8_t blue_dist_pct;
    uint8_t far_red_dist_pct;
};

// The Valid* types below hold a value that passed its parse(). Only parse() (implemented in
// lumen_type_validation.cpp) creates one, so code that takes a Valid* type needs no re-check.
// raw() gives the plain struct for encode and persist.

// All four values finite, temp_min_c <= temp_max_c, humidity_min_pct <= humidity_max_pct.
class ValidThresholds {
   public:
    static std::optional<ValidThresholds> parse(const ThresholdConfig& raw);
    const ThresholdConfig& raw() const { return raw_; }

   private:
    explicit ValidThresholds(const ThresholdConfig& raw) : raw_(raw) {}

    ThresholdConfig raw_;
};

// brightness_pct <= 100 and the three distribution percents sum to 100.
class ValidLedState {
   public:
    static std::optional<ValidLedState> parse(const LedState& raw);
    const LedState& raw() const { return raw_; }

   private:
    explicit ValidLedState(const LedState& raw) : raw_(raw) {}

    LedState raw_;
};

// Finite and >= 0.
class EnergyTotalWh {
   public:
    static std::optional<EnergyTotalWh> parse(float raw_wh);
    float raw() const { return raw_wh_; }

   private:
    explicit EnergyTotalWh(float raw_wh) : raw_wh_(raw_wh) {}

    float raw_wh_;
};

struct RuntimeConfig {
    char device_id[DEVICE_ID_MAX_LEN];
    DeviceMode mode;
    ValidThresholds thresholds;
    ValidLedState led;
    EnergyTotalWh energy_total_wh;
};

struct SensorReading {
    uint64_t timestamp_ms;
    float light_lux;
    float temperature_c;
    float humidity_pct;
    uint32_t sequence;
};

struct StatusMessage {
    uint64_t timestamp_ms;
    DeviceMode mode;
    LedState led;
    DegradedTier degraded_tier;
    uint32_t uptime_sec;
};

struct LedCommand {
    ValidLedState desired_led;
};

struct ConfigCommand {
    ValidThresholds desired_thresholds;
};

struct ModeCommand {
    DeviceMode desired_mode;
};

// Each command kind carries only its own payload. ModeCommand is first because a default
// std::variant holds its first alternative, and only ModeCommand has a default value.
using CommandPayload = std::variant<ModeCommand, LedCommand, ConfigCommand>;

struct CommandEnvelope {
    CommandId command_id;
    CommandPayload payload;
};

struct EnergyMessage {
    uint64_t timestamp_ms;
    float total_wh;
    float session_wh;
    uint32_t light_on_sec;
    uint32_t max_brightness_sec;
};

struct AckMessage {
    CommandId command_id;
    AckResult result;
    uint64_t timestamp_ms;
    DeviceMode mode;
    LedState led;
    AckReason reason;
};
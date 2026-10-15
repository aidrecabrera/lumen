#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <variant>

static constexpr size_t DEVICE_ID_MAX_LEN = 24U;
static constexpr size_t COMMAND_ID_MAX_LEN = 37U;
static constexpr size_t COMMAND_PAYLOAD_MAX_BYTES = 256U;

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

enum class AutoState : uint8_t { ACTIVE, NIGHT_OFF, PAUSED_OUT_OF_RANGE };

inline const char* autoStateText(AutoState state) {
    switch (state) {
        case AutoState::ACTIVE:
            return "active";
        case AutoState::NIGHT_OFF:
            return "night_off";
        case AutoState::PAUSED_OUT_OF_RANGE:
            return "paused_out_of_range";
    }

    return "unknown";
}

enum class CloudState : uint8_t { CONNECTED, CONNECTING, NOT_CONFIGURED, INVALID_CONFIG };

inline const char* cloudStateText(CloudState state) {
    switch (state) {
        case CloudState::CONNECTED:
            return "connected";
        case CloudState::CONNECTING:
            return "connecting";
        case CloudState::NOT_CONFIGURED:
            return "not_configured";
        case CloudState::INVALID_CONFIG:
            return "invalid_config";
    }

    return "unknown";
}

// an ack goes back on the path its command came from.
enum class CommandSource : uint8_t { CLOUD, LOCAL };

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

// lux where auto brightness reaches zero, LUX_CEILING_MIN..LUX_CEILING_MAX.
class ValidLuxCeiling {
   public:
    static std::optional<ValidLuxCeiling> parse(float raw_lux);
    float raw() const { return raw_lux_; }

   private:
    explicit ValidLuxCeiling(float raw_lux) : raw_lux_(raw_lux) {}

    float raw_lux_;
};

static constexpr size_t WIFI_SSID_MAX_LEN = 32U;
static constexpr size_t WIFI_PASSWORD_MAX_LEN = 63U;
static constexpr size_t LOCAL_PASSWORD_MIN_LEN = 8U;
static constexpr size_t LOCAL_PASSWORD_HASH_LEN = 32U;

// ssid 1..32 bytes; password empty (open network) or 8..63 bytes, as wpa2 allows.
class ValidWifiCredentials {
   public:
    static std::optional<ValidWifiCredentials> parse(const char* ssid, const char* password);
    const char* ssid() const { return ssid_; }
    const char* password() const { return password_; }

   private:
    ValidWifiCredentials() = default;

    char ssid_[WIFI_SSID_MAX_LEN + 1] = {};
    char password_[WIFI_PASSWORD_MAX_LEN + 1] = {};
};

// md5 digest-auth hash of the local password, 32 lowercase hex characters.
class LocalPasswordHash {
   public:
    static std::optional<LocalPasswordHash> parse(const char* hex);
    const char* c_str() const { return hex_; }

   private:
    LocalPasswordHash() = default;

    char hex_[LOCAL_PASSWORD_HASH_LEN + 1] = {};
};

struct RuntimeConfig {
    char device_id[DEVICE_ID_MAX_LEN];
    DeviceMode mode;
    ValidThresholds thresholds;
    ValidLedState led;
    EnergyTotalWh energy_total_wh;
    ValidLuxCeiling lux_ceiling;
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
    AutoState auto_state;
    CloudState cloud;
    ThresholdConfig thresholds;
    float lux_ceiling;
};

struct LedCommand {
    ValidLedState desired_led;
};

// optional, so a config command without it keeps the current ceiling.
struct ConfigCommand {
    ValidThresholds desired_thresholds;
    std::optional<ValidLuxCeiling> desired_lux_ceiling;
};

struct ModeCommand {
    DeviceMode desired_mode;
};

// Each command kind carries only its own payload. ModeCommand is first because a default
// std::variant holds its first alternative, and only ModeCommand has a default value.
using CommandPayload = std::variant<ModeCommand, LedCommand, ConfigCommand>;

struct CommandEnvelope {
    CommandId command_id;
    CommandSource source;
    CommandPayload payload;
};

using InboundCommandFn =
    void (*)(CommandSource source, CommandKind kind, const uint8_t* payload, uint16_t length);

struct EnergyMessage {
    uint64_t timestamp_ms;
    float total_wh;
    float session_wh;
    uint32_t light_on_sec;
    uint32_t max_brightness_sec;
};

struct AckMessage {
    CommandId command_id;
    CommandSource source;
    AckResult result;
    uint64_t timestamp_ms;
    DeviceMode mode;
    LedState led;
    AckReason reason;
};
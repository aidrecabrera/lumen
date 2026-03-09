#pragma once

#include <stdbool.h>
#include <stdint.h>

static constexpr uint8_t DEVICE_ID_MAX_LEN = 24;
static constexpr uint8_t COMMAND_ID_MAX_LEN = 37;
static constexpr uint8_t REASON_MAX_LEN = 48;

enum class DeviceMode : uint8_t { AUTONOMOUS = 0, MANUAL = 1 };

enum class CommandKind : uint8_t { LED_CONTROL = 0, CONFIG_UPDATE = 1, MODE_CHANGE = 2 };

enum class AckResult : uint8_t { APPLIED = 0, REJECTED = 1, DEFERRED = 2 };

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

struct RuntimeConfig {
    char device_id[DEVICE_ID_MAX_LEN];
    DeviceMode mode;
    ThresholdConfig thresholds;
    LedState led;
    float energy_total_wh;
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
    uint8_t degraded_tier;
    uint32_t uptime_sec;
};

struct CommandEnvelope {
    char command_id[COMMAND_ID_MAX_LEN];
    CommandKind kind;
    DeviceMode desired_mode;
    LedState desired_led;
    ThresholdConfig desired_thresholds;
    uint64_t received_timestamp_ms;
};

struct EnergyMessage {
    uint64_t timestamp_ms;
    float total_wh;
    float session_wh;
    uint32_t light_on_sec;
    uint32_t max_brightness_sec;
};

struct AckMessage {
    char command_id[COMMAND_ID_MAX_LEN];
    AckResult result;
    uint64_t timestamp_ms;
    DeviceMode mode;
    LedState led;
    char reason[REASON_MAX_LEN];
};

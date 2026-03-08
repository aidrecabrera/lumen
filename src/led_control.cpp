#include "led_control.h"

#include <math.h>
#include <string.h>

#include <Adafruit_NeoPixel.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "config.h"
#include "config_manager.h"

namespace {
static const char* TAG = "led_control";

static Adafruit_NeoPixel led_strip(
    LED_PIXEL_COUNT,
    LED_DATA_PIN,
    NEO_GRB + NEO_KHZ800);

static bool is_initialized = false;
static LedState current_led = {};
static DeviceMode current_mode = DeviceMode::AUTONOMOUS;
static ThresholdConfig current_thresholds = {};


uint64_t getNowMs()
{
    return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
}


uint32_t getUptimeSec()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000000ULL);
}


bool isThresholdConfigValid(const ThresholdConfig& thresholds)
{
    if (!isfinite(thresholds.temp_min_c) ||
        !isfinite(thresholds.temp_max_c) ||
        !isfinite(thresholds.humidity_min_pct) ||
        !isfinite(thresholds.humidity_max_pct)) {
        return false;
    }

    if (thresholds.temp_min_c > thresholds.temp_max_c) {
        return false;
    }

    if (thresholds.humidity_min_pct > thresholds.humidity_max_pct) {
        return false;
    }

    return true;
}


bool isLedStateValid(const LedState& led)
{
    uint16_t total_pct = static_cast<uint16_t>(led.red_dist_pct) +
        static_cast<uint16_t>(led.blue_dist_pct) +
        static_cast<uint16_t>(led.far_red_dist_pct);

    if (led.brightness_pct > 100) {
        return false;
    }

    if (total_pct != 100) {
        return false;
    }

    return true;
}


uint8_t scaleChannel(uint8_t brightness_pct, uint8_t channel_pct)
{
    uint16_t scaled = static_cast<uint16_t>(brightness_pct) *
        static_cast<uint16_t>(channel_pct) * 255U;

    return static_cast<uint8_t>(scaled / 10000U);
}


uint8_t clampBrightness(float brightness_pct)
{
    if (brightness_pct <= 0.0f) {
        return 0;
    }

    if (brightness_pct >= 100.0f) {
        return 100;
    }

    return static_cast<uint8_t>(brightness_pct + 0.5f);
}


uint8_t computeAutonomousBrightness(float light_lux)
{
    if (!isfinite(light_lux) || light_lux <= 0.0f) {
        return 100;
    }

    if (light_lux >= AUTONOMOUS_LUX_CEILING) {
        return 0;
    }

    float ratio = 1.0f - (light_lux / AUTONOMOUS_LUX_CEILING);
    return clampBrightness(ratio * 100.0f);
}


void copyCommandId(char* dest, const char* src)
{
    if (src == nullptr) {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, COMMAND_ID_MAX_LEN - 1);
    dest[COMMAND_ID_MAX_LEN - 1] = '\0';
}


void copyReason(char* dest, const char* src)
{
    if (src == nullptr) {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, REASON_MAX_LEN - 1);
    dest[REASON_MAX_LEN - 1] = '\0';
}


AckMessage buildAck(
    const char* command_id,
    AckResult result,
    const char* reason)
{
    AckMessage ack = {};
    copyCommandId(ack.command_id, command_id);
    ack.result = result;
    ack.timestamp_ms = getNowMs();
    ack.mode = current_mode;
    ack.led = current_led;
    copyReason(ack.reason, reason);
    return ack;
}


void showOff()
{
    led_strip.clear();
    led_strip.show();
}


void applyToHardware(const LedState& led)
{
    if (!led.power || led.brightness_pct == 0) {
        showOff();
        return;
    }

    uint8_t red_pct = led.red_enabled ? led.red_dist_pct : 0;
    uint8_t blue_pct = led.blue_enabled ? led.blue_dist_pct : 0;
    uint8_t far_red_pct = led.far_red_enabled ? led.far_red_dist_pct : 0;

    // Intentional deviation: far-red is approximated on RGB LEDs as red.
    uint8_t red_level = scaleChannel(
        led.brightness_pct,
        static_cast<uint8_t>(red_pct + far_red_pct));
    uint8_t blue_level = scaleChannel(led.brightness_pct, blue_pct);

    uint32_t pixel_color = led_strip.Color(red_level, 0, blue_level);

    for (uint16_t pixel_idx = 0; pixel_idx < LED_PIXEL_COUNT; ++pixel_idx) {
        led_strip.setPixelColor(pixel_idx, pixel_color);
    }

    led_strip.show();
}


void loadPersistedState()
{
    const RuntimeConfig& config = ConfigManager::getConfig();
    current_mode = config.mode;
    current_led = config.led;
    current_thresholds = config.thresholds;
}


AckMessage rejectCommand(const CommandEnvelope& command, const char* reason)
{
    return buildAck(command.command_id, AckResult::REJECTED, reason);
}


AckMessage applyModeChange(const CommandEnvelope& command)
{
    current_mode = command.desired_mode;

    bool was_saved = ConfigManager::saveMode(current_mode);
    if (!was_saved) {
        ESP_LOGW(TAG, "mode save failed");
    }

    return buildAck(command.command_id, AckResult::APPLIED, "");
}


AckMessage applyConfigChange(const CommandEnvelope& command)
{
    if (!isThresholdConfigValid(command.desired_thresholds)) {
        return rejectCommand(command, "invalid thresholds");
    }

    current_thresholds = command.desired_thresholds;

    bool was_saved = ConfigManager::saveThresholds(current_thresholds);
    if (!was_saved) {
        ESP_LOGW(TAG, "thresholds save failed");
    }

    return buildAck(command.command_id, AckResult::APPLIED, "");
}


AckMessage applyLedChange(const CommandEnvelope& command)
{
    if (!isLedStateValid(command.desired_led)) {
        return rejectCommand(command, "invalid led state");
    }

    current_led = command.desired_led;
    applyToHardware(current_led);

    bool was_saved = ConfigManager::saveLedState(current_led);
    if (!was_saved) {
        ESP_LOGW(TAG, "led state save failed");
    }

    return buildAck(command.command_id, AckResult::APPLIED, "");
}
}  // namespace


namespace LedControl {
bool init()
{
    loadPersistedState();

    if (!isLedStateValid(current_led)) {
        ESP_LOGE(TAG, "persisted led invalid");
        return false;
    }

    if (!isThresholdConfigValid(current_thresholds)) {
        ESP_LOGE(TAG, "persisted thresholds invalid");
        return false;
    }

    led_strip.begin();
    applyToHardware(current_led);

    is_initialized = true;
    ESP_LOGI(TAG, "led control initialized");
    return true;
}


AckMessage applyCommand(const CommandEnvelope& command)
{
    if (!is_initialized) {
        return buildAck(command.command_id, AckResult::REJECTED, "not ready");
    }

    if (command.kind == CommandKind::MODE_CHANGE) {
        return applyModeChange(command);
    }

    if (command.kind == CommandKind::CONFIG_UPDATE) {
        return applyConfigChange(command);
    }

    if (command.kind == CommandKind::LED_CONTROL) {
        return applyLedChange(command);
    }

    return rejectCommand(command, "bad command");
}


void controlTick(const SensorReading& latest_reading)
{
    if (!is_initialized) {
        return;
    }

    if (current_mode != DeviceMode::AUTONOMOUS) {
        return;
    }

    if (!current_led.power) {
        return;
    }

    uint8_t next_brightness_pct = computeAutonomousBrightness(
        latest_reading.light_lux);

    if (next_brightness_pct == current_led.brightness_pct) {
        return;
    }

    current_led.brightness_pct = next_brightness_pct;
    applyToHardware(current_led);
}


const LedState& getCurrentLedState()
{
    return current_led;
}


DeviceMode getCurrentMode()
{
    return current_mode;
}


StatusMessage buildStatusMessage(uint8_t degraded_tier)
{
    StatusMessage status = {};
    status.timestamp_ms = getNowMs();
    status.mode = current_mode;
    status.led = current_led;
    status.degraded_tier = degraded_tier;
    status.uptime_sec = getUptimeSec();
    return status;
}
}  // namespace LedControl

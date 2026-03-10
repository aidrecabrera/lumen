#include "led_control.h"

#include <Adafruit_NeoPixel.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>
#include <string.h>

#include "config.h"
#include "config_manager.h"

namespace {
static const char* TAG = "led_control";

static Adafruit_NeoPixel led_strip(LED_PIXEL_COUNT, LED_DATA_PIN, NEO_GRB + NEO_KHZ800);

static bool is_initialized = false;
static LedState current_led = {};
static DeviceMode current_mode = DeviceMode::AUTONOMOUS;
static ThresholdConfig current_thresholds = {};

static StaticSemaphore_t state_mutex_storage;
static SemaphoreHandle_t state_mutex = nullptr;

uint64_t getNowMs() { return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL); }

uint32_t getUptimeSec() { return static_cast<uint32_t>(esp_timer_get_time() / 1000000ULL); }

bool ensureStateMutex() {
    if (state_mutex != nullptr) {
        return true;
    }

    state_mutex = xSemaphoreCreateMutexStatic(&state_mutex_storage);
    return state_mutex != nullptr;
}

bool lockState(TickType_t timeout_ticks = portMAX_DELAY) {
    return (state_mutex != nullptr) && (xSemaphoreTake(state_mutex, timeout_ticks) == pdTRUE);
}

void unlockState() {
    if (state_mutex != nullptr) {
        xSemaphoreGive(state_mutex);
    }
}

bool copyText(char* dest, size_t dest_len, const char* src) {
    if (dest == nullptr || dest_len == 0U || src == nullptr) {
        return false;
    }

    const size_t src_len = strlen(src);
    if (src_len >= dest_len) {
        dest[0] = '\0';
        return false;
    }

    memcpy(dest, src, src_len);
    dest[src_len] = '\0';
    return true;
}

bool isThresholdConfigValid(const ThresholdConfig& thresholds) {
    if (!isfinite(thresholds.temp_min_c) || !isfinite(thresholds.temp_max_c) ||
        !isfinite(thresholds.humidity_min_pct) || !isfinite(thresholds.humidity_max_pct)) {
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

bool isLedStateValid(const LedState& led) {
    const uint16_t total_pct = static_cast<uint16_t>(led.red_dist_pct) +
                               static_cast<uint16_t>(led.blue_dist_pct) +
                               static_cast<uint16_t>(led.far_red_dist_pct);

    if (led.brightness_pct > 100U) {
        return false;
    }

    if (total_pct != 100U) {
        return false;
    }

    return true;
}

bool isReadingWithinThresholds(
    const SensorReading& reading, const ThresholdConfig& thresholds
) {
    if (!isfinite(reading.temperature_c) || !isfinite(reading.humidity_pct)) {
        return false;
    }

    return (reading.temperature_c >= thresholds.temp_min_c) &&
           (reading.temperature_c <= thresholds.temp_max_c) &&
           (reading.humidity_pct >= thresholds.humidity_min_pct) &&
           (reading.humidity_pct <= thresholds.humidity_max_pct);
}

uint8_t scaleChannel(uint8_t brightness_pct, uint8_t channel_pct) {
    const uint16_t scaled =
        static_cast<uint16_t>(brightness_pct) * static_cast<uint16_t>(channel_pct) * 255U;

    return static_cast<uint8_t>(scaled / 10000U);
}

uint8_t clampBrightness(float brightness_pct) {
    if (brightness_pct <= 0.0f) {
        return 0U;
    }

    if (brightness_pct >= 100.0f) {
        return 100U;
    }

    return static_cast<uint8_t>(brightness_pct + 0.5f);
}

uint8_t computeAutonomousBrightness(float light_lux) {
    if (!isfinite(light_lux) || light_lux <= 0.0f) {
        return 100U;
    }

    if (light_lux >= AUTONOMOUS_LUX_CEILING) {
        return 0U;
    }

    const float ratio = 1.0f - (light_lux / AUTONOMOUS_LUX_CEILING);
    return clampBrightness(ratio * 100.0f);
}

void showOff() {
    led_strip.clear();
    led_strip.show();
}

void applyToHardware(const LedState& led) {
    if (!led.power || led.brightness_pct == 0U) {
        showOff();
        return;
    }

    const uint8_t red_pct = led.red_enabled ? led.red_dist_pct : 0U;
    const uint8_t blue_pct = led.blue_enabled ? led.blue_dist_pct : 0U;
    const uint8_t far_red_pct = led.far_red_enabled ? led.far_red_dist_pct : 0U;

    // Intentional deviation: far-red is approximated on RGB LEDs as red.
    const uint8_t red_level =
        scaleChannel(led.brightness_pct, static_cast<uint8_t>(red_pct + far_red_pct));
    const uint8_t blue_level = scaleChannel(led.brightness_pct, blue_pct);

    const uint32_t pixel_color = led_strip.Color(red_level, 0U, blue_level);

    for (uint16_t pixel_idx = 0; pixel_idx < LED_PIXEL_COUNT; ++pixel_idx) {
        led_strip.setPixelColor(pixel_idx, pixel_color);
    }

    led_strip.show();
}

void loadPersistedState() {
    const RuntimeConfig& config = ConfigManager::getConfig();
    current_mode = config.mode;
    current_led = config.led;
    current_thresholds = config.thresholds;
}

bool persistRuntimeState(
    DeviceMode mode, const LedState& led, const ThresholdConfig& thresholds
) {
    RuntimeConfig config = ConfigManager::getConfig();
    config.mode = mode;
    config.led = led;
    config.thresholds = thresholds;
    return ConfigManager::saveRuntimeConfig(config);
}

void snapshotState(DeviceMode& mode, LedState& led, ThresholdConfig* thresholds = nullptr) {
    mode = DeviceMode::AUTONOMOUS;
    memset(&led, 0, sizeof(led));

    if (thresholds != nullptr) {
        memset(thresholds, 0, sizeof(*thresholds));
    }

    if (!lockState(pdMS_TO_TICKS(100))) {
        ESP_LOGW(TAG, "state lock timeout");
        return;
    }

    mode = current_mode;
    led = current_led;
    if (thresholds != nullptr) {
        *thresholds = current_thresholds;
    }

    unlockState();
}

AckMessage buildAck(
    const char* command_id,
    AckResult result,
    const char* reason,
    DeviceMode mode,
    const LedState& led
) {
    AckMessage ack = {};
    copyText(ack.command_id, sizeof(ack.command_id), command_id != nullptr ? command_id : "");
    ack.result = result;
    ack.timestamp_ms = getNowMs();
    ack.mode = mode;
    ack.led = led;
    copyText(ack.reason, sizeof(ack.reason), reason != nullptr ? reason : "");
    return ack;
}

AckMessage buildAckFromCurrent(const char* command_id, AckResult result, const char* reason) {
    DeviceMode mode = DeviceMode::AUTONOMOUS;
    LedState led = {};
    snapshotState(mode, led);
    return buildAck(command_id, result, reason, mode, led);
}

AckMessage rejectCommand(const CommandEnvelope& command, const char* reason) {
    ESP_LOGW(TAG, "command rejected: %s", reason);
    return buildAckFromCurrent(command.command_id, AckResult::REJECTED, reason);
}

AckMessage applyModeChange(const CommandEnvelope& command) {
    LedState led_snapshot = {};
    ThresholdConfig thresholds_snapshot = {};
    DeviceMode mode_snapshot = DeviceMode::AUTONOMOUS;
    snapshotState(mode_snapshot, led_snapshot, &thresholds_snapshot);

    const DeviceMode next_mode = command.desired_mode;
    if (!persistRuntimeState(next_mode, led_snapshot, thresholds_snapshot)) {
        return rejectCommand(command, "persist failed");
    }

    if (!lockState(pdMS_TO_TICKS(100))) {
        return rejectCommand(command, "state lock failed");
    }

    current_mode = next_mode;
    unlockState();

    ESP_LOGI(TAG, "mode changed");
    return buildAck(command.command_id, AckResult::APPLIED, "mode applied", next_mode, led_snapshot);
}

AckMessage applyConfigChange(const CommandEnvelope& command) {
    if (!isThresholdConfigValid(command.desired_thresholds)) {
        return rejectCommand(command, "bad thresholds");
    }

    LedState led_snapshot = {};
    ThresholdConfig thresholds_snapshot = {};
    DeviceMode mode_snapshot = DeviceMode::AUTONOMOUS;
    snapshotState(mode_snapshot, led_snapshot, &thresholds_snapshot);

    if (!persistRuntimeState(mode_snapshot, led_snapshot, command.desired_thresholds)) {
        return rejectCommand(command, "persist failed");
    }

    if (!lockState(pdMS_TO_TICKS(100))) {
        return rejectCommand(command, "state lock failed");
    }

    current_thresholds = command.desired_thresholds;
    unlockState();

    return buildAck(
        command.command_id,
        AckResult::APPLIED,
        "config applied",
        mode_snapshot,
        led_snapshot
    );
}

AckMessage applyLedChange(const CommandEnvelope& command) {
    if (!isLedStateValid(command.desired_led)) {
        return rejectCommand(command, "bad led state");
    }

    LedState led_snapshot = {};
    ThresholdConfig thresholds_snapshot = {};
    DeviceMode mode_snapshot = DeviceMode::AUTONOMOUS;
    snapshotState(mode_snapshot, led_snapshot, &thresholds_snapshot);

    if (mode_snapshot == DeviceMode::AUTONOMOUS) {
        return rejectCommand(command, "manual locked");
    }

    if (!persistRuntimeState(mode_snapshot, command.desired_led, thresholds_snapshot)) {
        return rejectCommand(command, "persist failed");
    }

    if (!lockState(pdMS_TO_TICKS(100))) {
        return rejectCommand(command, "state lock failed");
    }

    current_led = command.desired_led;
    unlockState();

    applyToHardware(command.desired_led);
    return buildAck(
        command.command_id,
        AckResult::APPLIED,
        "led applied",
        mode_snapshot,
        command.desired_led
    );
}
}  // namespace

namespace LedControl {
bool init() {
    if (!ensureStateMutex()) {
        ESP_LOGE(TAG, "state mutex create failed");
        return false;
    }

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

AckMessage applyCommand(const CommandEnvelope& command) {
    if (!is_initialized) {
        return buildAckFromCurrent(command.command_id, AckResult::REJECTED, "not ready");
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

void controlTick(const SensorReading& latest_reading) {
    if (!is_initialized) {
        return;
    }

    DeviceMode mode_snapshot = DeviceMode::AUTONOMOUS;
    LedState led_snapshot = {};
    ThresholdConfig thresholds_snapshot = {};
    snapshotState(mode_snapshot, led_snapshot, &thresholds_snapshot);

    if (mode_snapshot != DeviceMode::AUTONOMOUS) {
        return;
    }

    if (!led_snapshot.power) {
        return;
    }

    uint8_t next_brightness_pct = 0U;
    if (isReadingWithinThresholds(latest_reading, thresholds_snapshot)) {
        next_brightness_pct = computeAutonomousBrightness(latest_reading.light_lux);
    }

    if (next_brightness_pct == led_snapshot.brightness_pct) {
        return;
    }

    if (!lockState(pdMS_TO_TICKS(100))) {
        ESP_LOGW(TAG, "state lock timeout");
        return;
    }

    current_led.brightness_pct = next_brightness_pct;
    led_snapshot = current_led;

    unlockState();

    applyToHardware(led_snapshot);
}

LedState getCurrentLedState() {
    DeviceMode ignored_mode = DeviceMode::AUTONOMOUS;
    LedState led = {};
    snapshotState(ignored_mode, led);
    return led;
}

DeviceMode getCurrentMode() {
    DeviceMode mode = DeviceMode::AUTONOMOUS;
    LedState ignored_led = {};
    snapshotState(mode, ignored_led);
    return mode;
}

StatusMessage buildStatusMessage(uint8_t degraded_tier) {
    DeviceMode mode = DeviceMode::AUTONOMOUS;
    LedState led = {};
    snapshotState(mode, led);

    StatusMessage status = {};
    status.timestamp_ms = getNowMs();
    status.mode = mode;
    status.led = led;
    status.degraded_tier = degraded_tier;
    status.uptime_sec = getUptimeSec();
    return status;
}
}  // namespace LedControl
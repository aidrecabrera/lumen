#include "lumen_led_control.h"

#include <Adafruit_NeoPixel.h>
#include <assert.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>
#include <string.h>

#include "lumen_board_config.h"
#include "lumen_config_manager.h"
#include "lumen_system_utils.h"

namespace {
static const char* TAG = "led_control";

static Adafruit_NeoPixel led_strip(LED_PIXEL_COUNT, LED_DATA_PIN, NEO_GRB + NEO_KHZ800);

struct ControlState {
    DeviceMode mode;
    ValidLedState led;
    ValidThresholds thresholds;
};

// Empty until init() loads the persisted state. Guarded by state_mutex.
static std::optional<ControlState> current_state;

static StaticSemaphore_t state_mutex_storage;
static SemaphoreHandle_t state_mutex = nullptr;

uint32_t getUptimeSec() { return static_cast<uint32_t>(getNowMs() / 1000ULL); }

// Static creation with a static buffer cannot return NULL (IDF 4.4 freertos/semphr.h:793:
// NULL only when the buffer is NULL), so the handle needs no null check.
// init() calls this before the tasks start, so the lazy path never races.
void createStateMutexOnce() {
    if (state_mutex == nullptr) {
        state_mutex = xSemaphoreCreateMutexStatic(&state_mutex_storage);
    }
}

void lockState() {
    createStateMutexOnce();
    xSemaphoreTake(state_mutex, portMAX_DELAY);
}

// Only called after lockState(), which created the mutex.
void unlockState() { xSemaphoreGive(state_mutex); }

bool isReadingWithinThresholds(const SensorReading& reading, const ThresholdConfig& thresholds) {
    if (!isfinite(reading.temperature_c) || !isfinite(reading.humidity_pct)) {
        return false;
    }

    return (reading.temperature_c >= thresholds.temp_min_c) &&
           (reading.temperature_c <= thresholds.temp_max_c) &&
           (reading.humidity_pct >= thresholds.humidity_min_pct) &&
           (reading.humidity_pct <= thresholds.humidity_max_pct);
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
    // Sensors::readLight accepts only finite lux >= 0. A NaN here would make clampBrightness
    // cast NaN to uint8_t, so stop loudly instead.
    assert(isfinite(light_lux) && light_lux >= 0.0f);

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

    // Far-red is approximated on RGB LEDs as additional red.
    const uint8_t red_level =
        LedControl::scaleChannel(led.brightness_pct, static_cast<uint8_t>(red_pct + far_red_pct));
    const uint8_t blue_level = LedControl::scaleChannel(led.brightness_pct, blue_pct);

    const uint32_t pixel_color = led_strip.Color(red_level, 0U, blue_level);

    for (uint16_t pixel_idx = 0; pixel_idx < LED_PIXEL_COUNT; ++pixel_idx) {
        led_strip.setPixelColor(pixel_idx, pixel_color);
    }

    led_strip.show();
}

bool persistRuntimeState(const ControlState& next) {
    RuntimeConfig config = ConfigManager::getConfigSnapshot();
    config.mode = next.mode;
    config.led = next.led;
    config.thresholds = next.thresholds;
    return ConfigManager::saveRuntimeConfig(config);
}

std::optional<ControlState> snapshotState() {
    lockState();
    const std::optional<ControlState> snapshot = current_state;
    unlockState();
    return snapshot;
}

// Before init() there is no state, so mode and led keep the caller's defaults.
void snapshotModeAndLed(DeviceMode& mode, LedState& led) {
    const std::optional<ControlState> snapshot = snapshotState();
    if (snapshot) {
        mode = snapshot->mode;
        led = snapshot->led.raw();
    }
}

AckMessage buildAck(
    const CommandId& command_id,
    AckResult result,
    AckReason reason,
    DeviceMode mode,
    const LedState& led
) {
    AckMessage ack = {};
    ack.command_id = command_id;
    ack.result = result;
    ack.timestamp_ms = getNowMs();
    ack.mode = mode;
    ack.led = led;
    ack.reason = reason;
    return ack;
}

AckMessage buildAckFromCurrent(const CommandId& command_id, AckResult result, AckReason reason) {
    DeviceMode mode = DeviceMode::AUTONOMOUS;
    LedState led = {};
    snapshotModeAndLed(mode, led);
    return buildAck(command_id, result, reason, mode, led);
}

AckMessage rejectCommand(const CommandId& command_id, AckReason reason) {
    ESP_LOGW(TAG, "command rejected: %s", ackReasonText(reason));
    return buildAckFromCurrent(command_id, AckResult::REJECTED, reason);
}

// The apply* functions get the state snapshot that applyCommand() took after init().
// current_state is set then, so their writes through current_state-> are safe.
AckMessage applyModeChange(
    const CommandId& command_id, const ModeCommand& command, const ControlState& snapshot
) {
    ControlState next = snapshot;
    next.mode = command.desired_mode;

    if (!persistRuntimeState(next)) {
        return rejectCommand(command_id, AckReason::PERSIST_FAILED);
    }

    lockState();
    current_state->mode = command.desired_mode;
    unlockState();

    ESP_LOGI(TAG, "mode changed");
    return buildAck(
        command_id,
        AckResult::APPLIED,
        AckReason::MODE_APPLIED,
        command.desired_mode,
        snapshot.led.raw()
    );
}

AckMessage applyConfigChange(
    const CommandId& command_id, const ConfigCommand& command, const ControlState& snapshot
) {
    ControlState next = snapshot;
    next.thresholds = command.desired_thresholds;

    if (!persistRuntimeState(next)) {
        return rejectCommand(command_id, AckReason::PERSIST_FAILED);
    }

    lockState();
    current_state->thresholds = command.desired_thresholds;
    unlockState();

    return buildAck(
        command_id, AckResult::APPLIED, AckReason::CONFIG_APPLIED, snapshot.mode, snapshot.led.raw()
    );
}

AckMessage applyLedChange(
    const CommandId& command_id, const LedCommand& command, const ControlState& snapshot
) {
    if (snapshot.mode == DeviceMode::AUTONOMOUS) {
        return rejectCommand(command_id, AckReason::MANUAL_LOCKED);
    }

    ControlState next = snapshot;
    next.led = command.desired_led;

    if (!persistRuntimeState(next)) {
        return rejectCommand(command_id, AckReason::PERSIST_FAILED);
    }

    lockState();
    current_state->led = command.desired_led;
    unlockState();

    applyToHardware(command.desired_led.raw());
    return buildAck(
        command_id,
        AckResult::APPLIED,
        AckReason::LED_APPLIED,
        snapshot.mode,
        command.desired_led.raw()
    );
}

// std::visit needs one overload per CommandPayload alternative, so a new kind is a compile error.
struct CommandApplier {
    const CommandId& command_id;
    const ControlState& snapshot;

    AckMessage operator()(const LedCommand& command) const {
        return applyLedChange(command_id, command, snapshot);
    }

    AckMessage operator()(const ConfigCommand& command) const {
        return applyConfigChange(command_id, command, snapshot);
    }

    AckMessage operator()(const ModeCommand& command) const {
        return applyModeChange(command_id, command, snapshot);
    }
};
}  // namespace

namespace LedControl {
// ConfigManager parsed the persisted led and thresholds, so this load needs no check.
bool init() {
    createStateMutexOnce();
    const RuntimeConfig config = ConfigManager::getConfigSnapshot();
    const ControlState loaded = {config.mode, config.led, config.thresholds};

    led_strip.begin();
    applyToHardware(loaded.led.raw());

    lockState();
    current_state = loaded;
    unlockState();

    ESP_LOGI(TAG, "led control initialized");
    return true;
}

AckMessage applyCommand(const CommandEnvelope& command) {
    const std::optional<ControlState> snapshot = snapshotState();
    if (!snapshot) {
        return buildAckFromCurrent(command.command_id, AckResult::REJECTED, AckReason::NOT_READY);
    }

    // The payload holds only trivially copyable types, so it is never valueless and
    // std::visit cannot throw.
    return std::visit(CommandApplier{command.command_id, *snapshot}, command.payload);
}

AckMessage buildRejectedAck(const CommandId& command_id, AckReason reason) {
    ESP_LOGW(
        TAG, "command %s rejected before apply: %s", command_id.c_str(), ackReasonText(reason)
    );
    return buildAckFromCurrent(command_id, AckResult::REJECTED, reason);
}

void controlTick(const SensorReading& latest_reading) {
    const std::optional<ControlState> snapshot = snapshotState();
    if (!snapshot) {
        return;
    }

    if (snapshot->mode != DeviceMode::AUTONOMOUS) {
        return;
    }

    if (!snapshot->led.raw().power) {
        return;
    }

    if (!isReadingWithinThresholds(latest_reading, snapshot->thresholds.raw())) {
        ESP_LOGW(TAG, "autonomous reading outside thresholds, holding brightness");
        return;
    }

    const uint8_t next_brightness_pct = computeAutonomousBrightness(latest_reading.light_lux);

    if (next_brightness_pct == snapshot->led.raw().brightness_pct) {
        return;
    }

    // clampBrightness() keeps the value at 100 or less, so this parse fails only on a code bug.
    lockState();
    LedState next_led = current_state->led.raw();
    next_led.brightness_pct = next_brightness_pct;
    const std::optional<ValidLedState> valid_next_led = ValidLedState::parse(next_led);
    if (valid_next_led) {
        current_state->led = *valid_next_led;
    }
    unlockState();

    if (!valid_next_led) {
        ESP_LOGE(TAG, "autonomous brightness %u rejected", next_brightness_pct);
        return;
    }

    applyToHardware(next_led);
}

LedState getCurrentLedState() {
    DeviceMode ignored_mode = DeviceMode::AUTONOMOUS;
    LedState led = {};
    snapshotModeAndLed(ignored_mode, led);
    return led;
}

DeviceMode getCurrentMode() {
    DeviceMode mode = DeviceMode::AUTONOMOUS;
    LedState ignored_led = {};
    snapshotModeAndLed(mode, ignored_led);
    return mode;
}

StatusMessage buildStatusMessage(DegradedTier degraded_tier) {
    DeviceMode mode = DeviceMode::AUTONOMOUS;
    LedState led = {};
    snapshotModeAndLed(mode, led);

    StatusMessage status = {};
    status.timestamp_ms = getNowMs();
    status.mode = mode;
    status.led = led;
    status.degraded_tier = degraded_tier;
    status.uptime_sec = getUptimeSec();
    return status;
}

uint8_t scaleChannel(uint8_t brightness_pct, uint8_t channel_pct) {
    // 32-bit math: 100 * 100 * 255 = 2,550,000 does not fit in uint16_t.
    const uint32_t scaled =
        static_cast<uint32_t>(brightness_pct) * static_cast<uint32_t>(channel_pct) * 255U;
    return static_cast<uint8_t>(scaled / 10000U);
}
}  // namespace LedControl

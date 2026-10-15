#include "lumen_led_control.h"

#include <Adafruit_NeoPixel.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

#include "lumen_auto_control.h"
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
    ValidLuxCeiling lux_ceiling;
    AutoState auto_state;
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
    config.lux_ceiling = next.lux_ceiling;
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
    const CommandEnvelope& command,
    AckResult result,
    AckReason reason,
    DeviceMode mode,
    const LedState& led
) {
    AckMessage ack = {};
    ack.command_id = command.command_id;
    ack.source = command.source;
    ack.result = result;
    ack.timestamp_ms = getNowMs();
    ack.mode = mode;
    ack.led = led;
    ack.reason = reason;
    return ack;
}

AckMessage buildAckFromCurrent(const CommandEnvelope& command, AckResult result, AckReason reason) {
    DeviceMode mode = DeviceMode::AUTONOMOUS;
    LedState led = {};
    snapshotModeAndLed(mode, led);
    return buildAck(command, result, reason, mode, led);
}

AckMessage rejectCommand(const CommandEnvelope& command, AckReason reason) {
    ESP_LOGW(TAG, "command rejected: %s", ackReasonText(reason));
    return buildAckFromCurrent(command, AckResult::REJECTED, reason);
}

// The apply* functions get the state snapshot that applyCommand() took after init().
// current_state is set then, so their writes through current_state-> are safe.
AckMessage applyModeChange(
    const CommandEnvelope& envelope, const ModeCommand& command, const ControlState& snapshot
) {
    ControlState next = snapshot;
    next.mode = command.desired_mode;

    if (!persistRuntimeState(next)) {
        return rejectCommand(envelope, AckReason::PERSIST_FAILED);
    }

    lockState();
    current_state->mode = command.desired_mode;
    current_state->auto_state = AutoState::ACTIVE;
    unlockState();

    ESP_LOGI(TAG, "mode changed");
    return buildAck(
        envelope,
        AckResult::APPLIED,
        AckReason::MODE_APPLIED,
        command.desired_mode,
        snapshot.led.raw()
    );
}

AckMessage applyConfigChange(
    const CommandEnvelope& envelope, const ConfigCommand& command, const ControlState& snapshot
) {
    ControlState next = snapshot;
    next.thresholds = command.desired_thresholds;
    next.lux_ceiling = command.desired_lux_ceiling.value_or(snapshot.lux_ceiling);

    if (!persistRuntimeState(next)) {
        return rejectCommand(envelope, AckReason::PERSIST_FAILED);
    }

    lockState();
    current_state->thresholds = next.thresholds;
    current_state->lux_ceiling = next.lux_ceiling;
    unlockState();

    return buildAck(
        envelope, AckResult::APPLIED, AckReason::CONFIG_APPLIED, snapshot.mode, snapshot.led.raw()
    );
}

AckMessage applyLedChange(
    const CommandEnvelope& envelope, const LedCommand& command, const ControlState& snapshot
) {
    if (snapshot.mode == DeviceMode::AUTONOMOUS) {
        return rejectCommand(envelope, AckReason::MANUAL_LOCKED);
    }

    ControlState next = snapshot;
    next.led = command.desired_led;

    if (!persistRuntimeState(next)) {
        return rejectCommand(envelope, AckReason::PERSIST_FAILED);
    }

    lockState();
    current_state->led = command.desired_led;
    unlockState();

    applyToHardware(command.desired_led.raw());
    return buildAck(
        envelope,
        AckResult::APPLIED,
        AckReason::LED_APPLIED,
        snapshot.mode,
        command.desired_led.raw()
    );
}

// std::visit needs one overload per CommandPayload alternative, so a new kind is a compile error.
struct CommandApplier {
    const CommandEnvelope& envelope;
    const ControlState& snapshot;

    AckMessage operator()(const LedCommand& command) const {
        return applyLedChange(envelope, command, snapshot);
    }

    AckMessage operator()(const ConfigCommand& command) const {
        return applyConfigChange(envelope, command, snapshot);
    }

    AckMessage operator()(const ModeCommand& command) const {
        return applyModeChange(envelope, command, snapshot);
    }
};
}  // namespace

namespace LedControl {
// ConfigManager parsed the persisted led and thresholds, so this load needs no check.
bool init() {
    createStateMutexOnce();
    const RuntimeConfig config = ConfigManager::getConfigSnapshot();
    const ControlState loaded = {
        config.mode, config.led, config.thresholds, config.lux_ceiling, AutoState::ACTIVE
    };

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
        return buildAckFromCurrent(command, AckResult::REJECTED, AckReason::NOT_READY);
    }

    // The payload holds only trivially copyable types, so it is never valueless and
    // std::visit cannot throw.
    return std::visit(CommandApplier{command, *snapshot}, command.payload);
}

AckMessage buildRejectedAck(const CommandId& command_id, CommandSource source, AckReason reason) {
    ESP_LOGW(
        TAG, "command %s rejected before apply: %s", command_id.c_str(), ackReasonText(reason)
    );
    CommandEnvelope command = {};
    command.command_id = command_id;
    command.source = source;
    return buildAckFromCurrent(command, AckResult::REJECTED, reason);
}

bool controlTick(const SensorReading& latest_reading) {
    const std::optional<ControlState> snapshot = snapshotState();
    if (!snapshot || snapshot->mode != DeviceMode::AUTONOMOUS || !snapshot->led.raw().power) {
        return false;
    }

    const AutoDecision decision = decideAutonomous(
        snapshot->auto_state, latest_reading, snapshot->thresholds.raw(), snapshot->lux_ceiling
    );
    const uint8_t current_pct = snapshot->led.raw().brightness_pct;
    const uint8_t next_pct = decision.brightness_pct.value_or(current_pct);
    if (decision.state == snapshot->auto_state && next_pct == current_pct) {
        return false;
    }

    // decideAutonomous keeps brightness at 100 or less, so this parse fails only on a bug.
    LedState next_led = snapshot->led.raw();
    next_led.brightness_pct = next_pct;
    const ValidLedState valid_next_led = ValidLedState::parse(next_led).value();

    lockState();
    current_state->led = valid_next_led;
    current_state->auto_state = decision.state;
    unlockState();

    applyToHardware(next_led);
    return true;
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

// degraded_tier and cloud belong to the network task, which sets them before publishing.
StatusMessage buildStatusMessage() {
    StatusMessage status = {};
    status.timestamp_ms = getNowMs();
    status.uptime_sec = getUptimeSec();

    const std::optional<ControlState> snapshot = snapshotState();
    if (snapshot) {
        status.mode = snapshot->mode;
        status.led = snapshot->led.raw();
        status.auto_state = snapshot->auto_state;
        status.thresholds = snapshot->thresholds.raw();
        status.lux_ceiling = snapshot->lux_ceiling.raw();
    }

    return status;
}

uint8_t scaleChannel(uint8_t brightness_pct, uint8_t channel_pct) {
    // 32-bit math: 100 * 100 * 255 = 2,550,000 does not fit in uint16_t.
    const uint32_t scaled =
        static_cast<uint32_t>(brightness_pct) * static_cast<uint32_t>(channel_pct) * 255U;
    return static_cast<uint8_t>(scaled / 10000U);
}
}  // namespace LedControl

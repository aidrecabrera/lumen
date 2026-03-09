#include "energy_tracker.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <math.h>

#include "config.h"
#include "config_manager.h"

namespace {
static const char* TAG = "energy_tracker";

static float total_wh = 0.0f;
static float session_wh = 0.0f;
static uint64_t light_on_ms = 0;
static uint64_t max_brightness_ms = 0;
static uint64_t last_update_ms = 0;
static bool is_initialized = false;

static constexpr float FULL_SCALE_POWER_W = 12.0f;
static constexpr uint32_t MAX_DELTA_MS = 5000;

uint64_t getNowMs() { return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL); }

uint32_t toSeconds(uint64_t duration_ms) { return static_cast<uint32_t>(duration_ms / 1000ULL); }

uint32_t getActiveDistributionPct(const LedState& led) {
    uint32_t total_pct = 0;

    if (led.red_enabled) {
        total_pct += led.red_dist_pct;
    }

    if (led.blue_enabled) {
        total_pct += led.blue_dist_pct;
    }

    if (led.far_red_enabled) {
        total_pct += led.far_red_dist_pct;
    }

    return total_pct;
}

float computePowerWatts(const LedState& led) {
    if (!led.power || led.brightness_pct == 0) {
        return 0.0f;
    }

    uint32_t active_distribution_pct = getActiveDistributionPct(led);
    float brightness_ratio = static_cast<float>(led.brightness_pct) / 100.0f;
    float channel_ratio = static_cast<float>(active_distribution_pct) / 100.0f;

    return FULL_SCALE_POWER_W * brightness_ratio * channel_ratio;
}

uint32_t clampDeltaMs(uint64_t delta_ms) {
    if (delta_ms > MAX_DELTA_MS) {
        return MAX_DELTA_MS;
    }

    return static_cast<uint32_t>(delta_ms);
}
}  // namespace

namespace EnergyTracker {
bool init() {
    const RuntimeConfig& config = ConfigManager::getConfig();

    if (!isfinite(config.energy_total_wh) || config.energy_total_wh < 0.0f) {
        ESP_LOGE(TAG, "persisted energy invalid");
        return false;
    }

    total_wh = config.energy_total_wh;
    session_wh = 0.0f;
    light_on_ms = 0;
    max_brightness_ms = 0;
    last_update_ms = getNowMs();
    is_initialized = true;

    ESP_LOGI(TAG, "energy tracker initialized");
    return true;
}

void updateFromLedState(const LedState& led) {
    if (!is_initialized) {
        return;
    }

    uint64_t now_ms = getNowMs();
    if (now_ms <= last_update_ms) {
        return;
    }

    uint32_t delta_ms = clampDeltaMs(now_ms - last_update_ms);
    last_update_ms = now_ms;

    float power_w = computePowerWatts(led);
    float delta_wh = power_w * (static_cast<float>(delta_ms) / 3600000.0f);

    total_wh += delta_wh;
    session_wh += delta_wh;

    if (led.power && led.brightness_pct > 0) {
        light_on_ms += delta_ms;
    }

    if (led.power && led.brightness_pct == 100) {
        max_brightness_ms += delta_ms;
    }
}

EnergyMessage getSnapshot() {
    EnergyMessage snapshot = {};
    snapshot.timestamp_ms = getNowMs();
    snapshot.total_wh = total_wh;
    snapshot.session_wh = session_wh;
    snapshot.light_on_sec = toSeconds(light_on_ms);
    snapshot.max_brightness_sec = toSeconds(max_brightness_ms);
    return snapshot;
}

bool requestPersist() {
    if (!is_initialized) {
        ESP_LOGW(TAG, "persist before init");
        return false;
    }

    bool was_saved = ConfigManager::saveEnergyTotal(total_wh);
    if (!was_saved) {
        ESP_LOGW(TAG, "energy persist failed");
        return false;
    }

    return true;
}
}  // namespace EnergyTracker

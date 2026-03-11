#include "config_manager.h"

#include <Preferences.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "validation.h"

namespace {
static const char* TAG = "config_manager";
static const char* NVS_NAMESPACE = "spot";

static const char* KEY_RUNTIME_CFG = "runtime_cfg";
static const char* KEY_ENERGY_WH = "energy_wh";

struct PersistedRuntimeConfig {
    char device_id[DEVICE_ID_MAX_LEN];
    uint8_t mode;
    uint8_t reserved[3];
    ThresholdConfig thresholds;
    LedState led;
};

static Preferences preferences;
static RuntimeConfig runtime_config = {};
static bool is_initialized = false;
static bool has_nvs = false;

ThresholdConfig buildDefaultThresholds() {
    ThresholdConfig thresholds = {};
    thresholds.temp_min_c = 18.0f;
    thresholds.temp_max_c = 28.0f;
    thresholds.humidity_min_pct = 60.0f;
    thresholds.humidity_max_pct = 80.0f;
    return thresholds;
}

LedState buildDefaultLedState() {
    LedState led = {};
    led.power = true;
    led.brightness_pct = 50U;
    led.red_enabled = true;
    led.blue_enabled = true;
    led.far_red_enabled = true;
    led.red_dist_pct = 40U;
    led.blue_dist_pct = 35U;
    led.far_red_dist_pct = 25U;
    return led;
}

bool generateDeviceId(char* out_device_id, size_t out_len) {
    uint8_t base_mac[6] = {};
    const esp_err_t read_result = esp_read_mac(base_mac, ESP_MAC_WIFI_STA);

    if (read_result != ESP_OK) {
        ESP_LOGE(TAG, "mac read failed: %s", esp_err_to_name(read_result));
        return false;
    }

    const int written = snprintf(
        out_device_id,
        out_len,
        "spot-%02x%02x%02x%02x%02x%02x",
        base_mac[0],
        base_mac[1],
        base_mac[2],
        base_mac[3],
        base_mac[4],
        base_mac[5]
    );

    if (written <= 0 || static_cast<size_t>(written) >= out_len) {
        ESP_LOGE(TAG, "device id build failed");
        return false;
    }

    return true;
}

void applyDefaultConfig(RuntimeConfig& config) {
    memset(&config, 0, sizeof(config));
    config.mode = DeviceMode::AUTONOMOUS;
    config.thresholds = buildDefaultThresholds();
    config.led = buildDefaultLedState();
    config.energy_total_wh = 0.0f;

    if (!generateDeviceId(config.device_id, sizeof(config.device_id))) {
        copyText(config.device_id, sizeof(config.device_id), "spot-unknown");
    }
}

DeviceMode sanitizeMode(uint8_t raw_mode) {
    if (raw_mode == static_cast<uint8_t>(DeviceMode::MANUAL)) {
        return DeviceMode::MANUAL;
    }

    return DeviceMode::AUTONOMOUS;
}

bool hasTextInBuffer(const char* text, size_t max_len) {
    return (text != nullptr) && (strnlen(text, max_len) < max_len) && (text[0] != '\0');
}

bool writeRuntimeConfigBlob(const RuntimeConfig& config) {
    if (!has_nvs) {
        ESP_LOGW(TAG, "nvs unavailable");
        return false;
    }

    if (!hasTextInBuffer(config.device_id, sizeof(config.device_id))) {
        ESP_LOGW(TAG, "runtime config save rejected: bad device id");
        return false;
    }

    if (!isThresholdConfigValid(config.thresholds) || !isLedStateValid(config.led)) {
        ESP_LOGW(TAG, "runtime config save rejected: invalid payload");
        return false;
    }

    PersistedRuntimeConfig persisted = {};
    copyText(persisted.device_id, sizeof(persisted.device_id), config.device_id);
    persisted.mode = static_cast<uint8_t>(config.mode);
    persisted.thresholds = config.thresholds;
    persisted.led = config.led;

    const size_t written = preferences.putBytes(KEY_RUNTIME_CFG, &persisted, sizeof(persisted));

    if (written != sizeof(persisted)) {
        ESP_LOGE(TAG, "runtime config blob write failed");
        return false;
    }

    return true;
}

bool writeEnergyTotal(float total_wh) {
    if (!has_nvs) {
        ESP_LOGW(TAG, "nvs unavailable");
        return false;
    }

    if (!isEnergyTotalValid(total_wh)) {
        ESP_LOGW(TAG, "energy save rejected");
        return false;
    }

    const size_t written = preferences.putFloat(KEY_ENERGY_WH, total_wh);
    if (written == 0U) {
        ESP_LOGE(TAG, "energy write failed");
        return false;
    }

    return true;
}

bool loadRuntimeConfigBlob(RuntimeConfig& config) {
    PersistedRuntimeConfig persisted = {};
    const size_t read_len = preferences.getBytes(KEY_RUNTIME_CFG, &persisted, sizeof(persisted));

    if (read_len != sizeof(persisted)) {
        return false;
    }

    if (hasTextInBuffer(persisted.device_id, sizeof(persisted.device_id))) {
        copyText(config.device_id, sizeof(config.device_id), persisted.device_id);
    } else {
        ESP_LOGW(TAG, "invalid device id in blob, using default");
    }

    config.mode = sanitizeMode(persisted.mode);

    if (isThresholdConfigValid(persisted.thresholds)) {
        config.thresholds = persisted.thresholds;
    } else {
        ESP_LOGW(TAG, "invalid thresholds in blob, using default");
    }

    if (isLedStateValid(persisted.led)) {
        config.led = persisted.led;
    } else {
        ESP_LOGW(TAG, "invalid led state in blob, using default");
    }

    return true;
}

void loadEnergyTotal(RuntimeConfig& config) {
    const float stored = preferences.getFloat(KEY_ENERGY_WH, config.energy_total_wh);

    if (isEnergyTotalValid(stored)) {
        config.energy_total_wh = stored;
    } else {
        ESP_LOGW(TAG, "invalid energy total in nvs, using default");
    }
}
}  // namespace

namespace ConfigManager {
bool init() {
    applyDefaultConfig(runtime_config);

    has_nvs = preferences.begin(NVS_NAMESPACE, false);
    if (!has_nvs) {
        ESP_LOGE(TAG, "nvs open failed, running with defaults");
        is_initialized = true;
        return false;
    }

    loadRuntimeConfigBlob(runtime_config);
    loadEnergyTotal(runtime_config);

    is_initialized = true;
    ESP_LOGI(TAG, "config loaded for %s", runtime_config.device_id);
    return true;
}

bool loadDefaults() {
    applyDefaultConfig(runtime_config);

    if (!has_nvs) {
        ESP_LOGW(TAG, "defaults applied in ram only");
        return false;
    }

    bool ok = writeRuntimeConfigBlob(runtime_config);
    ok &= writeEnergyTotal(runtime_config.energy_total_wh);
    return ok;
}

const RuntimeConfig& getConfig() { return runtime_config; }

bool saveThresholds(const ThresholdConfig& thresholds) {
    if (!isThresholdConfigValid(thresholds)) {
        ESP_LOGW(TAG, "threshold save rejected");
        return false;
    }

    RuntimeConfig next = runtime_config;
    next.thresholds = thresholds;

    if (!writeRuntimeConfigBlob(next)) {
        return false;
    }

    runtime_config.thresholds = thresholds;
    return true;
}

bool saveMode(DeviceMode mode) {
    RuntimeConfig next = runtime_config;
    next.mode = mode;

    if (!writeRuntimeConfigBlob(next)) {
        return false;
    }

    runtime_config.mode = mode;
    return true;
}

bool saveLedState(const LedState& led) {
    if (!isLedStateValid(led)) {
        ESP_LOGW(TAG, "led save rejected");
        return false;
    }

    RuntimeConfig next = runtime_config;
    next.led = led;

    if (!writeRuntimeConfigBlob(next)) {
        return false;
    }

    runtime_config.led = led;
    return true;
}

bool saveEnergyTotal(float total_wh) {
    if (!isEnergyTotalValid(total_wh)) {
        ESP_LOGW(TAG, "energy save rejected");
        return false;
    }

    if (!writeEnergyTotal(total_wh)) {
        return false;
    }

    runtime_config.energy_total_wh = total_wh;
    return true;
}

bool saveRuntimeConfig(const RuntimeConfig& config) {
    if (!isThresholdConfigValid(config.thresholds) || !isLedStateValid(config.led)) {
        ESP_LOGW(TAG, "runtime config save rejected: invalid payload");
        return false;
    }

    RuntimeConfig next = runtime_config;
    next.mode = config.mode;
    next.thresholds = config.thresholds;
    next.led = config.led;

    if (!writeRuntimeConfigBlob(next)) {
        return false;
    }

    runtime_config.mode = next.mode;
    runtime_config.thresholds = next.thresholds;
    runtime_config.led = next.led;
    return true;
}
}  // namespace ConfigManager
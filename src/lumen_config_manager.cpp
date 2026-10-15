#include "lumen_config_manager.h"

#include <Preferences.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdio.h>
#include <string.h>

#include "lumen_board_config.h"
#include "lumen_system_utils.h"
#include "lumen_type_validation.h"

namespace {
static const char* TAG = "config_manager";
static const char* NVS_NAMESPACE = "lumen";

static const char* KEY_RUNTIME_CFG = "runtime_cfg";
static const char* KEY_ENERGY_WH = "energy_wh";
// own key: the runtime blob has no version field, so a new field there breaks saved configs.
static const char* KEY_LUX_CEILING = "lux_ceiling";
static const char* KEY_WIFI_SSID = "wifi_ssid";
static const char* KEY_WIFI_PASSWORD = "wifi_pass";
static const char* KEY_LOCAL_HASH = "local_hash";
static const char* KEY_WIFI_CONNECTED = "wifi_ok";

struct PersistedRuntimeConfig {
    char device_id[DEVICE_ID_MAX_LEN];
    uint8_t mode;
    uint8_t reserved[3];
    ThresholdConfig thresholds;
    LedState led;
};

// catch struct layout changes at compile time if these fire, the blob format
// has shifted and previously persisted configs will read back as garbage
static_assert(sizeof(LedState) == 8, "LedState layout changed, NVS blob will break");
static_assert(sizeof(ThresholdConfig) == 16, "ThresholdConfig layout changed, NVS blob will break");
static_assert(
    sizeof(PersistedRuntimeConfig) == 52,
    "PersistedRuntimeConfig layout changed, NVS blob will break"
);

constexpr ThresholdConfig DEFAULT_THRESHOLDS = {18.0f, 28.0f, 60.0f, 80.0f};

// power, brightness_pct, red/blue/far_red enabled, red/blue/far_red dist_pct.
constexpr LedState DEFAULT_LED_STATE = {true, 50U, true, true, true, 40U, 35U, 25U};

constexpr float DEFAULT_ENERGY_TOTAL_WH = 0.0f;

// The defaults are constants, so parse() fails only after a bad edit to them. value() then
// throws, and the throw stops the firmware at startup instead of running with a bad default.
// device_id stays empty here; applyDefaultConfig() sets it.
RuntimeConfig buildDefaultConfig() {
    return RuntimeConfig{
        {},
        DeviceMode::AUTONOMOUS,
        ValidThresholds::parse(DEFAULT_THRESHOLDS).value(),
        ValidLedState::parse(DEFAULT_LED_STATE).value(),
        EnergyTotalWh::parse(DEFAULT_ENERGY_TOTAL_WH).value(),
        ValidLuxCeiling::parse(DEFAULT_LUX_CEILING).value(),
    };
}

static Preferences preferences;
static RuntimeConfig runtime_config = buildDefaultConfig();
static bool is_initialized = false;
static bool has_nvs = false;
static StaticSemaphore_t config_mutex_storage;
static SemaphoreHandle_t config_mutex = nullptr;

// Static creation with a static buffer cannot return NULL (IDF 4.4 freertos/semphr.h:793:
// NULL only when the buffer is NULL), so the handle needs no null check.
void lockConfig() {
    if (config_mutex == nullptr) {
        config_mutex = xSemaphoreCreateMutexStatic(&config_mutex_storage);
    }

    xSemaphoreTake(config_mutex, portMAX_DELAY);
}

// Only called after lockConfig(), which created the mutex.
void unlockConfig() { xSemaphoreGive(config_mutex); }

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
        "lumen-%02x%02x%02x%02x%02x%02x",
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
    config = buildDefaultConfig();

    if (!generateDeviceId(config.device_id, sizeof(config.device_id))) {
        copyText(config.device_id, sizeof(config.device_id), "lumen-unknown");
    }
}

DeviceMode sanitizeMode(uint8_t raw_mode) {
    DeviceMode mode = DeviceMode::AUTONOMOUS;
    if (!parseDeviceMode(raw_mode, mode)) {
        ESP_LOGW(TAG, "invalid mode %u in blob, using default", raw_mode);
        return DeviceMode::AUTONOMOUS;
    }

    return mode;
}

bool hasTextInBuffer(const char* text, size_t max_len) {
    return (text != nullptr) && (strnlen(text, max_len) < max_len) && (text[0] != '\0');
}

// The thresholds and led are valid by type. device_id is always set by applyDefaultConfig()
// or copied only when valid.
bool writeRuntimeConfigBlob(const RuntimeConfig& config) {
    if (!has_nvs) {
        ESP_LOGW(TAG, "nvs unavailable");
        return false;
    }

    PersistedRuntimeConfig persisted = {};
    copyText(persisted.device_id, sizeof(persisted.device_id), config.device_id);
    persisted.mode = static_cast<uint8_t>(config.mode);
    persisted.thresholds = config.thresholds.raw();
    persisted.led = config.led.raw();

    const size_t written = preferences.putBytes(KEY_RUNTIME_CFG, &persisted, sizeof(persisted));

    if (written != sizeof(persisted)) {
        ESP_LOGE(TAG, "runtime config blob write failed");
        return false;
    }

    return true;
}

bool writeEnergyTotal(EnergyTotalWh total_wh) {
    if (!has_nvs) {
        ESP_LOGW(TAG, "nvs unavailable");
        return false;
    }

    const size_t written = preferences.putFloat(KEY_ENERGY_WH, total_wh.raw());
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

    const std::optional<ValidThresholds> thresholds = ValidThresholds::parse(persisted.thresholds);
    if (thresholds) {
        config.thresholds = *thresholds;
    } else {
        ESP_LOGW(TAG, "invalid thresholds in blob, using default");
    }

    const std::optional<ValidLedState> led = ValidLedState::parse(persisted.led);
    if (led) {
        config.led = *led;
    } else {
        ESP_LOGW(TAG, "invalid led state in blob, using default");
    }

    return true;
}

bool writeLuxCeiling(ValidLuxCeiling lux_ceiling) {
    if (!has_nvs) {
        ESP_LOGW(TAG, "nvs unavailable");
        return false;
    }

    if (preferences.putFloat(KEY_LUX_CEILING, lux_ceiling.raw()) == 0U) {
        ESP_LOGE(TAG, "lux ceiling write failed");
        return false;
    }

    return true;
}

// caller holds the config lock. ram changes only after the nvs writes succeed.
bool commitRuntimeConfigLocked(const RuntimeConfig& next) {
    const bool lux_changed = next.lux_ceiling.raw() != runtime_config.lux_ceiling.raw();
    if (lux_changed && !writeLuxCeiling(next.lux_ceiling)) {
        return false;
    }

    if (!writeRuntimeConfigBlob(next)) {
        return false;
    }

    runtime_config = next;
    return true;
}

void loadLuxCeiling(RuntimeConfig& config) {
    const float stored = preferences.getFloat(KEY_LUX_CEILING, config.lux_ceiling.raw());

    const std::optional<ValidLuxCeiling> lux_ceiling = ValidLuxCeiling::parse(stored);
    if (lux_ceiling) {
        config.lux_ceiling = *lux_ceiling;
    } else {
        ESP_LOGW(TAG, "invalid lux ceiling in nvs, using default");
    }
}

void loadEnergyTotal(RuntimeConfig& config) {
    const float stored = preferences.getFloat(KEY_ENERGY_WH, config.energy_total_wh.raw());

    const std::optional<EnergyTotalWh> total_wh = EnergyTotalWh::parse(stored);
    if (total_wh) {
        config.energy_total_wh = *total_wh;
    } else {
        ESP_LOGW(TAG, "invalid energy total in nvs, using default");
    }
}
}  // namespace

namespace ConfigManager {
bool init() {
    lockConfig();
    applyDefaultConfig(runtime_config);

    has_nvs = preferences.begin(NVS_NAMESPACE, false);
    if (!has_nvs) {
        ESP_LOGE(TAG, "nvs open failed, running with defaults");
        is_initialized = true;
        unlockConfig();
        return false;
    }

    loadRuntimeConfigBlob(runtime_config);
    loadEnergyTotal(runtime_config);
    loadLuxCeiling(runtime_config);

    is_initialized = true;
    ESP_LOGI(TAG, "config loaded for %s", runtime_config.device_id);
    unlockConfig();
    return true;
}

bool loadDefaults() {
    lockConfig();
    applyDefaultConfig(runtime_config);

    if (!has_nvs) {
        ESP_LOGW(TAG, "defaults applied in ram only");
        unlockConfig();
        return false;
    }

    bool ok = writeRuntimeConfigBlob(runtime_config);
    ok &= writeEnergyTotal(runtime_config.energy_total_wh);
    ok &= writeLuxCeiling(runtime_config.lux_ceiling);
    unlockConfig();
    return ok;
}

RuntimeConfig getConfigSnapshot() {
    lockConfig();
    const RuntimeConfig snapshot = runtime_config;
    unlockConfig();
    return snapshot;
}

bool saveThresholds(const ValidThresholds& thresholds) {
    lockConfig();
    RuntimeConfig next = runtime_config;
    next.thresholds = thresholds;
    const bool ok = commitRuntimeConfigLocked(next);
    unlockConfig();
    return ok;
}

bool saveMode(DeviceMode mode) {
    lockConfig();
    RuntimeConfig next = runtime_config;
    next.mode = mode;
    const bool ok = commitRuntimeConfigLocked(next);
    unlockConfig();
    return ok;
}

bool saveLedState(const ValidLedState& led) {
    lockConfig();
    RuntimeConfig next = runtime_config;
    next.led = led;
    const bool ok = commitRuntimeConfigLocked(next);
    unlockConfig();
    return ok;
}

bool saveEnergyTotal(EnergyTotalWh total_wh) {
    lockConfig();
    if (!writeEnergyTotal(total_wh)) {
        unlockConfig();
        return false;
    }

    runtime_config.energy_total_wh = total_wh;
    unlockConfig();
    return true;
}

bool saveRuntimeConfig(const RuntimeConfig& config) {
    lockConfig();
    RuntimeConfig next = runtime_config;
    next.mode = config.mode;
    next.thresholds = config.thresholds;
    next.led = config.led;
    next.lux_ceiling = config.lux_ceiling;
    const bool ok = commitRuntimeConfigLocked(next);
    unlockConfig();
    return ok;
}

bool saveSetup(const ValidWifiCredentials& credentials, const LocalPasswordHash& password_hash) {
    lockConfig();
    const bool ok = has_nvs && preferences.putString(KEY_WIFI_SSID, credentials.ssid()) > 0U &&
                    preferences.putString(KEY_WIFI_PASSWORD, credentials.password()) ==
                        strlen(credentials.password()) &&
                    preferences.putString(KEY_LOCAL_HASH, password_hash.c_str()) > 0U &&
                    preferences.putBool(KEY_WIFI_CONNECTED, false) > 0U;
    unlockConfig();

    if (!ok) {
        ESP_LOGE(TAG, "setup save failed");
    }
    return ok;
}

std::optional<ValidWifiCredentials> loadWifiCredentials() {
    lockConfig();
    const String ssid = has_nvs ? preferences.getString(KEY_WIFI_SSID, "") : String();
    const String password = has_nvs ? preferences.getString(KEY_WIFI_PASSWORD, "") : String();
    unlockConfig();
    return ValidWifiCredentials::parse(ssid.c_str(), password.c_str());
}

std::optional<LocalPasswordHash> loadLocalPasswordHash() {
    lockConfig();
    const String hash = has_nvs ? preferences.getString(KEY_LOCAL_HASH, "") : String();
    unlockConfig();
    return LocalPasswordHash::parse(hash.c_str());
}

bool wifiHasConnected() {
    lockConfig();
    const bool connected = has_nvs && preferences.getBool(KEY_WIFI_CONNECTED, false);
    unlockConfig();
    return connected;
}

void markWifiConnected() {
    lockConfig();
    if (has_nvs && !preferences.getBool(KEY_WIFI_CONNECTED, false)) {
        preferences.putBool(KEY_WIFI_CONNECTED, true);
    }
    unlockConfig();
}
}  // namespace ConfigManager

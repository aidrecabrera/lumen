#include "config_manager.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <Preferences.h>
#include <esp_log.h>
#include <esp_mac.h>

#include "config.h"
#include "types.h"

namespace
{
    static const char *TAG = "config_manager";
    static const char *NVS_NAMESPACE = "spot";

    static const char *KEY_DEVICE_ID = "device_id";
    static const char *KEY_DEVICE_MODE = "device_mode";
    static const char *KEY_TEMP_MIN_C = "temp_min_c";
    static const char *KEY_TEMP_MAX_C = "temp_max_c";
    static const char *KEY_HUMIDITY_MIN_PCT = "humidity_min_pct";
    static const char *KEY_HUMIDITY_MAX_PCT = "humidity_max_pct";
    static const char *KEY_LED_POWER = "led_power";
    static const char *KEY_LED_BRIGHTNESS_PCT = "led_brightness_pct";
    static const char *KEY_LED_RED_ENABLED = "led_red_enabled";
    static const char *KEY_LED_BLUE_ENABLED = "led_blue_enabled";
    static const char *KEY_LED_FAR_RED_ENABLED = "led_far_red_enabled";
    static const char *KEY_LED_RED_PCT = "led_red_pct";
    static const char *KEY_LED_BLUE_PCT = "led_blue_pct";
    static const char *KEY_LED_FAR_RED_PCT = "led_far_red_pct";
    static const char *KEY_ENERGY_TOTAL_WH = "energy_total_wh";

    static Preferences preferences;
    static RuntimeConfig runtime_config = {};
    static bool is_initialized = false;
    static bool has_nvs = false;

    ThresholdConfig buildDefaultThresholds()
    {
        ThresholdConfig thresholds = {};
        thresholds.temp_min_c = 18.0f;
        thresholds.temp_max_c = 28.0f;
        thresholds.humidity_min_pct = 60.0f;
        thresholds.humidity_max_pct = 80.0f;
        return thresholds;
    }

    LedState buildDefaultLedState()
    {
        LedState led = {};
        led.power = true;
        led.brightness_pct = 50;
        led.red_enabled = true;
        led.blue_enabled = true;
        led.far_red_enabled = true;
        led.red_dist_pct = 40;
        led.blue_dist_pct = 35;
        led.far_red_dist_pct = 25;
        return led;
    }

    bool isThresholdConfigValid(const ThresholdConfig &thresholds)
    {
        if (!isfinite(thresholds.temp_min_c) ||
            !isfinite(thresholds.temp_max_c) ||
            !isfinite(thresholds.humidity_min_pct) ||
            !isfinite(thresholds.humidity_max_pct))
        {
            return false;
        }

        if (thresholds.temp_min_c > thresholds.temp_max_c)
        {
            return false;
        }

        if (thresholds.humidity_min_pct > thresholds.humidity_max_pct)
        {
            return false;
        }

        return true;
    }

    bool isLedStateValid(const LedState &led)
    {
        uint16_t total_pct = static_cast<uint16_t>(led.red_dist_pct) +
                             static_cast<uint16_t>(led.blue_dist_pct) +
                             static_cast<uint16_t>(led.far_red_dist_pct);

        if (led.brightness_pct > 100)
        {
            return false;
        }

        if (total_pct != 100)
        {
            return false;
        }

        return true;
    }

    bool isEnergyTotalValid(float total_wh)
    {
        if (!isfinite(total_wh))
        {
            return false;
        }

        if (total_wh < 0.0f)
        {
            return false;
        }

        return true;
    }

    void copyText(char *dest, size_t dest_len, const char *src)
    {
        if (dest == nullptr || dest_len == 0)
        {
            return;
        }

        if (src == nullptr)
        {
            dest[0] = '\0';
            return;
        }

        strncpy(dest, src, dest_len - 1);
        dest[dest_len - 1] = '\0';
    }

    bool generateDeviceId(char *out_device_id, size_t out_len)
    {
        uint8_t base_mac[6] = {};
        esp_err_t read_result = esp_read_mac(base_mac, ESP_MAC_WIFI_STA);

        if (read_result != ESP_OK)
        {
            ESP_LOGE(TAG, "mac read failed");
            return false;
        }

        int written = snprintf(
            out_device_id,
            out_len,
            "spot-%02x%02x%02x%02x%02x%02x",
            base_mac[0],
            base_mac[1],
            base_mac[2],
            base_mac[3],
            base_mac[4],
            base_mac[5]);

        if (written <= 0 || static_cast<size_t>(written) >= out_len)
        {
            ESP_LOGE(TAG, "device id build failed");
            return false;
        }

        return true;
    }

    void applyDefaultConfig(RuntimeConfig &config)
    {
        memset(&config, 0, sizeof(config));
        config.mode = DeviceMode::AUTONOMOUS;
        config.thresholds = buildDefaultThresholds();
        config.led = buildDefaultLedState();
        config.energy_total_wh = 0.0f;

        bool was_generated = generateDeviceId(
            config.device_id,
            sizeof(config.device_id));

        if (was_generated)
        {
            return;
        }

        copyText(config.device_id, sizeof(config.device_id), "spot-unknown");
    }

    DeviceMode sanitizeMode(uint8_t raw_mode)
    {
        if (raw_mode == static_cast<uint8_t>(DeviceMode::AUTONOMOUS))
        {
            return DeviceMode::AUTONOMOUS;
        }

        if (raw_mode == static_cast<uint8_t>(DeviceMode::MANUAL))
        {
            return DeviceMode::MANUAL;
        }

        return DeviceMode::AUTONOMOUS;
    }

    void loadThresholdsFromNvs(RuntimeConfig &config)
    {
        ThresholdConfig stored = {};
        stored.temp_min_c = preferences.getFloat(
            KEY_TEMP_MIN_C,
            config.thresholds.temp_min_c);
        stored.temp_max_c = preferences.getFloat(
            KEY_TEMP_MAX_C,
            config.thresholds.temp_max_c);
        stored.humidity_min_pct = preferences.getFloat(
            KEY_HUMIDITY_MIN_PCT,
            config.thresholds.humidity_min_pct);
        stored.humidity_max_pct = preferences.getFloat(
            KEY_HUMIDITY_MAX_PCT,
            config.thresholds.humidity_max_pct);

        if (isThresholdConfigValid(stored))
        {
            config.thresholds = stored;
            return;
        }

        ESP_LOGW(TAG, "invalid thresholds in nvs");
    }

    void loadLedStateFromNvs(RuntimeConfig &config)
    {
        LedState stored = config.led;
        stored.power = preferences.getBool(KEY_LED_POWER, config.led.power);
        stored.brightness_pct = preferences.getUChar(
            KEY_LED_BRIGHTNESS_PCT,
            config.led.brightness_pct);
        stored.red_enabled = preferences.getBool(
            KEY_LED_RED_ENABLED,
            config.led.red_enabled);
        stored.blue_enabled = preferences.getBool(
            KEY_LED_BLUE_ENABLED,
            config.led.blue_enabled);
        stored.far_red_enabled = preferences.getBool(
            KEY_LED_FAR_RED_ENABLED,
            config.led.far_red_enabled);
        stored.red_dist_pct = preferences.getUChar(
            KEY_LED_RED_PCT,
            config.led.red_dist_pct);
        stored.blue_dist_pct = preferences.getUChar(
            KEY_LED_BLUE_PCT,
            config.led.blue_dist_pct);
        stored.far_red_dist_pct = preferences.getUChar(
            KEY_LED_FAR_RED_PCT,
            config.led.far_red_dist_pct);

        if (isLedStateValid(stored))
        {
            config.led = stored;
            return;
        }

        ESP_LOGW(TAG, "invalid led state in nvs");
    }

    void loadDeviceIdFromNvs(RuntimeConfig &config)
    {
        char stored_device_id[DEVICE_ID_MAX_LEN] = {};
        size_t read_len = preferences.getString(
            KEY_DEVICE_ID,
            stored_device_id,
            sizeof(stored_device_id));

        if (read_len == 0 || stored_device_id[0] == '\0')
        {
            return;
        }

        copyText(config.device_id, sizeof(config.device_id), stored_device_id);
    }

    void loadRuntimeConfigFromNvs(RuntimeConfig &config)
    {
        loadDeviceIdFromNvs(config);
        config.mode = sanitizeMode(
            preferences.getUChar(
                KEY_DEVICE_MODE,
                static_cast<uint8_t>(config.mode)));

        loadThresholdsFromNvs(config);
        loadLedStateFromNvs(config);

        float stored_energy_total_wh = preferences.getFloat(
            KEY_ENERGY_TOTAL_WH,
            config.energy_total_wh);

        if (isEnergyTotalValid(stored_energy_total_wh))
        {
            config.energy_total_wh = stored_energy_total_wh;
            return;
        }

        ESP_LOGW(TAG, "invalid energy total in nvs");
    }

    bool putStringKey(const char *key, const char *value)
    {
        if (!has_nvs)
        {
            ESP_LOGW(TAG, "nvs unavailable for %s", key);
            return false;
        }

        size_t written_len = preferences.putString(key, value);
        if (written_len == 0)
        {
            ESP_LOGE(TAG, "put string failed %s", key);
            return false;
        }

        return true;
    }

    bool putBoolKey(const char *key, bool value)
    {
        if (!has_nvs)
        {
            ESP_LOGW(TAG, "nvs unavailable for %s", key);
            return false;
        }

        size_t written_len = preferences.putBool(key, value);
        if (written_len == 0)
        {
            ESP_LOGE(TAG, "put bool failed %s", key);
            return false;
        }

        return true;
    }

    bool putUCharKey(const char *key, uint8_t value)
    {
        if (!has_nvs)
        {
            ESP_LOGW(TAG, "nvs unavailable for %s", key);
            return false;
        }

        size_t written_len = preferences.putUChar(key, value);
        if (written_len == 0)
        {
            ESP_LOGE(TAG, "put u8 failed %s", key);
            return false;
        }

        return true;
    }

    bool putFloatKey(const char *key, float value)
    {
        if (!has_nvs)
        {
            ESP_LOGW(TAG, "nvs unavailable for %s", key);
            return false;
        }

        size_t written_len = preferences.putFloat(key, value);
        if (written_len == 0)
        {
            ESP_LOGE(TAG, "put float failed %s", key);
            return false;
        }

        return true;
    }
} // namespace

namespace ConfigManager
{
    bool init()
    {
        applyDefaultConfig(runtime_config);

        has_nvs = preferences.begin(NVS_NAMESPACE, false);
        is_initialized = true;

        if (!has_nvs)
        {
            ESP_LOGE(TAG, "nvs open failed");
            return false;
        }

        loadRuntimeConfigFromNvs(runtime_config);
        ESP_LOGI(TAG, "config loaded for %s", runtime_config.device_id);
        return true;
    }

    bool loadDefaults()
    {
        applyDefaultConfig(runtime_config);

        if (!is_initialized)
        {
            return true;
        }

        if (!has_nvs)
        {
            ESP_LOGW(TAG, "defaults in ram only");
            return false;
        }

        bool wrote_device_id = putStringKey(KEY_DEVICE_ID, runtime_config.device_id);
        bool wrote_mode = putUCharKey(
            KEY_DEVICE_MODE,
            static_cast<uint8_t>(runtime_config.mode));
        bool wrote_thresholds = saveThresholds(runtime_config.thresholds);
        bool wrote_led = saveLedState(runtime_config.led);
        bool wrote_energy = saveEnergyTotal(runtime_config.energy_total_wh);

        return wrote_device_id &&
               wrote_mode &&
               wrote_thresholds &&
               wrote_led &&
               wrote_energy;
    }

    const RuntimeConfig &getConfig()
    {
        return runtime_config;
    }

    bool saveThresholds(const ThresholdConfig &thresholds)
    {
        if (!isThresholdConfigValid(thresholds))
        {
            ESP_LOGW(TAG, "threshold save rejected");
            return false;
        }

        runtime_config.thresholds = thresholds;

        bool wrote_temp_min = putFloatKey(
            KEY_TEMP_MIN_C,
            runtime_config.thresholds.temp_min_c);
        bool wrote_temp_max = putFloatKey(
            KEY_TEMP_MAX_C,
            runtime_config.thresholds.temp_max_c);
        bool wrote_humidity_min = putFloatKey(
            KEY_HUMIDITY_MIN_PCT,
            runtime_config.thresholds.humidity_min_pct);
        bool wrote_humidity_max = putFloatKey(
            KEY_HUMIDITY_MAX_PCT,
            runtime_config.thresholds.humidity_max_pct);

        return wrote_temp_min &&
               wrote_temp_max &&
               wrote_humidity_min &&
               wrote_humidity_max;
    }

    bool saveMode(DeviceMode mode)
    {
        runtime_config.mode = mode;

        return putUCharKey(
            KEY_DEVICE_MODE,
            static_cast<uint8_t>(runtime_config.mode));
    }

    bool saveLedState(const LedState &led)
    {
        if (!isLedStateValid(led))
        {
            ESP_LOGW(TAG, "led save rejected");
            return false;
        }

        runtime_config.led = led;

        bool wrote_power = putBoolKey(KEY_LED_POWER, runtime_config.led.power);
        bool wrote_brightness = putUCharKey(
            KEY_LED_BRIGHTNESS_PCT,
            runtime_config.led.brightness_pct);
        bool wrote_red_enabled = putBoolKey(
            KEY_LED_RED_ENABLED,
            runtime_config.led.red_enabled);
        bool wrote_blue_enabled = putBoolKey(
            KEY_LED_BLUE_ENABLED,
            runtime_config.led.blue_enabled);
        bool wrote_far_red_enabled = putBoolKey(
            KEY_LED_FAR_RED_ENABLED,
            runtime_config.led.far_red_enabled);
        bool wrote_red_pct = putUCharKey(
            KEY_LED_RED_PCT,
            runtime_config.led.red_dist_pct);
        bool wrote_blue_pct = putUCharKey(
            KEY_LED_BLUE_PCT,
            runtime_config.led.blue_dist_pct);
        bool wrote_far_red_pct = putUCharKey(
            KEY_LED_FAR_RED_PCT,
            runtime_config.led.far_red_dist_pct);

        return wrote_power &&
               wrote_brightness &&
               wrote_red_enabled &&
               wrote_blue_enabled &&
               wrote_far_red_enabled &&
               wrote_red_pct &&
               wrote_blue_pct &&
               wrote_far_red_pct;
    }

    bool saveEnergyTotal(float total_wh)
    {
        if (!isEnergyTotalValid(total_wh))
        {
            ESP_LOGW(TAG, "energy save rejected");
            return false;
        }

        runtime_config.energy_total_wh = total_wh;
        return putFloatKey(KEY_ENERGY_TOTAL_WH, runtime_config.energy_total_wh);
    }
} // namespace ConfigManager
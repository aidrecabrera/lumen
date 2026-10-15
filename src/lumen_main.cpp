#include <Arduino.h>
#include <FFat.h>
#include <esp_err.h>
#include <esp_idf_version.h>
#include <esp_log.h>
#include <esp_task_wdt.h>

#include <memory>
#include <new>

#include "lumen_board_config.h"
#include "lumen_cloud_config.h"
#include "lumen_config_manager.h"
#include "lumen_energy_tracker.h"
#include "lumen_led_control.h"
#include "lumen_local_transport.h"
#include "lumen_mqtt_client.h"
#include "lumen_sensor_manager.h"
#include "lumen_setup_portal.h"
#include "lumen_task_manager.h"
#include "lumen_wifi_manager.h"

namespace {
static const char* TAG = "main";

RTC_DATA_ATTR static uint8_t boot_failure_count = 0;

static constexpr const char* CLOUD_CONFIG_PATH = "/cloud.json";

// lives for the whole run: the tls client keeps pointers into its pem strings.
static CloudConfig cloud_config;

bool initWatchdog() {
#if ESP_IDF_VERSION_MAJOR >= 5
    esp_task_wdt_config_t watchdog_config = {};
    watchdog_config.timeout_ms = WATCHDOG_TIMEOUT_MS;
    watchdog_config.idle_core_mask = (1U << portNUM_PROCESSORS) - 1U;
    watchdog_config.trigger_panic = true;

    const esp_err_t init_result = esp_task_wdt_init(&watchdog_config);
#else
    const esp_err_t init_result = esp_task_wdt_init(WATCHDOG_TIMEOUT_MS / 1000U, true);
#endif
    // IDF 4.4 returns only OK or NO_MEM. On IDF 5, INVALID_STATE means the 10 s config was not
    // applied, so it must fail boot too.
    if (init_result != ESP_OK) {
        ESP_LOGE(TAG, "watchdog init failed: %s", esp_err_to_name(init_result));
        return false;
    }

    // INVALID_STATE here means the TWDT is not initialized (esp_task_wdt.h:81). That cannot
    // happen after init succeeded, and accepting it would leave this task unwatched.
    const esp_err_t add_result = esp_task_wdt_add(nullptr);
    if (add_result != ESP_OK) {
        ESP_LOGE(TAG, "watchdog add failed: %s", esp_err_to_name(add_result));
        return false;
    }

    return true;
}

void enterSafeMode() {
    ESP_LOGE(TAG, "entering safe mode");
    while (true) {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void failBoot(const char* reason) {
    ++boot_failure_count;
    ESP_LOGE(TAG, "boot failed: %s (%u/%u)", reason, boot_failure_count, REBOOT_THRESHOLD);

    if (boot_failure_count >= REBOOT_THRESHOLD) {
        ESP_LOGE(TAG, "reboot threshold reached");
        enterSafeMode();
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    ESP.restart();
}

CloudState loadCloudConfig() {
    File file = FFat.open(CLOUD_CONFIG_PATH, "r");
    if (!file) {
        ESP_LOGI(TAG, "no %s, remote path off", CLOUD_CONFIG_PATH);
        return CloudState::NOT_CONFIGURED;
    }

    const size_t size = file.size();
    std::unique_ptr<char[]> text(new (std::nothrow) char[size + 1U]);
    if (size > CLOUD_CONFIG_FILE_MAX_LEN || !text || file.readBytes(text.get(), size) != size) {
        ESP_LOGE(TAG, "cloud config unreadable (%u bytes)", static_cast<unsigned>(size));
        return CloudState::INVALID_CONFIG;
    }

    const CloudConfigParse result = parseCloudConfig(text.get(), size, cloud_config);
    if (result != CloudConfigParse::OK) {
        ESP_LOGE(TAG, "cloud config invalid (code %u)", static_cast<unsigned>(result));
        return CloudState::INVALID_CONFIG;
    }

    return CloudState::CONNECTING;
}

void startRemotePath(const char* device_id) {
    const CloudState loaded = loadCloudConfig();
    if (loaded != CloudState::CONNECTING) {
        MqttClient::disable(loaded);
        return;
    }

    if (!MqttClient::init(device_id, cloud_config)) {
        MqttClient::disable(CloudState::INVALID_CONFIG);
    }
}

void startLocalPath() {
    const std::optional<LocalPasswordHash> password_hash = ConfigManager::loadLocalPasswordHash();
    if (!password_hash) {
        ESP_LOGW(TAG, "no local password saved, local dashboard off");
        return;
    }

    LocalTransport::init(WifiManager::hostname(), *password_hash);
}

// failBoot() does not return: it restarts the chip or parks it in safe mode.
void requireBootStep(bool step_ok, const char* step_name) {
    if (!step_ok) {
        failBoot(step_name);
    }
    esp_task_wdt_reset();
}
}  // namespace

#ifndef PIO_UNIT_TESTING
void setup() {
    Serial.begin(115200);
    delay(100);

    ESP_LOGI(TAG, "lumen firmware starting (boot failures: %u)", boot_failure_count);

    requireBootStep(initWatchdog(), "watchdog");

    if (!ConfigManager::init()) {
        ESP_LOGW(TAG, "config degraded to defaults");
    }
    esp_task_wdt_reset();

    requireBootStep(Sensors::init(), "sensors");
    requireBootStep(LedControl::init(), "led control");
    requireBootStep(EnergyTracker::init(), "energy tracker");

    SetupPortal::runIfNeeded();
    esp_task_wdt_reset();

    requireBootStep(WifiManager::init(), "wifi");

    if (!FFat.begin(false)) {
        ESP_LOGW(TAG, "ffat mount failed: upload the filesystem image with make fs-image");
    }

    // both paths are optional: the lighting runs without either.
    const RuntimeConfig config = ConfigManager::getConfigSnapshot();
    startLocalPath();
    startRemotePath(config.device_id);
    esp_task_wdt_reset();

    TaskManager::createQueues();
    requireBootStep(TaskManager::createTasks(), "tasks");

    boot_failure_count = 0;
    ESP_LOGI(TAG, "boot complete for %s", config.device_id);
}

void loop() {
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(1000));
}
#endif

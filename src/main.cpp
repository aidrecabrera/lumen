#include <Arduino.h>
#include <esp_idf_version.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

#include "command_handler.h"
#include "config.h"
#include "config_manager.h"
#include "energy_tracker.h"
#include "led_control.h"
#include "mqtt_client.h"
#include "sensors.h"
#include "task_manager.h"
#include "wifi_manager.h"

namespace {
static const char* TAG = "main";
static uint8_t boot_failure_count = 0;

bool initWatchdog() {
#if ESP_IDF_VERSION_MAJOR >= 5
    esp_task_wdt_config_t watchdog_config = {};
    watchdog_config.timeout_ms = WATCHDOG_TIMEOUT_MS;
    watchdog_config.idle_core_mask = 0;
    watchdog_config.trigger_panic = true;

    esp_err_t init_result = esp_task_wdt_init(&watchdog_config);
    if (init_result != ESP_OK) {
        ESP_LOGE(TAG, "watchdog init failed");
        return false;
    }
#else
    esp_err_t init_result = esp_task_wdt_init(WATCHDOG_TIMEOUT_MS / 1000, true);
    if (init_result != ESP_OK) {
        ESP_LOGE(TAG, "watchdog init failed");
        return false;
    }
#endif

    esp_err_t add_result = esp_task_wdt_add(nullptr);
    if (add_result != ESP_OK) {
        ESP_LOGE(TAG, "watchdog add failed");
        return false;
    }

    return true;
}

void restartNow() {
    delay(100);
    ESP.restart();
}

void failBoot(const char* reason) {
    ++boot_failure_count;

    ESP_LOGE(TAG, "boot failed: %s (%u/%u)", reason, boot_failure_count, REBOOT_THRESHOLD);

    if (boot_failure_count >= REBOOT_THRESHOLD) {
        ESP_LOGE(TAG, "reboot threshold reached");
    }

    restartNow();
}
}  // namespace

void setup() {
    Serial.begin(115200);
    delay(100);

    ESP_LOGI(TAG, "spot firmware starting");

    bool has_watchdog = initWatchdog();
    if (!has_watchdog) {
        failBoot("watchdog");
    }

    bool has_config = ConfigManager::init();
    if (!has_config) {
        ESP_LOGW(TAG, "config degraded to defaults");
    }

    bool has_sensors = Sensors::init();
    if (!has_sensors) {
        failBoot("sensors");
    }

    bool has_led = LedControl::init();
    if (!has_led) {
        failBoot("led control");
    }

    bool has_energy = EnergyTracker::init();
    if (!has_energy) {
        failBoot("energy tracker");
    }

    bool has_wifi = WifiManager::init();
    if (!has_wifi) {
        failBoot("wifi");
    }

    const RuntimeConfig& config = ConfigManager::getConfig();
    bool has_mqtt = MqttClient::init(config.device_id);
    if (!has_mqtt) {
        failBoot("mqtt");
    }

    bool has_queues = TaskManager::createQueues();
    if (!has_queues) {
        failBoot("queues");
    }

    bool has_handler = CommandHandler::init();
    if (!has_handler) {
        failBoot("command handler");
    }

    bool has_tasks = TaskManager::createTasks();
    if (!has_tasks) {
        failBoot("tasks");
    }

    boot_failure_count = 0;
    ESP_LOGI(TAG, "boot complete for %s", config.device_id);
}

void loop() {
    esp_task_wdt_reset();
    delay(1000);
}
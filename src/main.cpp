#include <Arduino.h>
#include <esp_err.h>
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

RTC_DATA_ATTR static uint8_t boot_failure_count = 0;

bool initWatchdog() {
#if ESP_IDF_VERSION_MAJOR >= 5
    esp_task_wdt_config_t watchdog_config = {};
    watchdog_config.timeout_ms = WATCHDOG_TIMEOUT_MS;
    watchdog_config.idle_core_mask = (1U << portNUM_PROCESSORS) - 1U;
    watchdog_config.trigger_panic = true;

    const esp_err_t init_result = esp_task_wdt_init(&watchdog_config);
    if (init_result != ESP_OK && init_result != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "watchdog init failed: %s", esp_err_to_name(init_result));
        return false;
    }
#else
    const esp_err_t init_result = esp_task_wdt_init(WATCHDOG_TIMEOUT_MS / 1000U, true);
    if (init_result != ESP_OK && init_result != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "watchdog init failed: %s", esp_err_to_name(init_result));
        return false;
    }
#endif

    const esp_err_t add_result = esp_task_wdt_add(nullptr);
    if (add_result != ESP_OK && add_result != ESP_ERR_INVALID_STATE) {
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
}  // namespace

void setup() {
    Serial.begin(115200);
    delay(100);

    ESP_LOGI(TAG, "spot firmware starting (boot failures: %u)", boot_failure_count);

    if (!initWatchdog()) {
        failBoot("watchdog");
    }
    esp_task_wdt_reset();

    if (!ConfigManager::init()) {
        ESP_LOGW(TAG, "config degraded to defaults");
    }
    esp_task_wdt_reset();

    if (!Sensors::init()) {
        failBoot("sensors");
    }
    esp_task_wdt_reset();

    if (!LedControl::init()) {
        failBoot("led control");
    }
    esp_task_wdt_reset();

    if (!EnergyTracker::init()) {
        failBoot("energy tracker");
    }
    esp_task_wdt_reset();

    if (!WifiManager::init()) {
        failBoot("wifi");
    }
    esp_task_wdt_reset();

    const RuntimeConfig& config = ConfigManager::getConfig();
    if (!MqttClient::init(config.device_id)) {
        failBoot("mqtt");
    }
    esp_task_wdt_reset();

    if (!TaskManager::createQueues()) {
        failBoot("queues");
    }
    esp_task_wdt_reset();

    if (!CommandHandler::init()) {
        failBoot("command handler");
    }
    esp_task_wdt_reset();

    if (!TaskManager::createTasks()) {
        failBoot("tasks");
    }
    esp_task_wdt_reset();

    boot_failure_count = 0;
    ESP_LOGI(TAG, "boot complete for %s", config.device_id);
}

void loop() {
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(1000));
}
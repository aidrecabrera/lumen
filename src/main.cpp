#include <Arduino.h>
#include <esp_idf_version.h>
#include <esp_log.h>
#include <esp_task_wdt.h>
#include <esp_system.h>

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


bool initWatchdog()
{
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
    esp_err_t init_result = esp_task_wdt_init(
        WATCHDOG_TIMEOUT_MS / 1000,
        true);
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


void restartNow()
{
    delay(100);
    ESP.restart();
}
}  // namespace


void setup()
{
    Serial.begin(115200);
    delay(100);

    bool has_watchdog = initWatchdog();
    if (!has_watchdog) {
        restartNow();
    }

    bool has_config = ConfigManager::init();
    if (!has_config) {
        ESP_LOGW(TAG, "config init failed");
    }

    bool has_sensors = Sensors::init();
    if (!has_sensors) {
        ESP_LOGE(TAG, "sensor init failed");
        restartNow();
    }

    bool has_led = LedControl::init();
    if (!has_led) {
        ESP_LOGE(TAG, "led init failed");
        restartNow();
    }

    bool has_energy = EnergyTracker::init();
    if (!has_energy) {
        ESP_LOGE(TAG, "energy init failed");
        restartNow();
    }

    bool has_wifi = WifiManager::init();
    if (!has_wifi) {
        ESP_LOGE(TAG, "wifi init failed");
        restartNow();
    }

    const RuntimeConfig& config = ConfigManager::getConfig();
    bool has_mqtt = MqttClient::init(config.device_id);
    if (!has_mqtt) {
        ESP_LOGE(TAG, "mqtt init failed");
        restartNow();
    }

    bool has_queues = TaskManager::createQueues();
    if (!has_queues) {
        ESP_LOGE(TAG, "queue init failed");
        restartNow();
    }

    bool has_handler = CommandHandler::init();
    if (!has_handler) {
        ESP_LOGE(TAG, "command handler init failed");
        restartNow();
    }

    bool has_tasks = TaskManager::createTasks();
    if (!has_tasks) {
        ESP_LOGE(TAG, "task init failed");
        restartNow();
    }

    ESP_LOGI(TAG, "boot complete");
}


void loop()
{
    esp_task_wdt_reset();
    delay(1000);
}

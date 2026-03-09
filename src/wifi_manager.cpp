#include "wifi_manager.h"

#include <WiFi.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_timer.h>
#include <stdio.h>

#include "config.h"
#include "secrets.h"

namespace {
static const char* TAG = "wifi_manager";

static bool is_initialized = false;
static bool was_connected = false;
static uint8_t degraded_tier = 0;
static uint8_t failure_count = 0;
static uint32_t backoff_ms = WIFI_BACKOFF_BASE_MS;
static uint64_t next_attempt_ms = 0;

uint64_t getNowMs() { return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL); }

uint32_t nextBackoffMs(uint32_t current_backoff_ms) {
    if (current_backoff_ms >= WIFI_BACKOFF_MAX_MS) {
        return WIFI_BACKOFF_MAX_MS;
    }

    uint32_t doubled_backoff_ms = current_backoff_ms * 2U;
    if (doubled_backoff_ms > WIFI_BACKOFF_MAX_MS) {
        return WIFI_BACKOFF_MAX_MS;
    }

    return doubled_backoff_ms;
}

void resetState() {
    degraded_tier = 0;
    failure_count = 0;
    backoff_ms = WIFI_BACKOFF_BASE_MS;
    next_attempt_ms = 0;
}

bool buildHostname(char* hostname, size_t hostname_len) {
    uint8_t base_mac[6] = {};
    esp_err_t read_result = esp_read_mac(base_mac, ESP_MAC_WIFI_STA);

    if (read_result != ESP_OK) {
        return false;
    }

    int written = snprintf(
        hostname,
        hostname_len,
        "%s%02x%02x%02x",
        WIFI_HOSTNAME_PREFIX,
        base_mac[3],
        base_mac[4],
        base_mac[5]
    );

    if (written <= 0 || static_cast<size_t>(written) >= hostname_len) {
        return false;
    }

    return true;
}

void beginConnection() { WiFi.begin(WIFI_SSID, WIFI_PASSWORD); }

void recordReconnectFailure() {
    if (failure_count < 255) {
        ++failure_count;
    }

    if (failure_count >= WIFI_FAILURE_THRESHOLD) {
        degraded_tier = 2;
        return;
    }

    degraded_tier = 1;
}

void logConnectionChange(bool is_connected) {
    if (is_connected == was_connected) {
        return;
    }

    was_connected = is_connected;

    if (was_connected) {
        ESP_LOGI(TAG, "wifi connected");
        return;
    }

    ESP_LOGW(TAG, "wifi disconnected");
}
}  // namespace

namespace WifiManager {
bool init() {
    char hostname[32] = {};

    bool mode_ok = WiFi.mode(WIFI_STA);
    if (!mode_ok) {
        ESP_LOGE(TAG, "wifi station mode failed");
        return false;
    }

    bool has_hostname = buildHostname(hostname, sizeof(hostname));
    if (has_hostname) {
        bool hostname_ok = WiFi.setHostname(hostname);
        if (!hostname_ok) {
            ESP_LOGW(TAG, "hostname set failed");
        }
    }

    WiFi.setSleep(false);

    resetState();
    beginConnection();
    next_attempt_ms = getNowMs() + WIFI_STA_CONNECT_TIMEOUT_MS;
    is_initialized = true;

    ESP_LOGI(TAG, "wifi init started");
    return true;
}

bool connectOrPoll() {
    if (!is_initialized) {
        ESP_LOGW(TAG, "poll before init");
        return false;
    }

    bool is_connected = (WiFi.status() == WL_CONNECTED);
    logConnectionChange(is_connected);

    if (is_connected) {
        resetReconnect();
        return true;
    }

    uint64_t now_ms = getNowMs();
    if (now_ms < next_attempt_ms) {
        return false;
    }

    recordReconnectFailure();
    WiFi.disconnect();
    beginConnection();

    ESP_LOGW(TAG, "wifi reconnect %u", failure_count);

    next_attempt_ms = now_ms + backoff_ms;
    backoff_ms = nextBackoffMs(backoff_ms);
    return false;
}

bool isConnected() {
    if (!is_initialized) {
        return false;
    }

    return WiFi.status() == WL_CONNECTED;
}

uint8_t getDegradedTier() { return degraded_tier; }

void resetReconnect() { resetState(); }
}  // namespace WifiManager

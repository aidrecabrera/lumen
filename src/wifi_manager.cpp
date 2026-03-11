#include "wifi_manager.h"

#include <WiFi.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <stdio.h>

#include "config.h"
#include "secrets.h"
#include "utils.h"

namespace {
static const char* TAG = "wifi_manager";

static bool is_initialized = false;
static bool was_connected = false;
static bool connection_in_progress = false;
static uint8_t degraded_tier = 0;
static uint8_t failure_count = 0;
static uint32_t backoff_ms = WIFI_BACKOFF_BASE_MS;
static uint64_t next_attempt_ms = 0;
static uint64_t connect_deadline_ms = 0;

uint32_t nextBackoffMs(uint32_t current_backoff_ms) {
    if (current_backoff_ms >= WIFI_BACKOFF_MAX_MS) {
        return WIFI_BACKOFF_MAX_MS;
    }

    const uint32_t doubled_backoff_ms = current_backoff_ms * 2U;
    return (doubled_backoff_ms > WIFI_BACKOFF_MAX_MS) ? WIFI_BACKOFF_MAX_MS
                                                      : doubled_backoff_ms;
}

void resetState() {
    degraded_tier = 0;
    failure_count = 0;
    backoff_ms = WIFI_BACKOFF_BASE_MS;
    next_attempt_ms = 0;
    connect_deadline_ms = 0;
    connection_in_progress = false;
}

bool buildHostname(char* hostname, size_t hostname_len) {
    uint8_t base_mac[6] = {};
    const esp_err_t read_result = esp_read_mac(base_mac, ESP_MAC_WIFI_STA);

    if (read_result != ESP_OK) {
        return false;
    }

    const int written = snprintf(
        hostname,
        hostname_len,
        "%s%02x%02x%02x",
        WIFI_HOSTNAME_PREFIX,
        base_mac[3],
        base_mac[4],
        base_mac[5]
    );

    return (written > 0) && (static_cast<size_t>(written) < hostname_len);
}

void startConnectionAttempt(uint64_t now_ms) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    connection_in_progress = true;
    connect_deadline_ms = now_ms + WIFI_STA_CONNECT_TIMEOUT_MS;
}

void scheduleReconnect(uint64_t now_ms) {
    const uint32_t jitter_window_ms = backoff_ms / 4U;
    const uint32_t jitter_ms =
        (jitter_window_ms == 0U) ? 0U : (esp_random() % (jitter_window_ms + 1U));

    next_attempt_ms = now_ms + backoff_ms + jitter_ms;
    backoff_ms = nextBackoffMs(backoff_ms);
}

void recordReconnectFailure() {
    if (failure_count < 255U) {
        ++failure_count;
    }

    degraded_tier = (failure_count >= WIFI_FAILURE_THRESHOLD) ? 2U : 1U;
}

void logConnectionChange(bool is_connected) {
    if (is_connected == was_connected) {
        return;
    }

    was_connected = is_connected;

    if (was_connected) {
        ESP_LOGI(TAG, "wifi connected");
    } else {
        ESP_LOGW(TAG, "wifi disconnected");
    }
}
}  // namespace

namespace WifiManager {
bool init() {
    char hostname[32] = {};

    WiFi.persistent(false);

    const bool mode_ok = WiFi.mode(WIFI_STA);
    if (!mode_ok) {
        ESP_LOGE(TAG, "wifi station mode failed");
        return false;
    }

    const bool has_hostname = buildHostname(hostname, sizeof(hostname));
    if (has_hostname && !WiFi.setHostname(hostname)) {
        ESP_LOGW(TAG, "hostname set failed");
    }

    WiFi.setAutoReconnect(false);
    WiFi.setSleep(false);
    WiFi.disconnect(false, false);

    resetState();
    startConnectionAttempt(getNowMs());
    is_initialized = true;

    ESP_LOGI(TAG, "wifi init started");
    return true;
}

bool connectOrPoll() {
    if (!is_initialized) {
        ESP_LOGW(TAG, "poll before init");
        return false;
    }

    const bool is_connected = (WiFi.status() == WL_CONNECTED);
    logConnectionChange(is_connected);

    if (is_connected) {
        resetReconnect();
        return true;
    }

    const uint64_t now_ms = getNowMs();

    if (connection_in_progress) {
        if (now_ms < connect_deadline_ms) {
            return false;
        }

        connection_in_progress = false;
        WiFi.disconnect(false, false);
        recordReconnectFailure();
        scheduleReconnect(now_ms);

        ESP_LOGW(TAG, "wifi connect timeout, failures=%u", failure_count);
        return false;
    }

    if (now_ms < next_attempt_ms) {
        return false;
    }

    startConnectionAttempt(now_ms);
    ESP_LOGI(TAG, "wifi connect attempt started");
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
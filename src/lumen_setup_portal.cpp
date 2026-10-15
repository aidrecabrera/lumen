#include "lumen_setup_portal.h"

#include <Arduino.h>
#include <WebAuthentication.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <stdio.h>

#include <optional>

#include "lumen_board_config.h"
#include "lumen_config_manager.h"
#include "lumen_system_utils.h"
#include "lumen_wifi_manager.h"

namespace {
static const char* TAG = "setup_portal";

std::optional<uint64_t> button_pressed_since_ms;

bool isNeeded() {
    if (!ConfigManager::loadWifiCredentials() || !ConfigManager::loadLocalPasswordHash()) {
        ESP_LOGI(TAG, "no setup saved");
        return true;
    }

    if (!ConfigManager::wifiHasConnected()) {
        ESP_LOGI(TAG, "saved wifi details never connected");
        return true;
    }

    return false;
}

void saveSetup(const char* local_password) {
    const std::optional<ValidWifiCredentials> credentials =
        ValidWifiCredentials::parse(WiFi.SSID().c_str(), WiFi.psk().c_str());
    const String digest = generateDigestHash(LOCAL_AUTH_USERNAME, local_password, LOCAL_AUTH_REALM);
    const std::optional<LocalPasswordHash> password_hash = LocalPasswordHash::parse(digest.c_str());

    if (!credentials || !password_hash || strlen(local_password) < LOCAL_PASSWORD_MIN_LEN) {
        ESP_LOGE(TAG, "setup values invalid, the portal opens again on the next boot");
        return;
    }

    if (ConfigManager::saveSetup(*credentials, *password_hash)) {
        ConfigManager::markWifiConnected();
    }
}

void runPortal() {
    const char* hostname = WifiManager::hostname();
    char network_name[40] = {};
    char address_note[200] = {};
    snprintf(
        network_name,
        sizeof(network_name),
        "Lumen-Setup-%s",
        hostname + strlen(WIFI_HOSTNAME_PREFIX)
    );
    snprintf(
        address_note,
        sizeof(address_note),
        "<p>After saving, open <b>http://%s.local</b> and sign in as <b>%s</b>. "
        "If that name does not open, use the IP address shown by your router.</p>",
        hostname,
        LOCAL_AUTH_USERNAME
    );

    WiFiManager portal;
    WiFiManagerParameter password_field(
        "local_password",
        "Local dashboard password (at least 8 characters)",
        "",
        64,
        "type='password' minlength='8' required"
    );
    WiFiManagerParameter address_field(address_note);
    portal.addParameter(&password_field);
    portal.addParameter(&address_field);
    portal.setTitle("Lumen setup");
    portal.setConfigPortalTimeout(SETUP_PORTAL_TIMEOUT_SEC);

    ESP_LOGI(TAG, "setup network %s open for %u s", network_name, SETUP_PORTAL_TIMEOUT_SEC);
    // the portal blocks for up to 10 minutes, longer than the task watchdog allows.
    esp_task_wdt_delete(nullptr);
    const bool connected = portal.startConfigPortal(network_name);
    esp_task_wdt_add(nullptr);

    if (!connected) {
        ESP_LOGW(TAG, "setup portal closed without a working wifi network");
        return;
    }

    saveSetup(password_field.getValue());
    ESP_LOGI(
        TAG,
        "setup saved, dashboard at http://%s.local or http://%s",
        hostname,
        WiFi.localIP().toString().c_str()
    );
}
}  // namespace

namespace SetupPortal {
void runIfNeeded() {
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    if (isNeeded()) {
        runPortal();
    }
}

void watchButton(uint64_t now_ms) {
    if (digitalRead(BOOT_BUTTON_PIN) == HIGH) {
        button_pressed_since_ms.reset();
        return;
    }

    if (!button_pressed_since_ms) {
        button_pressed_since_ms = now_ms;
        return;
    }

    if (now_ms - *button_pressed_since_ms < SETUP_BUTTON_HOLD_MS) {
        return;
    }

    // the saved wifi then counts as never connected, so the next boot opens the portal;
    // if the portal times out, the next good connection sets the flag again.
    ESP_LOGI(TAG, "boot held, restarting into the setup portal");
    ConfigManager::forgetWifiConnected();
    esp_restart();
}
}  // namespace SetupPortal

#include "lumen_type_validation.h"

#include <math.h>
#include <string.h>

#include "lumen_board_config.h"

namespace {
bool isCommandIdChar(char value) {
    const bool is_lower = (value >= 'a') && (value <= 'z');
    const bool is_upper = (value >= 'A') && (value <= 'Z');
    const bool is_digit = (value >= '0') && (value <= '9');
    const bool is_mark = (value == '-') || (value == '_');
    return is_lower || is_upper || is_digit || is_mark;
}
}  // namespace

std::optional<ValidThresholds> ValidThresholds::parse(const ThresholdConfig& raw) {
    if (!isfinite(raw.temp_min_c) || !isfinite(raw.temp_max_c) || !isfinite(raw.humidity_min_pct) ||
        !isfinite(raw.humidity_max_pct)) {
        return std::nullopt;
    }

    if (raw.temp_min_c > raw.temp_max_c) {
        return std::nullopt;
    }

    if (raw.humidity_min_pct > raw.humidity_max_pct) {
        return std::nullopt;
    }

    return ValidThresholds(raw);
}

std::optional<ValidLedState> ValidLedState::parse(const LedState& raw) {
    const uint16_t total_pct = static_cast<uint16_t>(raw.red_dist_pct) +
                               static_cast<uint16_t>(raw.blue_dist_pct) +
                               static_cast<uint16_t>(raw.far_red_dist_pct);

    if (raw.brightness_pct > 100U) {
        return std::nullopt;
    }

    if (total_pct != 100U) {
        return std::nullopt;
    }

    return ValidLedState(raw);
}

std::optional<EnergyTotalWh> EnergyTotalWh::parse(float raw_wh) {
    if (!isfinite(raw_wh) || raw_wh < 0.0f) {
        return std::nullopt;
    }

    return EnergyTotalWh(raw_wh);
}

std::optional<ValidLuxCeiling> ValidLuxCeiling::parse(float raw_lux) {
    if (!isfinite(raw_lux) || raw_lux < LUX_CEILING_MIN || raw_lux > LUX_CEILING_MAX) {
        return std::nullopt;
    }

    return ValidLuxCeiling(raw_lux);
}

std::optional<ValidWifiCredentials> ValidWifiCredentials::parse(
    const char* ssid, const char* password
) {
    if (ssid == nullptr || password == nullptr) {
        return std::nullopt;
    }

    const size_t ssid_len = strlen(ssid);
    const size_t password_len = strlen(password);
    const bool ssid_ok = (ssid_len > 0U) && (ssid_len <= WIFI_SSID_MAX_LEN);
    const bool password_ok =
        (password_len == 0U) || (password_len >= 8U && password_len <= WIFI_PASSWORD_MAX_LEN);
    if (!ssid_ok || !password_ok) {
        return std::nullopt;
    }

    ValidWifiCredentials credentials;
    memcpy(credentials.ssid_, ssid, ssid_len);
    memcpy(credentials.password_, password, password_len);
    return credentials;
}

std::optional<LocalPasswordHash> LocalPasswordHash::parse(const char* hex) {
    if (hex == nullptr || strlen(hex) != LOCAL_PASSWORD_HASH_LEN) {
        return std::nullopt;
    }

    for (size_t index = 0; index < LOCAL_PASSWORD_HASH_LEN; ++index) {
        const char value = hex[index];
        const bool is_hex = (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
        if (!is_hex) {
            return std::nullopt;
        }
    }

    LocalPasswordHash hash;
    memcpy(hash.hex_, hex, LOCAL_PASSWORD_HASH_LEN);
    return hash;
}

bool parseDeviceMode(uint8_t raw_mode, DeviceMode& out_mode) {
    if (raw_mode == static_cast<uint8_t>(DeviceMode::AUTONOMOUS)) {
        out_mode = DeviceMode::AUTONOMOUS;
        return true;
    }

    if (raw_mode == static_cast<uint8_t>(DeviceMode::MANUAL)) {
        out_mode = DeviceMode::MANUAL;
        return true;
    }

    return false;
}

bool parseCommandId(const char* text, size_t length, CommandId& out_id) {
    if (text == nullptr) {
        return false;
    }

    if (length == 0U || length >= COMMAND_ID_MAX_LEN) {
        return false;
    }

    for (size_t index = 0; index < length; ++index) {
        if (!isCommandIdChar(text[index])) {
            return false;
        }
    }

    memcpy(out_id.text_, text, length);
    out_id.text_[length] = '\0';
    return true;
}

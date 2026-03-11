#include "sensors.h"

#include <BH1750.h>
#include <DHT.h>
#include <Wire.h>
#include <esp_log.h>
#include <math.h>

#include "config.h"
#include "utils.h"

namespace {
static const char* TAG = "sensors";

static BH1750 light_sensor;
static DHT climate_sensor(DHT11_DATA_PIN, DHT11);

static bool is_initialized = false;
static bool is_healthy = false;
static uint32_t sample_sequence = 0;

bool readClimate(float& temperature_c, float& humidity_pct) {
    temperature_c = climate_sensor.readTemperature();
    humidity_pct = climate_sensor.readHumidity();

    if (!isfinite(temperature_c) || !isfinite(humidity_pct)) {
        return false;
    }

    return true;
}

bool readLight(float& light_lux) {
    light_lux = light_sensor.readLightLevel();

    if (!isfinite(light_lux)) {
        return false;
    }

    if (light_lux < 0.0f) {
        return false;
    }

    return true;
}

void setHealth(bool next_health) {
    if (is_healthy == next_health) {
        return;
    }

    is_healthy = next_health;

    if (is_healthy) {
        ESP_LOGI(TAG, "sensor health restored");
    } else {
        ESP_LOGW(TAG, "sensor health lost");
    }
}

bool fillReading(SensorReading& out) {
    float light_lux = 0.0f;
    float temperature_c = 0.0f;
    float humidity_pct = 0.0f;

    const bool has_light = readLight(light_lux);
    const bool has_climate = readClimate(temperature_c, humidity_pct);

    if (!has_light || !has_climate) {
        return false;
    }

    out.timestamp_ms = getNowMs();
    out.light_lux = light_lux;
    out.temperature_c = temperature_c;
    out.humidity_pct = humidity_pct;
    out.sequence = ++sample_sequence;
    return true;
}
}  // namespace

namespace Sensors {
bool init() {
    const bool wire_ok = Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    if (!wire_ok) {
        ESP_LOGE(TAG, "i2c init failed");
        is_initialized = false;
        setHealth(false);
        return false;
    }

    climate_sensor.begin();

    const bool has_light_sensor = light_sensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
    if (!has_light_sensor) {
        ESP_LOGE(TAG, "bh1750 init failed");
        is_initialized = false;
        setHealth(false);
        return false;
    }

    sample_sequence = 0;
    is_initialized = true;
    setHealth(true);
    ESP_LOGI(TAG, "sensors initialized");
    return true;
}

bool readCurrent(SensorReading& out) {
    if (!is_initialized) {
        ESP_LOGW(TAG, "read before init");
        setHealth(false);
        return false;
    }

    const bool was_filled = fillReading(out);
    setHealth(was_filled);
    return was_filled;
}

bool isHealthy() { return is_healthy; }
}  // namespace Sensors
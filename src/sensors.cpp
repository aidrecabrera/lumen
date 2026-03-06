#include "sensors.h"

#include <math.h>

#include <BH1750.h>
#include <DHT.h>
#include <Wire.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "config.h"

namespace {
static const char* TAG = "sensors";

static BH1750 light_sensor;
static DHT climate_sensor(DHT11_DATA_PIN, DHT11);

static bool is_initialized = false;
static bool is_healthy = false;
static uint32_t sample_sequence = 0;


uint64_t getNowMs()
{
    return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
}


bool readClimateOnce(float& temperature_c, float& humidity_pct)
{
    temperature_c = climate_sensor.readTemperature();
    humidity_pct = climate_sensor.readHumidity();

    if (isnan(temperature_c) || isnan(humidity_pct)) {
        return false;
    }

    return true;
}


bool readClimate(float& temperature_c, float& humidity_pct)
{
    bool was_read = readClimateOnce(temperature_c, humidity_pct);
    if (was_read) {
        return true;
    }

    return readClimateOnce(temperature_c, humidity_pct);
}


bool readLight(float& light_lux)
{
    light_lux = light_sensor.readLightLevel();

    if (isnan(light_lux)) {
        return false;
    }

    if (light_lux < 0.0f) {
        return false;
    }

    return true;
}


void setHealth(bool next_health)
{
    if (is_healthy == next_health) {
        return;
    }

    is_healthy = next_health;

    if (is_healthy) {
        ESP_LOGI(TAG, "sensor health restored");
        return;
    }

    ESP_LOGW(TAG, "sensor health lost");
}


bool fillReading(SensorReading& out)
{
    float light_lux = 0.0f;
    float temperature_c = 0.0f;
    float humidity_pct = 0.0f;

    bool has_light = readLight(light_lux);
    bool has_climate = readClimate(temperature_c, humidity_pct);

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
bool init()
{
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    climate_sensor.begin();

    bool has_light_sensor = light_sensor.begin(
        BH1750::CONTINUOUS_HIGH_RES_MODE);

    if (!has_light_sensor) {
        ESP_LOGE(TAG, "bh1750 init failed");
        is_initialized = false;
        setHealth(false);
        return false;
    }

    is_initialized = true;
    setHealth(true);
    ESP_LOGI(TAG, "sensors initialized");
    return true;
}


bool readCurrent(SensorReading& out)
{
    if (!is_initialized) {
        ESP_LOGW(TAG, "read before init");
        setHealth(false);
        return false;
    }

    bool was_filled = fillReading(out);
    setHealth(was_filled);

    if (!was_filled) {
        return false;
    }

    return true;
}


bool isHealthy()
{
    return is_healthy;
}
}  // namespace Sensors

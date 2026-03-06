#include "sensors.h"

#include <BH1750.h>
#include <DHT.h>
#include <Wire.h>
#include <esp_log.h>

#include "config.h"

namespace {
static const char* TAG = "sensors";

static BH1750 light_sensor;
static DHT climate_sensor(DHT11_DATA_PIN, DHT11);

static bool is_initialized = false;
static bool is_healthy = false;


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
    (void)out;

    if (!is_initialized) {
        setHealth(false);
        return false;
    }

    return false;
}


bool isHealthy()
{
    return is_healthy;
}
}  // namespace Sensors

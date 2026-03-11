#include <Arduino.h>
#include <unity.h>

#include "lumen_app_types.h"
#include "lumen_sensor_manager.h"

void test_sensors_init_succeeds() {
    TEST_ASSERT_TRUE(Sensors::init());
}

void test_sensors_readCurrent_succeeds_and_sequence_increments() {
    SensorReading first{};
    SensorReading second{};

    TEST_ASSERT_TRUE(Sensors::init());

    delay(2500);
    TEST_ASSERT_TRUE(Sensors::readCurrent(first));

    delay(2100);
    TEST_ASSERT_TRUE(Sensors::readCurrent(second));

    TEST_ASSERT_EQUAL_UINT32(first.sequence + 1U, second.sequence);
}

void test_sensors_health_true_after_successful_read() {
    SensorReading reading{};

    TEST_ASSERT_TRUE(Sensors::init());

    delay(2500);
    TEST_ASSERT_TRUE(Sensors::readCurrent(reading));
    TEST_ASSERT_TRUE(Sensors::isHealthy());
}
#pragma once

#include <stdint.h>

static constexpr uint8_t I2C_SDA_PIN = 21;
static constexpr uint8_t I2C_SCL_PIN = 22;
static constexpr uint8_t DHT11_DATA_PIN = 4;
static constexpr uint8_t LED_DATA_PIN = 18;
static constexpr uint16_t LED_PIXEL_COUNT = 16;

static constexpr uint32_t WIFI_STA_CONNECT_TIMEOUT_MS = 15000;
static constexpr const char* WIFI_HOSTNAME_PREFIX = "lumen-";

static constexpr uint32_t TASK_SENSORS_STACK_BYTES = 6144;
static constexpr uint32_t TASK_LED_STACK_BYTES = 6144;
static constexpr uint32_t TASK_NETWORK_STACK_BYTES = 12288;

static constexpr uint8_t TASK_SENSORS_PRIORITY = 2;
static constexpr uint8_t TASK_LED_PRIORITY = 3;
static constexpr uint8_t TASK_NETWORK_PRIORITY = 4;

static constexpr uint8_t TASK_SENSORS_CORE = 1;
static constexpr uint8_t TASK_LED_CORE = 1;
static constexpr uint8_t TASK_NETWORK_CORE = 0;

static constexpr uint32_t SENSOR_INTERVAL_MS = 2000;
static constexpr uint32_t LED_CONTROL_INTERVAL_MS = 250;
static constexpr uint32_t ENERGY_PUBLISH_INTERVAL_MS = 10000;
static constexpr uint32_t ENERGY_PERSIST_INTERVAL_MS = 30000;

static constexpr uint8_t QUEUE_SENSOR_TO_NETWORK_LENGTH = 8;
static constexpr uint8_t QUEUE_SENSOR_TO_LED_LENGTH = 4;
static constexpr uint8_t QUEUE_COMMAND_LENGTH = 8;
static constexpr uint8_t QUEUE_STATUS_LENGTH = 4;
static constexpr uint8_t QUEUE_ENERGY_LENGTH = 4;
static constexpr uint8_t QUEUE_ACK_LENGTH = 8;

static constexpr const char* MQTT_TOPIC_ROOT = "lumen";
static constexpr const char* MQTT_TOPIC_AVAILABILITY_SUFFIX = "availability";
static constexpr const char* MQTT_TOPIC_TELEMETRY_SUFFIX = "telemetry";
static constexpr const char* MQTT_TOPIC_STATUS_SUFFIX = "status";
static constexpr const char* MQTT_TOPIC_ENERGY_SUFFIX = "energy";
static constexpr const char* MQTT_TOPIC_ACK_PREFIX = "ack";
static constexpr const char* MQTT_TOPIC_CMD_LED_SUFFIX = "command/led";
static constexpr const char* MQTT_TOPIC_CMD_CONFIG_SUFFIX = "command/config";
static constexpr const char* MQTT_TOPIC_CMD_MODE_SUFFIX = "command/mode";

static constexpr uint16_t MQTT_KEEPALIVE_SEC = 30;

// the cloud path is slower to fit the aws iot free tier.
static constexpr uint32_t STATUS_HEARTBEAT_INTERVAL_MS = 30000;
static constexpr uint32_t CLOUD_TELEMETRY_INTERVAL_MS = 30000;
static constexpr uint32_t CLOUD_ENERGY_INTERVAL_MS = 60000;

static constexpr uint16_t LOCAL_MAX_CLIENTS = 3;
static constexpr const char* LOCAL_AUTH_USERNAME = "lumen";
static constexpr const char* LOCAL_AUTH_REALM = "lumen";

static constexpr uint8_t BOOT_BUTTON_PIN = 0;
static constexpr uint32_t SETUP_BUTTON_HOLD_MS = 5000;
static constexpr uint32_t SETUP_PORTAL_TIMEOUT_SEC = 600;
// PubSubClient publishes at QoS 0 only. These set the will and subscribe QoS.
static constexpr uint8_t MQTT_QOS_STATUS = 1;
static constexpr uint8_t MQTT_QOS_COMMAND = 1;

static constexpr uint16_t TELEMETRY_PAYLOAD_MAX = 128;
static constexpr uint16_t STATUS_PAYLOAD_MAX = 384;
static constexpr uint16_t ENERGY_PAYLOAD_MAX = 128;
static constexpr uint16_t ACK_PAYLOAD_MAX = 256;

static constexpr uint32_t WIFI_BACKOFF_BASE_MS = 1000;
static constexpr uint32_t WIFI_BACKOFF_MAX_MS = 30000;
static constexpr uint8_t WIFI_FAILURE_THRESHOLD = 10;
static constexpr uint32_t WATCHDOG_TIMEOUT_MS = 10000;
static constexpr uint8_t REBOOT_THRESHOLD = 3;

static constexpr float DEFAULT_LUX_CEILING = 10000.0f;
static constexpr float LUX_CEILING_MIN = 100.0f;
static constexpr float LUX_CEILING_MAX = 65535.0f;

// the gap between the two levels stops flicker at dusk.
static constexpr float NIGHT_OFF_BELOW_LUX = 10.0f;
static constexpr float NIGHT_ON_ABOVE_LUX = 20.0f;

#ifndef LUMEN_LED_ESTIMATED_FULL_SCALE_POWER_W
#define LUMEN_LED_ESTIMATED_FULL_SCALE_POWER_W 12.0f
#endif

static constexpr float LED_ESTIMATED_FULL_SCALE_POWER_W = LUMEN_LED_ESTIMATED_FULL_SCALE_POWER_W;

#pragma once

#include <stdint.h>


static constexpr uint8_t  I2C_SDA_PIN       = 21;
static constexpr uint8_t  I2C_SCL_PIN       = 22;
static constexpr uint8_t  DHT11_DATA_PIN    = 4;
static constexpr uint8_t  LED_DATA_PIN      = 18;
static constexpr uint16_t LED_PIXEL_COUNT   = 16;


static constexpr uint32_t WIFI_STA_CONNECT_TIMEOUT_MS = 15000;
static constexpr const char* WIFI_HOSTNAME_PREFIX      = "spot-";


static constexpr uint32_t TASK_SENSORS_STACK_BYTES = 6144;
static constexpr uint32_t TASK_LED_STACK_BYTES     = 6144;
static constexpr uint32_t TASK_MQTT_STACK_BYTES    = 12288;

static constexpr uint8_t TASK_SENSORS_PRIORITY = 2;
static constexpr uint8_t TASK_LED_PRIORITY     = 3;
static constexpr uint8_t TASK_MQTT_PRIORITY    = 4;

static constexpr uint8_t TASK_SENSORS_CORE = 1;
static constexpr uint8_t TASK_LED_CORE     = 1;
static constexpr uint8_t TASK_MQTT_CORE    = 0;

static constexpr uint32_t SENSOR_INTERVAL_MS         = 2000;
static constexpr uint32_t LED_CONTROL_INTERVAL_MS     = 250;
static constexpr uint32_t ENERGY_PUBLISH_INTERVAL_MS  = 10000;
static constexpr uint32_t ENERGY_PERSIST_INTERVAL_MS  = 30000;

static constexpr uint8_t QUEUE_SENSOR_TO_MQTT_LENGTH = 8;
static constexpr uint8_t QUEUE_SENSOR_TO_LED_LENGTH  = 4;
static constexpr uint8_t QUEUE_COMMAND_LENGTH        = 8;
static constexpr uint8_t QUEUE_STATUS_LENGTH         = 4;
static constexpr uint8_t QUEUE_ENERGY_LENGTH         = 4;
static constexpr uint8_t QUEUE_ACK_LENGTH            = 8;


static constexpr const char* MQTT_BROKER_IP   = "192.168.1.50";
static constexpr uint16_t    MQTT_BROKER_PORT = 1883;

static constexpr const char* MQTT_TOPIC_ROOT              = "spot";
static constexpr const char* MQTT_TOPIC_TELEMETRY_SUFFIX  = "telemetry";
static constexpr const char* MQTT_TOPIC_STATUS_SUFFIX     = "status";
static constexpr const char* MQTT_TOPIC_ENERGY_SUFFIX     = "energy";
static constexpr const char* MQTT_TOPIC_ACK_PREFIX        = "ack";
static constexpr const char* MQTT_TOPIC_CMD_LED_SUFFIX    = "command/led";
static constexpr const char* MQTT_TOPIC_CMD_CONFIG_SUFFIX = "command/config";
static constexpr const char* MQTT_TOPIC_CMD_MODE_SUFFIX   = "command/mode";

static constexpr uint16_t MQTT_KEEPALIVE_SEC          = 30;
static constexpr uint32_t MQTT_HEARTBEAT_INTERVAL_MS  = 30000;
static constexpr uint8_t  MQTT_QOS_TELEMETRY          = 0;
static constexpr uint8_t  MQTT_QOS_STATUS             = 1;
static constexpr uint8_t  MQTT_QOS_ENERGY             = 1;
static constexpr uint8_t  MQTT_QOS_COMMAND             = 1;


static constexpr uint16_t TELEMETRY_DOC_SIZE = 128;
static constexpr uint16_t COMMAND_DOC_SIZE   = 256;
static constexpr uint16_t STATUS_DOC_SIZE    = 256;
static constexpr uint16_t ENERGY_DOC_SIZE    = 128;


static constexpr uint32_t WIFI_BACKOFF_BASE_MS   = 1000;
static constexpr uint32_t WIFI_BACKOFF_MAX_MS    = 30000;
static constexpr uint8_t  WIFI_FAILURE_THRESHOLD = 10;
static constexpr uint32_t WATCHDOG_TIMEOUT_MS    = 10000;
static constexpr uint8_t  REBOOT_THRESHOLD       = 3;

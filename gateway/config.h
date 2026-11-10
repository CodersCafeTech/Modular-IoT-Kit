#pragma once
#include <stdint.h>
// Gateway settings. KIT_ID is the MQTT topic and must match the Kit ID in the dashboard.

constexpr const char *KIT_ID = "UID123";

constexpr const char *MQTT_HOST = "test.mosquitto.org";  // public broker, no auth
constexpr uint16_t MQTT_PORT = 1883;
constexpr const char *MQTT_CLIENT_PREFIX = "modiot-gw-";  // + chip MAC, unique per gateway
constexpr uint32_t MQTT_RETRY_MS = 5000;

constexpr const char *SETUP_AP_NAME = "ModularIoT";      // WiFi setup hotspot
constexpr uint16_t SETUP_PORTAL_TIMEOUT_S = 180;         // restart if nobody configures WiFi

constexpr const char *BLE_DEVICE_NAME = "ModularIoT-GW";

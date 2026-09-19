#pragma once

// Wi-Fi defaults (development - Wokwi simulator)
#define WIFI_SSID     "Wokwi-GUEST"
#define WIFI_PASSWORD ""

// MQTT defaults (development only - switch to authenticated broker for production)
#define MQTT_BROKER "test.mosquitto.org"
#define MQTT_PORT   1883

// Device identifier – used as MQTT client ID and in topic namespace
// Topic pattern: railway/railsafe/<DEVICE_ID>/<subtopic>
#define DEVICE_ID "ESP32-RAILSAFE-01"

// LED usage flag – keep enabled (1) for hardware fallback; set to 0 to disable
#define USE_LEDS 1

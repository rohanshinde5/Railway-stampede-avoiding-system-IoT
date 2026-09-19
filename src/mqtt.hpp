// mqtt.hpp - Railway Crowd Management MQTT Integration
#ifndef MQTT_HPP
#define MQTT_HPP

#include <PubSubClient.h>
#include <WiFiClient.h>

#include "config.h"

// -- Extern globals defined in main.cpp -----------------------
extern int crowdCount;
extern const int MAX_CAPACITY;
extern bool emergencyMode;

// -- Forward declarations for main.cpp functions ---------------
extern void activateEmergency();
extern void resetSystem();
// getStatusName declared in main.cpp via CrowdStatus enum - use explicit forward declaration

// -- MQTT client objects ---------------------------------------
static WiFiClient espClient;
static PubSubClient mqttClient(espClient);

// -- Heartbeat timing ------------------------------------------
static unsigned long lastHeartbeat = 0;
static const unsigned long HEARTBEAT_INTERVAL = 15000UL; // 15 s

// -- Topic helper ----------------------------------------------
inline String makeTopic(const char* suffix) {
    return String("railway/railsafe/") + DEVICE_ID + "/" + suffix;
}

// -- Publish helpers -------------------------------------------
void publishState(int count, int capacity, const char* statusName) {
    int occupancy = (capacity > 0) ? (count * 100) / capacity : 0;
    char buf[300];
    snprintf(buf, sizeof(buf),
        "{\"deviceId\":\"%s\",\"crowdCount\":%d,\"capacity\":%d,"
        "\"occupancyPercent\":%d,\"status\":\"%s\","
        "\"emergency\":%s,\"gateState\":\"%s\",\"timestamp\":%lu}",
        DEVICE_ID, count, capacity, occupancy, statusName,
        emergencyMode ? "true" : "false",
        (strcmp(statusName, "NORMAL") == 0) ? "OPEN" :
            (strcmp(statusName, "WARNING") == 0) ? "PARTIAL" : "CLOSED",
        millis());
    mqttClient.publish(makeTopic("crowd/state").c_str(), buf);
}

void publishEntryEvent(int count) {
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"event\":\"ENTRY\",\"crowdCount\":%d,\"timestamp\":%lu}",
        count, millis());
    mqttClient.publish(makeTopic("crowd/event").c_str(), buf);
}

void publishExitEvent(int count) {
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"event\":\"EXIT\",\"crowdCount\":%d,\"timestamp\":%lu}",
        count, millis());
    mqttClient.publish(makeTopic("crowd/event").c_str(), buf);
}

void publishCommandConfirm(const char* command, bool success) {
    char buf[200];
    snprintf(buf, sizeof(buf),
        "{\"event\":\"%s\",\"command\":\"%s\",\"success\":%s,\"timestamp\":%lu}",
        success ? "COMMAND_EXECUTED" : "COMMAND_FAILED",
        command,
        success ? "true" : "false",
        millis());
    mqttClient.publish(makeTopic("system/command").c_str(), buf);
}

// -- MQTT callback ---------------------------------------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String t = String(topic);
    if (t.endsWith("/control/emergency")) {
        activateEmergency();
        publishCommandConfirm("EMERGENCY", true);
    } else if (t.endsWith("/control/reset")) {
        resetSystem();
        publishCommandConfirm("RESET", true);
    }
}

// -- MQTT reconnect (non-blocking attempt) ---------------------
static void mqttReconnect() {
    if (mqttClient.connect(DEVICE_ID)) {
        Serial.println("MQTT connected");
        mqttClient.subscribe(makeTopic("control/emergency").c_str());
        mqttClient.subscribe(makeTopic("control/reset").c_str());
        Serial.print("Subscribed to: railway/railsafe/");
        Serial.println(DEVICE_ID);
    } else {
        Serial.print("MQTT connect failed, rc=");
        Serial.println(mqttClient.state());
    }
}

// -- Setup -----------------------------------------------------
void setupMqtt() {
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    Serial.print("MQTT broker: ");
    Serial.print(MQTT_BROKER);
    Serial.print(":");
    Serial.println(MQTT_PORT);
    mqttReconnect();
}

// -- Maintain (call every loop) --------------------------------
void maintainMqtt() {
    if (!mqttClient.connected()) {
        static unsigned long lastRetry = 0;
        unsigned long now = millis();
        if (now - lastRetry >= 5000UL) {
            lastRetry = now;
            Serial.println("MQTT disconnected, retrying...");
            mqttReconnect();
        }
    }
    mqttClient.loop();

    // Heartbeat
    unsigned long now = millis();
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
        char hb[256];
        snprintf(hb, sizeof(hb),
            "{\"deviceId\":\"%s\",\"status\":\"ONLINE\","
            "\"crowdCount\":%d,\"uptime\":%lu,\"timestamp\":%lu}",
            DEVICE_ID, crowdCount, now / 1000, now);
        mqttClient.publish(makeTopic("system/heartbeat").c_str(), hb);
        lastHeartbeat = now;
    }
}

#endif // MQTT_HPP

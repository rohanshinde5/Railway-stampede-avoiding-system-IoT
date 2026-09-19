// wifi.hpp - Railway Crowd Management Wi-Fi Integration
#ifndef WIFI_HPP
#define WIFI_HPP

#include <WiFi.h>
#include "config.h"

void setupWifi() {
    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long startAttempt = millis();
    const unsigned long timeout = 20000UL; // 20 s
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < timeout) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Wi-Fi connected, IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Wi-Fi connection failed (will retry in loop)");
    }
}

void maintainWifi() {
    if (WiFi.status() != WL_CONNECTED) {
        static unsigned long lastRetry = 0;
        unsigned long now = millis();
        if (now - lastRetry >= 10000UL) {
            lastRetry = now;
            Serial.println("Wi-Fi lost, reconnecting...");
            WiFi.reconnect();
        }
    }
}

#endif // WIFI_HPP

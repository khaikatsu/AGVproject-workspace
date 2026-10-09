#include "WiFiModule.h"
#include "Config.h"
#include <WiFi.h>

void WiFiModule::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    // Let the ESP32 reconnect immediately after an AP drop; loop() still
    // requests a fresh connection periodically as a fallback.
    WiFi.setAutoReconnect(true);
    WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t info) {
        if (Config::DEBUG_ENABLED) Serial.printf("[WiFi] Disconnected reason=%u\n", info.wifi_sta_disconnected.reason);
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t) {
        if (Config::DEBUG_ENABLED) Serial.println("[WiFi] Got IP");
    }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    connect();
}
void WiFiModule::connect() {
    lastAttemptMs_ = millis();
    if (!Config::WIFI_SSID[0]) return;
    WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);
    if (Config::DEBUG_ENABLED) Serial.println("[WiFi] Connection requested");
}
void WiFiModule::reconnect() { connect(); }
void WiFiModule::loop() {
    if (!isConnected() && static_cast<uint32_t>(millis() - lastAttemptMs_) >= Config::WIFI_RETRY_MS) reconnect();
}
bool WiFiModule::isConnected() const { return WiFi.status() == WL_CONNECTED; }

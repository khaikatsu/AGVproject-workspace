#pragma once
#include <string>
constexpr int WL_CONNECTED=3,WIFI_STA=1;
struct HostIP {std::string toString() {return "127.0.0.1";}};
struct HostWiFi {
    int status() {return WL_CONNECTED;}
    void mode(int) {}
    void begin(const char*,const char*) {}
    void setAutoReconnect(bool) {}
    void reconnect() {}
    HostIP localIP() {return {};}
    HostIP gatewayIP() {return {};}
};
inline HostWiFi WiFi;
struct WiFiClient {void setConnectionTimeout(uint32_t) {}};

#pragma once

// Sao chép thành LocalNetworkConfig02.h để cấu hình mạng riêng cho xe 02.
// File .h thật nằm trong .gitignore. ID MQTT được khóa bởi Robot02Config.h.
#define AGV_WIFI_SSID "LAPTOP-ALJK4TUK 0260"
#define AGV_WIFI_PASSWORD "046x72A$"
// IP LAN của máy broker; không dùng 127.0.0.1 trên ESP32.
#define AGV_MQTT_HOST ""
#define AGV_MQTT_USER ""
#define AGV_MQTT_PASSWORD ""
#define AGV_MQTT_PORT 1883
#define AGV_MQTT_TLS false
#define AGV_MQTT_CA_PEM ""
#define AGV_NTP_SERVER "pool.ntp.org"


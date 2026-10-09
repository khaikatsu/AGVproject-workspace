#pragma once

// Bản mạng chung cũ: sao chép thành LocalNetworkConfig.h (bị ignore).
// Env bench 01/02 ưu tiên LocalNetworkConfig01.h/02.h nếu có.
// Bản một xe dùng ID 01; env bench mới khóa ID theo profile.
#define AGV_ROBOT_ID "01"
#define AGV_WIFI_SSID ""
#define AGV_WIFI_PASSWORD ""
// LAN address of the broker computer; 127.0.0.1 points to this ESP32.
#define AGV_MQTT_HOST ""
#define AGV_MQTT_USER ""
#define AGV_MQTT_PASSWORD ""
#define AGV_MQTT_PORT 1883
#define AGV_MQTT_TLS false
// For TLS, use 8883 and a broker certificate with a SAN matching AGV_MQTT_HOST.
// Paste the trusted root CA PEM here, e.g. R"PEM(-----BEGIN CERTIFICATE----- ... )PEM".
#define AGV_MQTT_CA_PEM ""
#define AGV_NTP_SERVER "pool.ntp.org"

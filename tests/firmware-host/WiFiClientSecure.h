#pragma once
#include "WiFi.h"
struct WiFiClientSecure : WiFiClient {void setCACert(const char*) {}};

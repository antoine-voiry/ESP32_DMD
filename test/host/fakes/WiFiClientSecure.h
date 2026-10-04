#ifndef FAKE_WIFI_CLIENT_SECURE_H
#define FAKE_WIFI_CLIENT_SECURE_H
#include "WiFi.h"
class WiFiClientSecure : public WiFiClient {
public:
    void setInsecure() {}
};
#endif

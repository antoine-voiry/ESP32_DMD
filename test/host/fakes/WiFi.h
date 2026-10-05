#ifndef FAKE_WIFI_H
#define FAKE_WIFI_H

#include <functional>

#include "Arduino.h"

enum wl_status_t { WL_IDLE_STATUS = 0, WL_CONNECTED = 3, WL_DISCONNECTED = 6 };
enum wifi_mode_t { WIFI_OFF, WIFI_STA, WIFI_AP, WIFI_AP_STA };
enum WiFiEvent_t { ARDUINO_EVENT_WIFI_STA_GOT_IP = 7, ARDUINO_EVENT_WIFI_STA_DISCONNECTED = 5, ARDUINO_EVENT_OTHER = 99 };

class WiFiClass {
public:
    wl_status_t status();
    IPAddress localIP();
    const char* getHostname();
    int RSSI();
    bool mode(wifi_mode_t) { return true; }
    bool setAutoReconnect(bool) { return true; }
    bool reconnect();
    void onEvent(void (*handler)(WiFiEvent_t));
};
extern WiFiClass WiFi;

class WiFiClient {
public:
    virtual ~WiFiClient() = default;
};

#endif

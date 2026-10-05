#ifndef FAKE_WIFIMANAGER_H
#define FAKE_WIFIMANAGER_H

// Behaviour set by fake::portal.

#include <functional>
#include <string>
#include <vector>

#include "WiFi.h"

class WiFiManagerParameter {
public:
    WiFiManagerParameter(const char* id, const char* label, const char* defaultValue, int length);
    const char* getID() const { return _id.c_str(); }
    const char* getValue() const { return _value.c_str(); }
    void setValue(const std::string& value) { _value = value; }

private:
    std::string _id;
    std::string _value;
};

class WiFiManager {
public:
    void setHostname(const char* hostname);
    void setEnableConfigPortal(bool enable);
    bool autoConnect();
    void addParameter(WiFiManagerParameter* p) { _params.push_back(p); }
    void setSaveConfigCallback(std::function<void()> cb) { _save = std::move(cb); }
    void setAPCallback(std::function<void(WiFiManager*)> cb) { _ap = std::move(cb); }
    void setConfigPortalTimeout(unsigned long seconds);
    bool startConfigPortal(const char* ssid, const char* password);

private:
    bool _portalEnabled = true;
    std::vector<WiFiManagerParameter*> _params;
    std::function<void()> _save;
    std::function<void(WiFiManager*)> _ap;
};

#endif

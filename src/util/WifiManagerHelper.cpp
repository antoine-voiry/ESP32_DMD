#include "WifiManagerHelper.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_wifi.h>

#include "ConfigHelper.h"

static const char* TAG = "WiFi";

namespace {

constexpr int kFieldLength = 255;
constexpr unsigned kPortalTimeoutSec = 600;

void onWiFiEvent(WiFiEvent_t event) {
    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        ESP_LOGI(TAG, "Connected, IP %s", WiFi.localIP().toString().c_str());
    } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Connection lost, reconnecting");
        WiFi.reconnect();
    }
}

}  // namespace

std::string WifiManagerHelper::newPortalPassword() {
    static const char kAlphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
    std::string password;
    for (int i = 0; i < 10; ++i) {
        password += kAlphabet[esp_random() % (sizeof(kAlphabet) - 1)];
    }
    return password;
}

void WifiManagerHelper::connect(bool forcePortal, const PortalNotice& notice) {
    ConfigHelper& config = ConfigHelper::getInstance();
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    esp_wifi_set_ps(WIFI_PS_NONE);  // power saving adds latency to MQTT messages
    WiFi.onEvent(onWiFiEvent);

    WiFiManager wm;
    if (!config.getHostname().empty()) {
        wm.setHostname(config.getHostname().c_str());
    }
    // Without this, a failed autoConnect() would open an unprotected access point.
    wm.setEnableConfigPortal(false);
    if (!forcePortal && wm.autoConnect()) {
        return;
    }

    const std::string password = newPortalPassword();
    WiFiManagerParameter mqttUrl("mqtt_url", "MQTT broker", config.getMqttUrl().c_str(), kFieldLength);
    WiFiManagerParameter mqttPath("mqtt_path", "MQTT topic", config.getMqttPath().c_str(), kFieldLength);
    WiFiManagerParameter hostname("hostname", "Hostname", config.getHostname().c_str(), kFieldLength);
    wm.addParameter(&mqttUrl);
    wm.addParameter(&mqttPath);
    wm.addParameter(&hostname);
    wm.setSaveConfigCallback([this]() { _saveRequested = true; });
    wm.setAPCallback([&](WiFiManager*) {
        ESP_LOGW(TAG, "Setup portal open: join '%s' with password '%s'", kPortalName, password.c_str());
        if (notice) notice(kPortalName, password);
    });
    wm.setConfigPortalTimeout(kPortalTimeoutSec);
    if (!wm.startConfigPortal(kPortalName, password.c_str())) {
        ESP_LOGE(TAG, "Setup portal closed without a connection, restarting");
        delay(1000);
        ESP.restart();
        return;
    }
    if (_saveRequested) {
        config.setMqttUrl(mqttUrl.getValue());
        config.setMqttPath(mqttPath.getValue());
        config.setHostname(hostname.getValue());
        config.saveConfigFile();
    }
}

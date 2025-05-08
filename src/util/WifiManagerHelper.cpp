#include <Arduino.h>

#include "WifiManagerHelper.h"

static const char* TAG = "WiFiManager";

// Define the portal name
#define PORTAL_NAME "DMD_CONFIG_WIFI"
#define MQTT_URL_ID "mqtt_url"
#define MQTT_URL_LABEL "MQTT URL"
#define MQTT_URL_LENGTH 255

#define MQTT_PATH_ID "mqtt_path"
#define MQTT_PATH_LABEL "MQTT Path"
#define MQTT_PATH_LENGTH 255

#define HOSTNAME_ID "hostname"
#define HOSTNAME_LABEL "Hostname"
#define HOSTNAME_LENGTH 255

// Save Config in JSON format
void WifiManagerHelper::saveConfigFile() {
    ESP_LOGI(TAG, "saveConfigFile - Saving config file");
    ConfigHelper::getInstance().saveConfigFile();
}
 
// Callback notifying us of the need to save configuration
void WifiManagerHelper::saveConfigCallback() {
    _callbackRegistered = true;  // Set flag to verify callback was called
    ESP_LOGI(TAG, "saveConfigCallback triggered");
    ESP_LOGD(TAG, "Previous _shouldSaveConfig value: %d", _shouldSaveConfig);
    _shouldSaveConfig = true;
    ESP_LOGD(TAG, "New _shouldSaveConfig value: %d", _shouldSaveConfig);
}
// Called when config mode launched
void WifiManagerHelper::configModeCallback(WiFiManager *myWiFiManager) {
    ESP_LOGI(TAG, "Entered Configuration Mode");
    ESP_LOGI(TAG, "Config SSID: %s, IP Address: %s", 
        myWiFiManager->getConfigPortalSSID().c_str(), 
        WiFi.softAPIP().toString().c_str());
}
void WifiManagerHelper::applydefaultWifiSettings() {
    // Configure WiFi for stability
    WiFi.persistent(true);
    WiFi.setAutoReconnect(true);   
    // Set power saving to WIFI_PS_NONE for better stability
    esp_wifi_set_ps(WIFI_PS_NONE);
    // Increase WiFi Tx power
    esp_wifi_set_max_tx_power(78);  // Maximum Tx power (in dBm)    
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // Register WiFi event handler
    WiFi.onEvent(WiFiEvent);
}
void WifiManagerHelper::setWMUp(boolean forceConfig, char* hostname) {
    WiFiManager wm;
    std::string mqtt_url_str;
    std::string mqtt_path_str;
    std::string host;

    // Apply default WiFi settings first
    applydefaultWifiSettings();

    // Load existing configuration
    if(ConfigHelper::getInstance().isConfigLoaded() && !forceConfig) {
        mqtt_url_str = ConfigHelper::getInstance().getMqttUrl();
        mqtt_path_str = ConfigHelper::getInstance().getMqttPath();
        host = ConfigHelper::getInstance().getHostname();
        
        // Set hostname if it's not empty
        if (!host.empty()) {
            ESP_LOGI(TAG, "Setting hostname to: %s", host.c_str());
            wm.setHostname(host.c_str());
            WiFi.setHostname(host.c_str());
        } else {
            ESP_LOGW(TAG, "Hostname is empty, using default");
        }

        // Try to connect with existing configuration
        ESP_LOGI(TAG, "Attempting to connect with saved configuration");
        if (wm.autoConnect("AutoConnectAP")) {
            ESP_LOGI(TAG, "Connected to WiFi using saved configuration");
            return;
        }
        // If autoConnect fails, fall through to configuration portal
        ESP_LOGW(TAG, "Failed to connect with saved configuration, starting config portal");
        forceConfig = true;
    }

    // Configuration portal needed
    if (forceConfig) {
        // Set callbacks before doing anything else
        wm.setAPCallback(std::bind(&WifiManagerHelper::configModeCallback, this, std::placeholders::_1));
        wm.setSaveConfigCallback(std::bind(&WifiManagerHelper::saveConfigCallback, this));

        // Get current values for the config portal with validation
        mqtt_url_str = ConfigHelper::getInstance().getMqttUrl();
        mqtt_path_str = ConfigHelper::getInstance().getMqttPath();
        host = ConfigHelper::getInstance().getHostname();

        // Provide default values if empty or invalid
        if (mqtt_url_str.empty()) mqtt_url_str = "";
        if (mqtt_path_str.empty()) mqtt_path_str = "";
        if (host.empty()) host = "";



        // Create parameters
        WiFiManagerParameter mqtt_url_box(MQTT_URL_ID, MQTT_URL_LABEL, 
            mqtt_url_str.c_str(), MQTT_URL_LENGTH);
        WiFiManagerParameter mqtt_path_box(MQTT_PATH_ID, MQTT_PATH_LABEL, 
            mqtt_path_str.c_str(), MQTT_PATH_LENGTH);
        WiFiManagerParameter hostname_box(HOSTNAME_ID, HOSTNAME_LABEL, 
            host.c_str(), HOSTNAME_LENGTH);

        // Add parameters to WiFiManager
        wm.addParameter(&mqtt_url_box);
        wm.addParameter(&mqtt_path_box);
        wm.addParameter(&hostname_box);

        // Configure portal behavior
        wm.setConfigPortalBlocking(true);
        wm.setBreakAfterConfig(true);

        // Start the portal
        ESP_LOGI(TAG, "Starting config portal");
        if (!wm.startConfigPortal(PORTAL_NAME, "12345678")) {
            ESP_LOGE(TAG, "Failed to connect or configure");
            delay(3000);
            ESP.restart();
        }

        // If we get here, the portal was successful
        if (_shouldSaveConfig) {
            // Only update config if the callback was triggered
            ESP_LOGI(TAG, "New configuration provided, saving...");
            ConfigHelper::getInstance().setMqttUrl(mqtt_url_box.getValue());
            ConfigHelper::getInstance().setMqttPath(mqtt_path_box.getValue());
            ConfigHelper::getInstance().setHostname(hostname_box.getValue());
            saveConfigFile();
            
            // Update current values
            mqtt_url_str = mqtt_url_box.getValue();
            mqtt_path_str = mqtt_path_box.getValue();
            host = hostname_box.getValue();
        }
    }

    // At this point we should be connected
    if (WiFi.status() == WL_CONNECTED) {
        ESP_LOGI(TAG, "WiFi connected, IP: %s", WiFi.localIP().toString().c_str());
        ESP_LOGI(TAG, "MQTT URL: %s, Path: %s, Hostname: %s", 
            mqtt_url_str.c_str(), mqtt_path_str.c_str(), host.c_str());
        
    } else {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        delay(3000);
        ESP.restart();
    }
}

void WifiManagerHelper::WiFiEvent(WiFiEvent_t event) {
    ESP_LOGD(TAG, "[WiFi-event] event: %d", event);
    switch(event) {
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "WiFi connected, IP: %s", WiFi.localIP().toString().c_str());
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGW(TAG, "WiFi lost connection");
            WiFi.reconnect();
            break;
    }
}
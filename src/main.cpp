#include <Arduino.h>
#include <WiFi.h>
#include <esp_log.h>
#include <ESPmDNS.h>  
#include "util/WifiManagerHelper.h"
#include "util/ConfigHelper.h"
#include "util/MQTTHelper.h"
#include "util/DMDRenderer.h"
#include "util/MessageHandler.h"
#include "util/MessageFilter.h"
#include "util/LocalWebServer.h"

static const char* TAG = "Main";  // Add this line for ESP_LOG tag

std::string  mqtt_url = "raspydarts.local"; // Replace with your MQTT broker address
std::string  mqtt_topic = "raspydarts/#";
std::string  mqtt_client_id = "esp32_client";
WifiManagerHelper *  wifiHelper = nullptr; 
DMDRenderer * dmdRenderer = nullptr; 
MessageHandler* messageHandler = nullptr; 
MessageFilter* messageFilter = nullptr; 
// Define the static member variable

MQTTHelper* mqttClient= nullptr; 
// Add forward declaration for WebServer instance
WebServer server(80);
LocalWebServer* webServer = nullptr;

void initLogging() {
    Serial.begin(115200);
    delay(100);
    // Set log level before any logging happens
    esp_log_level_set("*", ESP_LOG_VERBOSE); // Show all logs initially
    esp_log_level_set("wifi", ESP_LOG_INFO); // Less verbose WiFi logs
    esp_log_level_set("MQTTHelper", ESP_LOG_INFO); // Less verbose MQTT logs
    esp_log_level_set(TAG, ESP_LOG_DEBUG);   // Debug level for main module

    // Test logging
    ESP_LOGE(TAG, "Error level test");
    ESP_LOGW(TAG, "Warning level test");
    ESP_LOGI(TAG, "Info level test");
    ESP_LOGD(TAG, "Debug level test");
    ESP_LOGV(TAG, "Verbose level test");
        
}

bool resolveHostname(const std::string& hostname, IPAddress& resolvedIP) {
    // Wait a bit after WiFi connection before attempting mDNS
    delay(250);
    
    ESP_LOGI(TAG, "Attempting to resolve %s via mDNS...", hostname.c_str());
    // check first, if the hostname is already an IP address using ipaddress.ip_address(host_string)
    IPAddress testIP;
    if (testIP.fromString(hostname.c_str())) {
        resolvedIP = testIP;
        ESP_LOGI(TAG, "Hostname is already an IP address: %s", resolvedIP.toString().c_str());
        return true;
    }

    // Initialize mDNS if not already done
    if (!MDNS.begin("ESP32")) {
        ESP_LOGE(TAG, "Error setting up mDNS responder");
        return false;
    }

    // Try mDNS resolution with multiple attempts
    for (int i = 0; i < 3; i++) {
        resolvedIP = MDNS.queryHost(hostname.c_str(), 5000);
        if (resolvedIP != INADDR_NONE) {
            ESP_LOGI(TAG, "Resolved %s to %s", hostname.c_str(), resolvedIP.toString().c_str());
            return true;
        }
        ESP_LOGW(TAG, "mDNS resolution attempt %d failed, retrying...", i + 1);
        delay(1000);
    }
    ESP_LOGE(TAG, "mDNS resolution failed");
    return false;
}

boolean validateMQTTTReachable(const std::string& hostname, int port) {
    WiFiClient& client = wifiHelper->getWIFIClient();
    if (client.connect(hostname.c_str(), port)) {
        ESP_LOGI(TAG, "Network reachable: %s:%d", hostname.c_str(), port);
        client.stop();
        return true;
    } else {
        ESP_LOGE(TAG, "Network unreachable: %s:%d", hostname.c_str(), port);
        return false;
    }
}

/**
 * setup function
 * This function is called once at the beginning of the program.
 */
void setup() {
    // Initialize serial communication
    initLogging();
    
    bool forceConfig = !ConfigHelper::getInstance().loadConfigFile();
    ESP_LOGI(TAG, "Config file loaded: %s", forceConfig ? "No" : "Yes");
    
    // Initialize WiFi
    wifiHelper = new WifiManagerHelper();
    wifiHelper->setWMUp(forceConfig, const_cast<char*>(""));
    ESP_LOGI(TAG, "Connected to WiFi");

    // Get configuration
    mqtt_url = ConfigHelper::getInstance().getMqttUrl();
    mqtt_topic = ConfigHelper::getInstance().getMqttPath();
    mqtt_client_id = std::string(WiFi.getHostname());

    // Resolve MQTT broker address
    IPAddress resolvedIP;
    if (!mqtt_url.empty()) {
        if (!resolveHostname(mqtt_url, resolvedIP)) {
            ESP_LOGE(TAG, "Failed to resolve hostname: %s", mqtt_url.c_str());
        } else {
            ESP_LOGE(TAG, "Resolved hostname: %s", mqtt_url.c_str());
            mqtt_url = resolvedIP.toString().c_str();
        }
    }

    if (!validateMQTTTReachable(mqtt_url, 1883)) {
        ESP_LOGE(TAG, "Network unreachable for MQTT broker: %s", mqtt_url.c_str());
    } else {
        ESP_LOGI(TAG, "Network reachable for MQTT broker: %s", mqtt_url.c_str());
    }

    ESP_LOGI(TAG, "MQTT Config - URL: %s, Path: %s, Client ID: %s", 
             mqtt_url.c_str(), mqtt_topic.c_str(), mqtt_client_id.c_str());

    // Initialize MQTT client
    mqttClient = new MQTTHelper(mqtt_url, mqtt_client_id, mqtt_topic);
    
    // Initialize other components
    dmdRenderer = new DMDRenderer();
    messageHandler = new MessageHandler(dmdRenderer);
    messageFilter = new MessageFilter();

    // Initialize web server
    webServer = new LocalWebServer();
    webServer->begin();

    ESP_LOGI(TAG, "Setup completed successfully");
}

void loop() {
    static unsigned long lastLog = 0;
    unsigned long now = millis();

    // Only log every second
    if (now - lastLog >= 5000) {
        ESP_LOGD(TAG, "Loop iteration to show this is alive");
        lastLog = now;
    }

    // Handle web server requests
    if (webServer && webServer->isRunning()) {
        webServer->handleClient();
    }

    // Handle MQTT messages
    if (mqttClient && mqttClient->handleConnect()) {
        std::vector<std::string> messages = mqttClient->unStackMessages();
        for (const auto& message : messages) {
            if (messageFilter && messageFilter->isValidMessage(message)) {
                messageHandler->handleMessage(message);
            } else {
                ESP_LOGW(TAG, "Invalid message: %s", message.c_str());
            }
        }
    }

    // Small delay to prevent CPU hogging
    delay(10);
}

void cleanup() {
    delete webServer;
    delete mqttClient;
    delete dmdRenderer;
    delete messageHandler;
    delete messageFilter;
    delete wifiHelper;
    
    webServer = nullptr;
    mqttClient = nullptr;
    dmdRenderer = nullptr;
    messageHandler = nullptr;
    messageFilter = nullptr;
    wifiHelper = nullptr;
}


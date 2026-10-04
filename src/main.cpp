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
#include "util/AttractController.h"
#include "util/MediaLibrary.h"
#include "util/OnlineService.h"
#include "util/Storage.h"
#include "core/Media.h"
#include "util/Settings.h"
#include "util/TimeService.h"
#include "matrix/Hub75_Matrix.h"
static const char* TAG = "Main";  // Add this line for ESP_LOG tag
static unsigned long lastLog = 0;  // Move outside loop() to preserve value
static const int LOG_ALIVE_INTERVAL = 5000; // Maximum number of messages to process at once

std::string  mqtt_url = "raspydarts.local"; // Replace with your MQTT broker address
std::string  mqtt_topic = "raspydarts/#";
std::string  mqtt_client_id = "esp32_client";
WifiManagerHelper *  wifiHelper = nullptr; 
DMDRenderer * dmdRenderer = nullptr; 
MessageHandler* messageHandler = nullptr; 
MessageFilter* messageFilter = nullptr; 
// Define the static member variable

MQTTHelper* mqttClient= nullptr; 
TimeService timeService;
MediaLibrary mediaLibrary;
OnlineService onlineService;
AttractController* attract = nullptr;
LocalWebServer* webServer = nullptr;

void initLogging() {
    Serial.begin(115200);
    delay(100);
    // Set log level before any logging happens
    esp_log_level_set("*", ESP_LOG_INFO); // Show all logs initially
    esp_log_level_set("wifi", ESP_LOG_DEBUG); // Less verbose WiFi Logs
    esp_log_level_set(TAG, ESP_LOG_DEBUG);   // Debug level for main module
    esp_log_level_set("MQTTHelper*", ESP_LOG_INFO);// increase MQTT logs to debug
    esp_log_level_set("MQTTHelper", ESP_LOG_INFO);// increase MQTT logs to debug

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

    // Initialize maxtrix panel
    Hub75_Matrix* matrix = new Hub75_Matrix();
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
    dmdRenderer = new DMDRenderer(matrix);
    dmdRenderer->setBrightnessPercent(ConfigHelper::getInstance().getBrightness());
    dmdRenderer->defaultStyle() = settings::textStyle();
    storageBegin();
    mediaLibrary.begin();
    dmdRenderer->setMediaLibrary(&mediaLibrary);
    dmdRenderer->setCenterImages(settings::centerImages());
    timeService.begin(settings::timezone());
    onlineService.begin();
    attract = new AttractController(dmdRenderer, &mediaLibrary, &onlineService);
    attract->onMessage(millis());  // arms the Running.attract_mode countdown, as RenderFirstStart() did
    messageHandler = new MessageHandler(dmdRenderer, attract, &timeService, &mediaLibrary, &onlineService);
    messageFilter = new MessageFilter();

    // Port of RenderFirstStart(): tell the user where the web interface is.
    TextRequest address;
    address.text = std::string("http://") + WiFi.localIP().toString().c_str();
    address.motion = dmd::Motion::Left;
    dmdRenderer->renderText("Web ok via", 1000);
    dmdRenderer->renderText(address);
    // RenderFirstStart() ended on the Raspy2DMD logo.
    const std::string logo = std::string(dmd::media::kImages) + "/Raspy2DMD.png";
    if (storageExists(logo)) {
        dmdRenderer->renderImage(logo);
    }

    // Initialize web server
    webServer = new LocalWebServer();
    webServer->begin();

    ESP_LOGI(TAG, "Setup completed successfully");
}



void loop() {
    unsigned long now = millis();

    // Only log every second
    if (now - lastLog >= LOG_ALIVE_INTERVAL) {
        ESP_LOGI(TAG, "Loop iteration to show this is alive");
        lastLog = now;
    }

    // Handle web server requests
    if (webServer && webServer->isRunning()) {
        webServer->handleClient();
    }

    // Handle MQTT messages
    static int mqttWasConnected = -1;  // unknown until the first check
    if (mqttClient && mqttClient->handleConnect()) {
        if (mqttWasConnected != 1) {
            ESP_LOGI(TAG, "MQTT client connected");
            mqttWasConnected = 1;
        }
        mqttClient->loop();
        std::vector<std::string> messages = mqttClient->unStackMessages();
        for (const auto& message : messages) {
            if (messageFilter && messageFilter->isValidMessage(message)) {
                attract->onMessage(millis());
                messageHandler->handleMessage(message);
            } else {
                ESP_LOGW(TAG, "Invalid message: %s", message.c_str());
            }
        }
    } else if (mqttWasConnected != 0) {
        // Report the state change once instead of redrawing it on every loop.
        ESP_LOGE(TAG, "MQTT client not connected");
        dmdRenderer->renderStatus("MQTT not connected");
        mqttWasConnected = 0;
    }
    attract->loop(millis());
    timeService.loop(millis(), *dmdRenderer);
    dmdRenderer->update(); // Advance the current animation
    // Yield to the idle task; animations need a fast loop (scroll frames are 10 ms apart).
    delay(1);
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


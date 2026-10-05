#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_log.h>

#include "core/ConfigSchema.h"
#include "core/Media.h"
#include "core/Protocol.h"
#include "matrix/Hub75_Matrix.h"
#include "util/AttractController.h"
#include "util/ConfigHelper.h"
#include "util/DMDRenderer.h"
#include "util/LocalWebServer.h"
#include "util/MQTTHelper.h"
#include "util/MediaLibrary.h"
#include "util/MessageHandler.h"
#include "util/OnlineService.h"
#include "util/Settings.h"
#include "util/Storage.h"
#include "util/TimeService.h"
#include "util/WifiManagerHelper.h"

static const char* TAG = "Main";

static DMDRenderer* dmdRenderer = nullptr;
static MessageHandler* messageHandler = nullptr;
static MQTTHelper* mqttClient = nullptr;
static AttractController* attract = nullptr;
static LocalWebServer* webServer = nullptr;
static TimeService timeService;
static MediaLibrary mediaLibrary;
static OnlineService onlineService;
static dmd::PanelGeometry panel = {PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER};
static std::string mqttBroker;
static int mqttWasConnected = -1;  // unknown until the first check

// One path for every command, whether it came from MQTT or the web page's "Send" box.
static bool dispatchMessage(const std::string& message) {
    if (!dmd::isAcceptedPayload(message)) {
        ESP_LOGW(TAG, "Invalid message: %s", message.c_str());
        return false;
    }
    if (!messageHandler->accepts(message)) {
        ESP_LOGI(TAG, "Standalone mode, ignored: %s", message.c_str());
        return false;
    }
    attract->onMessage(millis());
    messageHandler->handleMessage(message);
    return true;
}

static std::string formatUptime(unsigned long ms) {
    const unsigned long s = ms / 1000;
    char buf[32];
    snprintf(buf, sizeof(buf), "%lud %02luh %02lum", s / 86400, (s / 3600) % 24, (s / 60) % 60);
    return buf;
}

static LocalWebServer::Status boardStatus() {
    const size_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
    return {
        {"Hostname", WiFi.getHostname()},
        {"IP", WiFi.localIP().toString().c_str()},
        {"Wi-Fi", std::to_string(WiFi.RSSI()) + " dBm"},
        {"MQTT", mqttBroker + (mqttWasConnected == 1 ? " (connected)" : " (offline)")},
        {"Topic", ConfigHelper::getInstance().getMqttPath()},
        {"Panel", std::to_string(panel.width()) + " x " + std::to_string(panel.rows)},
        {"Uptime", formatUptime(millis())},
        {"Free heap", std::to_string(ESP.getFreeHeap() / 1024) + " KB"},
        {"Storage", std::to_string(used / 1024) + " / " + std::to_string(total / 1024) + " KB"},
    };
}

// PubSubClient cannot resolve "raspydarts.local" by itself: ask mDNS for .local names.
static std::string resolveBroker(const std::string& host) {
    IPAddress ip;
    if (host.empty() || ip.fromString(host.c_str())) {
        return host;
    }
    for (int attempt = 0; attempt < 3; ++attempt) {
        const std::string name = host.size() > 6 && host.compare(host.size() - 6, 6, ".local") == 0
                                     ? host.substr(0, host.size() - 6)
                                     : host;
        ip = MDNS.queryHost(name.c_str(), 2000);
        if (ip != INADDR_NONE) {
            ESP_LOGI(TAG, "%s is %s", host.c_str(), ip.toString().c_str());
            return ip.toString().c_str();
        }
    }
    ESP_LOGW(TAG, "Cannot resolve %s with mDNS, trying DNS", host.c_str());
    return host;
}

void setup() {
    Serial.begin(115200);
    ConfigHelper& config = ConfigHelper::getInstance();
    const bool configured = config.loadConfigFile();

    // Panel geometry from DMDRenderer.cols / rows / led_chain (applied at start-up, like the Pi).
    panel = dmd::panelGeometry(config.getSetting("DMDRenderer", "cols", ""),
                               config.getSetting("DMDRenderer", "rows", ""),
                               config.getSetting("DMDRenderer", "led_chain", ""), panel);
    dmdRenderer = new DMDRenderer(new Hub75_Matrix(panel.cols, panel.rows, panel.chain));
    dmdRenderer->setBrightnessPercent(config.getBrightness());
    dmdRenderer->defaultStyle() = settings::textStyle();

    WifiManagerHelper().connect(!configured, [](const std::string& ssid, const std::string& password) {
        dmdRenderer->renderText("WiFi " + ssid + " " + password);
        dmdRenderer->update();
    });

    const std::string hostname = config.getHostname().empty() ? "esp32-dmd" : config.getHostname();
    if (MDNS.begin(hostname.c_str())) {
        MDNS.addService("http", "tcp", 80);
    }
    mqttBroker = resolveBroker(config.getMqttUrl());
    mqttClient = new MQTTHelper(mqttBroker, WiFi.getHostname(), config.getMqttPath());

    storageBegin();
    mediaLibrary.begin();
    dmdRenderer->setMediaLibrary(&mediaLibrary);
    dmdRenderer->setCenterImages(settings::centerImages());
    timeService.begin(settings::timezone());
    onlineService.begin();
    attract = new AttractController(dmdRenderer, &mediaLibrary, &onlineService);
    attract->onMessage(millis());  // arms the Running.attract_mode countdown, as RenderFirstStart() did
    messageHandler = new MessageHandler(dmdRenderer, attract, &timeService, &mediaLibrary, &onlineService);
    // receipconf answers on the broker we are connected to (Raspydarts').
    messageHandler->setPublisher([](const std::string& topic, const std::string& payload) {
        return mqttClient->publish(topic, payload);
    });

    // Port of RenderFirstStart(): tell the user where the web interface is (Running.default).
    if (settings::showWebAddress()) {
        TextRequest address;
        address.text = std::string("http://") + WiFi.localIP().toString().c_str();
        address.motion = dmd::Motion::Left;
        dmdRenderer->renderText("Web ok via", 1000);
        dmdRenderer->renderText(address);
    }
    // RenderFirstStart() ended on the Raspy2DMD logo.
    const std::string logo = std::string(dmd::media::kImages) + "/Raspy2DMD.png";
    if (storageExists(logo)) {
        dmdRenderer->renderImage(logo);
    }
    // Port of RenderStandalone(): standalone mode starts the attract mode right away.
    if (settings::standalone()) {
        attract->start(settings::scrollOrder());
    }

    webServer = new LocalWebServer();
    webServer->setHostname(hostname);
    // The settings page re-applies what rldconf re-applies.
    webServer->setOnSettingsSaved([]() { messageHandler->reloadSettings(); });
    webServer->setOnCommand(dispatchMessage);
    webServer->setStatusProvider(boardStatus);
    webServer->begin();
}

void loop() {
    webServer->handleClient();

    if (mqttClient->loop(millis())) {
        if (mqttWasConnected != 1) {
            ESP_LOGI(TAG, "MQTT connected");
            mqttWasConnected = 1;
        }
        for (const std::string& message : mqttClient->takeMessages()) {
            dispatchMessage(message);
        }
    } else if (mqttWasConnected != 0) {
        // Report the state change once instead of redrawing it on every loop.
        dmdRenderer->renderStatus("MQTT not connected");
        mqttWasConnected = 0;
    }
    attract->loop(millis());
    timeService.loop(millis(), *dmdRenderer);
    dmdRenderer->update();
    // Yield to the idle task; animations need a fast loop (scroll frames are 10 ms apart).
    delay(1);
}

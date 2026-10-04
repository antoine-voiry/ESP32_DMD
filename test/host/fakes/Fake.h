#ifndef FAKE_CONTROL_H
#define FAKE_CONTROL_H

// What the host tests steer and inspect: clock, Wi-Fi, files, network replies, MQTT broker,
// setup portal, web requests and the LED panel. fake::resetAll() restores the defaults.

#include <cstdint>
#include <ctime>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

class MatrixPanel_I2S_DMA;

namespace fake {

// Clock: millis() and delay(); time() returns `epoch` (UTC).
void setMillis(unsigned long ms);
void advance(unsigned long ms);
extern time_t epoch;
extern std::string timezone;  // last configTzTime() TZ

extern int restarts;
extern int deepSleeps;
void seedRandom(uint32_t seed);

struct WiFiState {
    bool connected = true;
    std::string ip = "192.168.1.42";
    std::string hostname = "dmd";
    int rssi = -58;
    int reconnects = 0;
    std::function<void(int)> eventHandler;
};
extern WiFiState wifi;

// Files live under a fresh temporary directory after each fsReset().
void fsReset();
extern bool fsMountFails;
void writeFile(const std::string& path, const std::string& content);
std::string readFile(const std::string& path);
bool fileExists(const std::string& path);

struct HttpReply {
    int code = 200;
    std::string body;
    bool chunked = false;
};
extern std::map<std::string, HttpReply> httpReplies;  // by URL prefix; no match = connection failure
extern std::vector<std::string> httpRequests;

struct MqttBroker {
    bool acceptConnections = true;
    bool connected = false;
    int connectAttempts = 0;
    std::string host;
    uint16_t port = 0;
    std::string clientId;
    uint16_t bufferSize = 0;
    std::vector<std::string> subscriptions;
    std::vector<std::pair<std::string, std::string>> published;
    std::vector<std::pair<std::string, std::string>> incoming;  // delivered by PubSubClient::loop()
};
extern MqttBroker mqtt;

extern std::map<std::string, std::string> mdnsHosts;  // name without .local -> IP
extern std::string mdnsName;
extern std::vector<std::string> mdnsServices;

struct Portal {
    bool savedNetworkWorks = true;  // autoConnect() result
    bool userConnects = true;       // startConfigPortal() result
    bool userSaves = true;          // the save callback fires
    std::map<std::string, std::string> entered;
    int opened = 0;
    std::string ssid;
    std::string password;
    std::string hostname;
    bool portalEnabledAtAutoConnect = true;
    unsigned long timeoutSec = 0;
};
extern Portal portal;

// Runs every task created with xTaskCreatePinnedToCore() until it waits for a notification.
void runTasks();

struct Upload {
    std::string filename;
    std::string content;
    bool abort = false;
};
struct WebRequest {
    bool post = false;
    std::string uri = "/";
    std::vector<std::pair<std::string, std::string>> args;
    std::string host = "192.168.1.42";
    std::string origin;
    std::vector<Upload> uploads;
};
struct WebResponse {
    int code = 0;
    std::string type;
    std::string body;
    std::map<std::string, std::string> headers;
    int sends = 0;
};
// Dispatches to the most recently created WebServer.
WebResponse request(const WebRequest& r);
int webHandleClientCalls();

MatrixPanel_I2S_DMA* panel();  // most recently created
extern bool panelBeginFails;

void resetAll();

}  // namespace fake

#endif

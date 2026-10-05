#ifndef LOCALWEBSERVER_H
#define LOCALWEBSERVER_H

// The board's web pages: dashboard (/), Raspy2DMD settings (/settings), media files (/files) and
// the portal values (/config). Requests from other sites are refused (see core/WebGuard.h).

#include <WebServer.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "Storage.h"

class LocalWebServer {
public:
    using Status = std::vector<std::pair<std::string, std::string>>;

    LocalWebServer();
    void begin();
    // Serves pending requests while Wi-Fi is connected.
    void handleClient();

    // The name the board answers to besides its IP address (also with a domain: hostname.local...).
    void setHostname(const std::string& hostname) { _hostname = hostname; }
    // Called after /settings saved values that apply without a restart.
    void setOnSettingsSaved(std::function<void()> callback) { _onSettingsSaved = std::move(callback); }
    // "Send to the panel": handled like an MQTT payload, returns false if rejected.
    void setOnCommand(std::function<bool(const std::string&)> callback) { _onCommand = std::move(callback); }
    // Key/value lines for the dashboard's status card.
    void setStatusProvider(std::function<Status()> provider) { _statusProvider = std::move(provider); }

private:
    // Registers a route behind the Host / Origin checks.
    void route(const char* path, HTTPMethod method, void (LocalWebServer::*handler)());
    // 0 when the request may proceed, else the HTTP status to refuse it with (403, 429).
    int refusal(bool post);
    bool requestAllowed(bool post);
    bool checkRateLimit();
    void redirect(const char* location);
    String page(const char* title, const char* active, const String& body) const;

    void handleRoot();
    void handleSend();
    void handleConfig();
    void handleSaveConfig();
    void handleFiles();
    void handleUpload();
    void handleUploadDone();
    void handleDelete();
    void handleSettings();
    void handleSaveSettings();

    static constexpr unsigned long kRateWindowMs = 60000;
    static constexpr int kMaxRequestsPerWindow = 30;

    WebServer _server;
    std::string _hostname;
    unsigned long _requestTimes[kMaxRequestsPerWindow] = {};
    int _requestIndex = 0;
    File _uploadFile;
    std::string _uploadTarget;
    bool _uploadFailed = false;
    int _uploadRefusal = 0;
    std::function<void()> _onSettingsSaved;
    std::function<bool(const std::string&)> _onCommand;
    std::function<Status()> _statusProvider;
};

#endif

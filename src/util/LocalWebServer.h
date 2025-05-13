#ifndef LOCALWEBSERVER_H
#define LOCALWEBSERVER_H

#include <WebServer.h>
#include <SPIFFS.h>  // Change to SPIFFS
#include <ArduinoJson.h>
#include <esp_log.h>
#include <esp_random.h>  // For secure token generation

class LocalWebServer {
private:
    // Server instance
    WebServer server;
    
    // Configuration constants
    static constexpr const char* CONFIG_FILE = "/config.json";
    static constexpr int JSON_CAPACITY = 1024;
    static constexpr uint16_t SERVER_PORT = 80;
    
    // Security settings
    static constexpr const char* SECURITY_HEADER = "X-Security-Token";
    static constexpr unsigned long TOKEN_VALIDITY = 3600000;  // 1 hour in ms
    String securityToken;
    unsigned long tokenTimestamp;
    
    // Rate limiting
    static constexpr unsigned long RATE_LIMIT_WINDOW = 60000;  // 1 minute
    static constexpr int MAX_REQUESTS = 30;  // Maximum requests per window
    unsigned long requestCounts[30];  // Circular buffer for request timestamps
    int requestIndex;
    
    // Handler methods
    void handleRoot();
    void handleConfig();
    void handleGetConfigJson();
    void handleSaveConfig();
    void handleNotFound();
    
    // Utility methods
    String generateConfigForm(const String& jsonString);
    bool saveConfigToFile(DynamicJsonDocument& doc);
    String readConfigFile();
    bool checkSecurityToken(void);
    void generateSecurityToken(void);
    bool checkRateLimit(void);
    void logAccess(const String& path, int responseCode);

public:
    LocalWebServer();
    void begin();
    void handleClient();
    bool isRunning() const;
};

#endif
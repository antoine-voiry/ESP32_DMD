#include "LocalWebServer.h"
#include <WiFi.h>      // ESP32 WiFi Library
#include <FS.h>        // Include for File class
#include <sstream>
#include <iomanip>
static const char* TAG = "LocalWebServer";

LocalWebServer::LocalWebServer() : server(SERVER_PORT) {
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {  // true = format on failure
        ESP_LOGE(TAG, "SPIFFS Mount Failed");
        return;
    }
    ESP_LOGI(TAG, "SPIFFS mounted successfully");

    // Initialize server routes
    server.on("/", std::bind(&LocalWebServer::handleRoot, this));
    server.on("/config", std::bind(&LocalWebServer::handleConfig, this));
    server.on("/get_config_json", std::bind(&LocalWebServer::handleGetConfigJson, this));
    server.on("/save_config", HTTP_POST, std::bind(&LocalWebServer::handleSaveConfig, this));
    server.onNotFound(std::bind(&LocalWebServer::handleNotFound, this));

}

void LocalWebServer::begin() {
    server.begin();
    ESP_LOGI(TAG, "HTTP server started on port %d", SERVER_PORT);
}

void LocalWebServer::handleClient() {
    server.handleClient();
}

bool LocalWebServer::isRunning() const {
    return WiFi.status() == WL_CONNECTED;
}

void LocalWebServer::handleRoot() {
    String html = F("<!DOCTYPE html><html><head><title>Configuration</title></head><body>"
                   "<h1>DMD Device Configuration</h1>"
                   "<p><a href='/config'>View/Edit Configuration</a></p>"
                   "</body></html>");
    server.send(200, "text/html", html);
    ESP_LOGD(TAG, "Root page served");
}

void LocalWebServer::handleConfig() {
    String jsonString = readConfigFile();
    if (jsonString.isEmpty()) {
        return;
    }
    
    String html = generateConfigForm(jsonString);
    server.send(200, "text/html", html);
    ESP_LOGD(TAG, "Config page served");
}

String LocalWebServer::generateConfigForm(const String& jsonString) {
    String html = F("<!DOCTYPE html><html><head><title>Configuration</title></head><body>"
                   "<h1>Current Configuration</h1>");
    html += "<pre>" + jsonString + "</pre><hr><h2>Edit Configuration</h2>";
    html += F("<form action='/save_config' method='post'>");

    DynamicJsonDocument doc(JSON_CAPACITY);
    deserializeJson(doc, jsonString);

    for (JsonPair pair : doc.as<JsonObject>()) {
        html += "<label>" + String(pair.key().c_str()) + ":</label>";
        html += "<input type='text' name='" + String(pair.key().c_str()) + 
                "' value='" + pair.value().as<String>() + "'><br><br>";
    }

    html += F("<input type='submit' value='Save Configuration'></form></body></html>");
    return html;
}

void LocalWebServer::handleGetConfigJson() {
    String jsonString = readConfigFile();
    if (!jsonString.isEmpty()) {
        server.send(200, "application/json", jsonString);
        ESP_LOGD(TAG, "JSON config served: %s", jsonString.c_str());
    }
}

String LocalWebServer::readConfigFile() {
    File configFile = SPIFFS.open(CONFIG_FILE, "r");  // Change to SPIFFS
    if (!configFile) {
        server.send(500, "text/plain", "Failed to open config file");
        ESP_LOGE(TAG, "Failed to open %s for reading", CONFIG_FILE);
        return String();
    }
    
    String jsonString = configFile.readString();
    configFile.close();
    return jsonString;
}

// Handling not found pages:
void LocalWebServer::handleNotFound() {
    String message = "File Not Found\n\n";
    message += "URI: " + server.uri();
    ESP_LOGW(TAG, "404 Not Found: %s", server.uri().c_str());
    server.send(404, "text/plain", message);
}

void LocalWebServer::handleSaveConfig() {
    if (!checkSecurityToken() || !checkRateLimit()) {
        server.send(403, "text/plain", "Access denied");
        return;
    }

    if (server.args() == 0) {
        server.send(400, "text/plain", "No arguments received");
        ESP_LOGW(TAG, "No arguments received in save config POST request");
        return;
    }

    DynamicJsonDocument doc(JSON_CAPACITY);
    for (uint8_t i = 0; i < server.args(); i++) {
        doc[server.argName(i)] = server.arg(i);
        ESP_LOGD(TAG, "POST param '%s' = '%s'", 
                 server.argName(i).c_str(), server.arg(i).c_str());
    }

    if (saveConfigToFile(doc)) {
        server.send(200, "text/plain", "Configuration saved successfully!");
        ESP_LOGI(TAG, "Configuration saved to %s", CONFIG_FILE);
    }
}

bool LocalWebServer::saveConfigToFile(DynamicJsonDocument& doc) {
    File configFile = SPIFFS.open(CONFIG_FILE, "w");  // Change to SPIFFS
    if (!configFile) {
        server.send(500, "text/plain", "Failed to open config file for writing");
        ESP_LOGE(TAG, "Failed to open %s for writing", CONFIG_FILE);
        return false;
    }
    
    serializeJsonPretty(doc, configFile);
    configFile.close();
    return true;
}

void LocalWebServer::generateSecurityToken() {
    uint32_t random = esp_random();  // Get hardware random number
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << random;
    securityToken = ss.str().c_str();
    tokenTimestamp = millis();
    ESP_LOGI(TAG, "New security token generated");
}

bool LocalWebServer::checkSecurityToken() {
    // if (!server.hasHeader(SECURITY_HEADER)) {
    //     ESP_LOGW(TAG, "Missing security token");
    //     return false;
    // }
    
    // String token = server.header(SECURITY_HEADER);
    // if (token != securityToken || (millis() - tokenTimestamp) > TOKEN_VALIDITY) {
    //     ESP_LOGW(TAG, "Invalid or expired security token");
    //     return false;
    // }
    
    return true;
}

bool LocalWebServer::checkRateLimit() {
    unsigned long now = millis();
    int count = 0;
    
    // Count requests in current window
    for (int i = 0; i < MAX_REQUESTS; i++) {
        if (now - requestCounts[i] < RATE_LIMIT_WINDOW) {
            count++;
        }
    }
    
    if (count >= MAX_REQUESTS) {
        ESP_LOGW(TAG, "Rate limit exceeded");
        return false;
    }
    
    // Store new request timestamp
    requestCounts[requestIndex] = now;
    requestIndex = (requestIndex + 1) % MAX_REQUESTS;
    return true;
}
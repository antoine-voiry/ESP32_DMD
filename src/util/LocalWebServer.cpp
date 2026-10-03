#include "LocalWebServer.h"
#include <WiFi.h>      // ESP32 WiFi Library
#include <FS.h>        // Include for File class
#include <sstream>
#include <cstring>
#include <iomanip>
#include "ConfigHelper.h"
#include "core/ConfigSchema.h"
#include "core/Media.h"
#include "core/TextUtil.h"
static const char* TAG = "LocalWebServer";

LocalWebServer::LocalWebServer() : server(SERVER_PORT), tokenTimestamp(0), requestIndex(0) {
    // 0 = free slot (the array used to start uninitialised, rate limiting at random).
    for (auto& t : requestCounts) {
        t = 0;
    }
    if (!storageBegin()) {
        ESP_LOGE(TAG, "Filesystem not mounted");
        return;
    }

    // Initialize server routes
    server.on("/", std::bind(&LocalWebServer::handleRoot, this));
    server.on("/config", std::bind(&LocalWebServer::handleConfig, this));
    server.on("/get_config_json", std::bind(&LocalWebServer::handleGetConfigJson, this));
    server.on("/save_config", HTTP_POST, std::bind(&LocalWebServer::handleSaveConfig, this));
    server.on("/files", HTTP_GET, std::bind(&LocalWebServer::handleFiles, this));
    server.on("/upload", HTTP_POST, std::bind(&LocalWebServer::handleUploadDone, this),
              std::bind(&LocalWebServer::handleUpload, this));
    server.on("/delete", HTTP_POST, std::bind(&LocalWebServer::handleDelete, this));
    server.on("/settings", HTTP_GET, std::bind(&LocalWebServer::handleSettings, this));
    server.on("/settings", HTTP_POST, std::bind(&LocalWebServer::handleSaveSettings, this));
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
                   "<p><a href='/settings'>Raspy2DMD settings</a></p>"
                   "<p><a href='/files'>Media files (GIFs, images, carousel texts, effects)</a></p>"
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
        // Nested objects (the Raspy2DMD "settings" set over MQTT) are not editable here.
        if (pair.value().is<JsonObject>() || pair.value().is<JsonArray>()) {
            continue;
        }
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
    File configFile = storage().open(CONFIG_FILE, "r");
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

    // Merge into the existing file so keys missing from the form (e.g. "settings") survive.
    DynamicJsonDocument doc(JSON_CAPACITY);
    File existing = storage().open(CONFIG_FILE, "r");
    if (existing) {
        if (deserializeJson(doc, existing)) {
            doc.clear();
        }
        existing.close();
    }
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
    File configFile = storage().open(CONFIG_FILE, "w");
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
        if (requestCounts[i] != 0 && now - requestCounts[i] < RATE_LIMIT_WINDOW) {
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
////////////////////////////////////////////////////////////////////////////////
// Media file manager

void LocalWebServer::handleFiles() {
    String html = F("<!DOCTYPE html><html><head><meta charset='utf-8'><title>Media files</title>"
                    "<style>body{font-family:sans-serif}td{padding:2px 8px}</style></head><body>"
                    "<h1>Media files</h1>"
                    "<p>Folders: /gifs, /images, /scores/&lt;dart&gt;, /specialsmoves/&lt;MOVE&gt;, /patterns, "
                    "/textes; files /effets.txt and /exclusions.txt at the root.</p>"
                    "<form id='up' method='post' enctype='multipart/form-data'>"
                    "Folder <input id='dir' value='/gifs' size='24'> "
                    "<input type='file' name='file' multiple> <input type='submit' value='Upload'></form>"
                    "<script>document.getElementById('up').onsubmit=function(){"
                    "this.action='/upload?dir='+encodeURIComponent(document.getElementById('dir').value);};"
                    "</script><table>");
    char line[64];
    for (const std::string& path : storageListFiles("/", true)) {
        File f = storage().open(path.c_str(), "r");
        const unsigned size = f ? static_cast<unsigned>(f.size()) : 0;
        const std::string escaped = dmd::htmlEscape(path);
        html += "<tr><td>";
        html += escaped.c_str();
        snprintf(line, sizeof(line), "</td><td>%u B</td><td>", size);
        html += line;
        if (path != CONFIG_FILE) {
            html += "<form method='post' action='/delete'><input type='hidden' name='path' value='";
            html += escaped.c_str();
            html += "'><input type='submit' value='Delete'></form>";
        }
        html += "</td></tr>";
    }
    snprintf(line, sizeof(line), "</table><p>%u / %u KB used</p>",
             static_cast<unsigned>(LittleFS.usedBytes() / 1024), static_cast<unsigned>(LittleFS.totalBytes() / 1024));
    html += line;
    html += F("<p><a href='/'>Back</a></p></body></html>");
    server.send(200, "text/html", html);
}

void LocalWebServer::handleUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        uploadFailed = false;
        const std::string target = dmd::uploadPath(server.arg("dir").c_str(), upload.filename.c_str());
        if (target.empty() || target == CONFIG_FILE) {
            ESP_LOGW(TAG, "Upload refused: '%s' into '%s'", upload.filename.c_str(), server.arg("dir").c_str());
            uploadFailed = true;
            return;
        }
        storageMakeParents(target);
        uploadTarget = target.c_str();
        uploadFile = storage().open(target.c_str(), "w");
        uploadFailed = !uploadFile;
        ESP_LOGI(TAG, "Upload started: %s", target.c_str());
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!uploadFailed && uploadFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
            ESP_LOGE(TAG, "Upload write failed (filesystem full?)");
            uploadFailed = true;
        }
    } else if (upload.status == UPLOAD_FILE_END || upload.status == UPLOAD_FILE_ABORTED) {
        if (uploadFile) {
            uploadFile.close();
        }
        if ((uploadFailed || upload.status == UPLOAD_FILE_ABORTED) && uploadTarget.length()) {
            storage().remove(uploadTarget.c_str());  // never leave a truncated file behind
            uploadFailed = true;
        }
        ESP_LOGI(TAG, "Upload %s: %s (%u bytes)", uploadFailed ? "failed" : "done", uploadTarget.c_str(),
                 static_cast<unsigned>(upload.totalSize));
        uploadTarget = "";
    }
}

void LocalWebServer::handleUploadDone() {
    if (uploadFailed) {
        server.send(400, "text/plain", "Upload failed (bad name or folder, or filesystem full)");
        return;
    }
    server.sendHeader("Location", "/files");
    server.send(303);
}

void LocalWebServer::handleDelete() {
    if (!checkRateLimit()) {
        server.send(429, "text/plain", "Too many requests");
        return;
    }
    const String path = server.arg("path");
    if (path.length() == 0 || path == CONFIG_FILE || path.indexOf("..") >= 0 || !storage().exists(path)) {
        server.send(400, "text/plain", "Cannot delete this file");
        return;
    }
    storage().remove(path);
    ESP_LOGI(TAG, "Deleted %s", path.c_str());
    server.sendHeader("Location", "/files");
    server.send(303);
}

////////////////////////////////////////////////////////////////////////////////
// Raspy2DMD settings

void LocalWebServer::handleSettings() {
    const ConfigHelper& config = ConfigHelper::getInstance();
    String html = F("<!DOCTYPE html><html><head><meta charset='utf-8'><title>Raspy2DMD settings</title>"
                    "<style>body{font-family:sans-serif}label{display:inline-block;width:16em}"
                    ".off{color:#888}fieldset{margin-bottom:1em}</style></head><body>"
                    "<h1>Raspy2DMD settings</h1>"
                    "<p>Same keys as Raspy2DMD.cfg (and conf|Section|key:value). Grey keys are kept for "
                    "Raspydarts but have no effect on the ESP32. Panel size and standalone mode restart the board.</p>"
                    "<form method='post' action='/settings'>");
    const char* section = "";
    for (const dmd::SettingDef& def : dmd::configSchema()) {
        if (strcmp(section, def.section) != 0) {
            if (*section) html += "</fieldset>";
            section = def.section;
            html += "<fieldset><legend>";
            html += def.section;
            html += "</legend>";
        }
        const std::string name = std::string(def.section) + "." + def.key;
        const std::string value = config.getSetting(def.section, def.key, def.def);
        html += def.usedOnEsp32 ? "<label>" : "<label class='off'>";
        html += def.key;
        html += "</label><input size='40' name='";
        html += dmd::htmlEscape(name).c_str();
        html += "' value='";
        html += dmd::htmlEscape(value).c_str();
        html += "'><br>";
    }
    html += F("</fieldset><input type='submit' value='Save'></form><p><a href='/'>Back</a></p></body></html>");
    server.send(200, "text/html", html);
}

void LocalWebServer::handleSaveSettings() {
    if (!checkRateLimit()) {
        server.send(429, "text/plain", "Too many requests");
        return;
    }
    ConfigHelper& config = ConfigHelper::getInstance();
    bool restart = false;
    int changed = 0;
    for (int i = 0; i < server.args(); i++) {
        const std::string name = server.argName(i).c_str();
        const size_t dot = name.find('.');
        if (dot == std::string::npos) continue;
        const std::string section = name.substr(0, dot);
        const std::string key = name.substr(dot + 1);
        const dmd::SettingDef* def = dmd::findSetting(section, key);
        if (!def) continue;  // only known Raspy2DMD keys
        const std::string value = server.arg(i).c_str();
        if (config.getSetting(section, key, def->def) == value) continue;
        config.setSetting(section, key, value);
        restart = restart || dmd::needsRestart(section, key);
        ++changed;
    }
    if (changed > 0) {
        config.saveConfigFile();
        ESP_LOGI(TAG, "%d setting(s) saved from the web page", changed);
    }
    if (restart) {
        server.send(200, "text/plain", "Saved, restarting to apply the panel / standalone settings...");
        delay(500);
        ESP.restart();
        return;
    }
    if (changed > 0 && onSettingsSaved) {
        onSettingsSaved();
    }
    server.sendHeader("Location", "/settings");
    server.send(303);
}

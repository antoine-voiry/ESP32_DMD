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

namespace {

const char kStyle[] PROGMEM =
    ":root{--bg:#0d0e12;--card:#161821;--line:#252838;--fg:#e8eaf0;--mut:#8b91a5;--acc:#ff5a36;--acc2:#3db1ff}"
    "*{box-sizing:border-box}body{margin:0;font:15px/1.5 system-ui,-apple-system,sans-serif;background:var(--bg);"
    "color:var(--fg)}a{color:var(--acc2)}header{display:flex;flex-wrap:wrap;gap:6px 20px;align-items:center;"
    "padding:12px 20px;background:#090a0d;border-bottom:1px solid var(--line)}header .logo{font-weight:700;"
    "letter-spacing:.5px}header .logo span{color:var(--acc)}nav a{color:var(--mut);text-decoration:none;"
    "margin-right:16px}nav a.on,nav a:hover{color:var(--fg)}main{max-width:900px;margin:auto;padding:20px 16px}"
    "h1{font-size:22px;margin:4px 0 16px}h2{font-size:16px;margin:0 0 10px;color:var(--mut);font-weight:600}"
    ".card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:16px 18px;"
    "margin-bottom:16px}input,select{background:var(--bg);color:var(--fg);border:1px solid var(--line);"
    "border-radius:8px;padding:7px 9px;font:inherit;max-width:100%}button,input[type=submit]{background:var(--acc);"
    "color:#fff;border:0;border-radius:8px;padding:8px 14px;font:inherit;cursor:pointer}"
    "input::file-selector-button{background:#252a3d;color:var(--fg);border:0;border-radius:6px;"
    "padding:4px 10px;margin-right:8px;font:inherit;cursor:pointer}button.chip{background:#252a3d;color:var(--fg);margin:3px 4px 3px 0;padding:6px 11px}"
    "button.chip:hover{background:#30364f}button.del{background:transparent;color:var(--mut);padding:2px 6px}"
    "button.del:hover{color:var(--acc)}.kv div{display:flex;justify-content:space-between;gap:12px;"
    "border-bottom:1px solid var(--line);padding:5px 0}.kv span{color:var(--mut)}table{width:100%;"
    "border-collapse:collapse}td{padding:6px 4px;border-bottom:1px solid var(--line);word-break:break-all}"
    "td.n{text-align:right;color:var(--mut);white-space:nowrap}.muted{color:var(--mut)}.row{display:flex;"
    "gap:8px;flex-wrap:wrap;align-items:center}.row input[type=text]{flex:1;min-width:200px}"
    "fieldset{border:1px solid var(--line);border-radius:10px;margin:0 0 14px;padding:10px 14px}"
    "legend{color:var(--acc);padding:0 6px}.f{display:flex;flex-wrap:wrap;align-items:center;gap:4px 12px;"
    "padding:3px 0}"
    ".f label{flex:0 0 15em}.f input{flex:1;min-width:180px}.off label{color:var(--mut)}"
    ".bar{height:8px;background:var(--bg);border-radius:4px;overflow:hidden}.bar i{display:block;height:100%;"
    "background:linear-gradient(90deg,var(--acc2),var(--acc))}pre{white-space:pre-wrap;color:var(--mut)}";

// One-click examples for the home page.
const char* const kPresets[][2] = {
    {"msg|Bienvenue|3", "Message"},
    {"msgmove|Bienvenue au club|left", "Scrolling text"},
    {"msgfx|DMD|rainbow|4", "Rainbow text"},
    {"score|T20 - T20 - T20", "180!"},
    {"fx|fireworks|6", "Fireworks"},
    {"fx|plasma|6", "Plasma"},
    {"fx|matrix|6", "Matrix"},
    {"time|start", "Clock"},
    {"meteo|", "Weather"},
    {"edfJoursTempo|", "EDF Tempo"},
    {"perf|", "Board status"},
    {"waiter|start", "Attract mode"},
    {"waiter|stop", "Stop attract"},
};

}  // namespace

String LocalWebServer::page(const char* title, const char* active, const String& body) const {
    static const char* const kNav[][2] = {
        {"/", "Home"}, {"/settings", "Settings"}, {"/files", "Media"}, {"/config", "Wi-Fi &amp; MQTT"}};
    String html;
    html.reserve(body.length() + 3600);
    html += F("<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'>"
              "<meta name='viewport' content='width=device-width,initial-scale=1'><title>");
    html += title;
    html += F(" · ESP32 DMD</title><style>");
    html += FPSTR(kStyle);
    html += F("</style></head><body><header><div class='logo'>ESP32 <span>DMD</span></div><nav>");
    for (const auto& item : kNav) {
        html += "<a href='";
        html += item[0];
        html += strcmp(item[0], active) == 0 ? "' class='on'>" : "'>";
        html += item[1];
        html += "</a>";
    }
    html += F("</nav></header><main>");
    html += body;
    html += F("</main></body></html>");
    return html;
}

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
    server.on("/send", HTTP_POST, std::bind(&LocalWebServer::handleSend, this));
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
    String body = F("<h1>Panel</h1>");
    if (server.hasArg("sent")) {
        body += server.arg("sent") == "1" ? F("<div class='card'>Sent to the panel.</div>")
                                         : F("<div class='card'>Rejected: unknown command, bad score, or "
                                             "not allowed in standalone mode.</div>");
    }
    body += F("<div class='card'><h2>Send to the panel</h2>"
              "<form method='post' action='/send'><div class='row'>"
              "<input type='text' name='cmd' placeholder='msg|Hello|3' autofocus>"
              "<input type='submit' value='Send'></div></form><div style='margin-top:10px'>"
              "<form method='post' action='/send'>");
    for (const auto& preset : kPresets) {
        body += "<button class='chip' name='cmd' value='";
        body += dmd::htmlEscape(preset[0]).c_str();
        body += "'>";
        body += preset[1];
        body += "</button>";
    }
    body += F("</form></div><p class='muted'>Same messages as Raspydarts sends over MQTT; see "
              "docs/PORTING.md for the full list.</p></div>");
    if (statusProvider) {
        body += F("<div class='card'><h2>Status</h2><div class='kv'>");
        for (const auto& kv : statusProvider()) {
            body += "<div><span>";
            body += dmd::htmlEscape(kv.first).c_str();
            body += "</span><b>";
            body += dmd::htmlEscape(kv.second).c_str();
            body += "</b></div>";
        }
        body += F("</div></div>");
    }
    server.send(200, "text/html", page("Panel", "/", body));
}

void LocalWebServer::handleSend() {
    if (!checkRateLimit()) {
        server.send(429, "text/plain", "Too many requests");
        return;
    }
    const std::string cmd = server.arg("cmd").c_str();
    const bool ok = !cmd.empty() && onCommand && onCommand(cmd);
    server.sendHeader("Location", ok ? "/?sent=1" : "/?sent=0");
    server.send(303);
}

void LocalWebServer::handleConfig() {
    String jsonString = readConfigFile();
    if (jsonString.isEmpty()) {
        return;
    }
    
    server.send(200, "text/html", page("Wi-Fi & MQTT", "/config", generateConfigForm(jsonString)));
    ESP_LOGD(TAG, "Config page served");
}

String LocalWebServer::generateConfigForm(const String& jsonString) {
    // Values can come from MQTT (conf): escape everything that goes into the page.
    String html = F("<h1>Wi-Fi &amp; MQTT</h1><div class='card'><h2>Connection</h2>"
                    "<form action='/save_config' method='post'>");

    DynamicJsonDocument doc(JSON_CAPACITY);
    deserializeJson(doc, jsonString);

    for (JsonPair pair : doc.as<JsonObject>()) {
        // Nested objects (the Raspy2DMD "settings" set over MQTT) are not editable here.
        if (pair.value().is<JsonObject>() || pair.value().is<JsonArray>()) {
            continue;
        }
        const std::string key = dmd::htmlEscape(pair.key().c_str());
        const std::string value = dmd::htmlEscape(pair.value().as<String>().c_str());
        html += "<div class='f'><label>";
        html += key.c_str();
        html += "</label><input type='text' name='";
        html += key.c_str();
        html += "' value='";
        html += value.c_str();
        html += "'></div>";
    }

    html += F("<p><input type='submit' value='Save'></p></form></div><div class='card'><h2>config.json</h2><pre>");
    html += dmd::htmlEscape(jsonString.c_str()).c_str();
    html += F("</pre></div>");
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
    const unsigned usedKb = static_cast<unsigned>(LittleFS.usedBytes() / 1024);
    const unsigned totalKb = static_cast<unsigned>(LittleFS.totalBytes() / 1024);
    char line[160];
    String html = F("<h1>Media</h1><div class='card'><h2>Upload</h2>"
                    "<form id='up' method='post' enctype='multipart/form-data'><div class='row'>"
                    "<input id='dir' list='dirs' value='/gifs' size='22'><datalist id='dirs'>"
                    "<option value='/gifs'><option value='/images'><option value='/scores/T20'>"
                    "<option value='/specialsmoves/MAXIMUM_TON_80'><option value='/patterns'><option value='/textes'>"
                    "<option value='/meteo'><option value='/edfjourstempo'><option value='/'></datalist>"
                    "<input type='file' name='file' multiple><input type='submit' value='Upload'></div></form>"
                    "<script>document.getElementById('up').onsubmit=function(){"
                    "this.action='/upload?dir='+encodeURIComponent(document.getElementById('dir').value);};"
                    "</script><p class='muted'>GIFs and PNGs are shrunk to fit the panel; keep them panel-sized. "
                    "Root files: effets.txt (id|name|text|gif|sound), exclusions.txt.</p>");
    snprintf(line, sizeof(line), "<div class='bar'><i style='width:%u%%'></i></div><p class='muted'>%u / %u KB used</p>",
             totalKb ? usedKb * 100 / totalKb : 0, usedKb, totalKb);
    html += line;
    html += F("</div><div class='card'><h2>Files</h2><table>");
    for (const std::string& path : storageListFiles("/", true)) {
        File f = storage().open(path.c_str(), "r");
        const unsigned size = f ? static_cast<unsigned>(f.size()) : 0;
        const std::string escaped = dmd::htmlEscape(path);
        html += "<tr><td>";
        html += escaped.c_str();
        snprintf(line, sizeof(line), "</td><td class='n'>%u KB</td><td class='n'>", (size + 1023) / 1024);
        html += line;
        if (path != CONFIG_FILE) {
            html += "<form method='post' action='/delete'><input type='hidden' name='path' value='";
            html += escaped.c_str();
            html += "'><button class='del' title='Delete'>&#10005;</button></form>";
        }
        html += "</td></tr>";
    }
    html += F("</table></div>");
    server.send(200, "text/html", page("Media", "/files", html));
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
    String html = F("<h1>Settings</h1><p class='muted'>Same keys as Raspy2DMD.cfg and "
                    "<code>conf|Section|key:value</code>. Grey keys are kept for Raspydarts but have no effect on "
                    "the ESP32. Panel size and standalone mode restart the board.</p>"
                    "<form method='post' action='/settings' class='card'>");
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
        html += def.usedOnEsp32 ? "<div class='f'><label>" : "<div class='f off'><label>";
        html += def.key;
        html += "</label><input name='";
        html += dmd::htmlEscape(name).c_str();
        html += "' value='";
        html += dmd::htmlEscape(value).c_str();
        html += "'></div>";
    }
    html += F("</fieldset><input type='submit' value='Save'></form>");
    server.send(200, "text/html", page("Settings", "/settings", html));
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

/**
 * This file load a configuration file from the board's filesystem (LittleFS), parse it and load it into the variables
 * @param _mqtt_url_str The MQTT URL string to be loaded
 * @param _mqtt_path_str The MQTT path string to be loaded
 * @param _hostname The hostname string to be loaded
 * @return true if the configuration was loaded successfully, false otherwise
 */

#include "ConfigHelper.h"
#include "core/ConfigSchema.h"

// Define the static member variable
ConfigHelper* ConfigHelper::_instance = nullptr;

static const char* TAG = "ConfigHelper";

ConfigHelper::~ConfigHelper() {
    if (_instance) {
        delete _instance;
        _instance = nullptr;
    }
}
void ConfigHelper::saveConfigFile() {
    ESP_LOGD(TAG, "Saving configuration...");
    
    // Create a JSON document
    DynamicJsonDocument json(512);
    json["mqtt_url"] = _mqtt_url;
    json["mqtt_path"] = _mqtt_path;
    json["hostname"] = _hostname;
    json["brightness"] = _brightness;
    json["brightnesshours"] = _brightnessHours;
    JsonObject settings = json["settings"].to<JsonObject>();
    for (const auto& kv : _settings) {
        settings[kv.first] = kv.second;
    }
     
    File configFile = storage().open(JSON_CONFIG_FILE, "w");
    if (!configFile) {
        ESP_LOGE(TAG, "Failed to open config file for writing");
        return;
    }
   
    serializeJsonPretty(json, Serial);
    if (serializeJson(json, configFile) == 0) {
        ESP_LOGE(TAG, "Failed to write to file");
    }
    
    ESP_LOGI(TAG, "Saving configuration done");
    configFile.close();
}

bool ConfigHelper::initConfigFile() {
    ESP_LOGD(TAG, "Deleting configuration file...");
    
    if (storage().exists(JSON_CONFIG_FILE)) {
        storage().remove(JSON_CONFIG_FILE);
        ESP_LOGD(TAG, "Deleted configuration file");
    } else {
        ESP_LOGW(TAG, "No Config file exists to delete");
    }
    return true;
}

bool ConfigHelper::loadConfigFile() {
    bool returnFlag = false;
    ESP_LOGD(TAG, "Mounting File System...");
    if(_configLoaded) {
       returnFlag =true;
    }else  if (storageBegin()) {
        ESP_LOGD(TAG, "Mounted file system");
        
        if (storage().exists(JSON_CONFIG_FILE)) {
            ESP_LOGD(TAG, "Reading config file");
            File configFile = storage().open(JSON_CONFIG_FILE, "r");
            
            if (configFile) {
                ESP_LOGD(TAG, "Opened configuration file");
                DynamicJsonDocument json(512);
                DeserializationError error = deserializeJson(json, configFile);
                serializeJsonPretty(json, Serial);
                
                if (!error) {
                    ESP_LOGD(TAG, "Parsing JSON");
                    if (json.containsKey("mqtt_url") && 
                        json.containsKey("mqtt_path") && 
                        json.containsKey("hostname")) {
                        try {

                            ESP_LOGD(TAG,"assigning values to variables");
                            ESP_LOGD(TAG, "MQTT Path: %s, URL: %s, Hostname: %s", 
                                json["mqtt_path"].as<const char*>(), json["mqtt_url"].as<const char*>(), json["hostname"].as<const char*>());
                            
                                
                            const char* path = json["mqtt_path"].as<const char*>();
                            const char* url = json["mqtt_url"].as<const char*>();
                            const char* host = json["hostname"].as<const char*>();
                            
                            _mqtt_path = std::string(path);
                            _mqtt_url = std::string(url);
                            _hostname = std::string(host);
                            // Optional keys, absent from config files written by older firmware.
                            _brightness = json["brightness"] | 90;
                            _brightnessHours = std::string(json["brightnesshours"] | "");
                            _settings.clear();
                            JsonObject settings = json["settings"].as<JsonObject>();
                            if (!settings.isNull()) {
                                for (JsonPair kv : settings) {
                                    // Keys are "Section.key"; keys are case-insensitive like configparser.
                                    std::string k = kv.key().c_str();
                                    const size_t dot = k.find('.');
                                    if (dot != std::string::npos) {
                                        k = k.substr(0, dot + 1) + dmd::normaliseKey(k.substr(dot + 1));
                                    }
                                    _settings[k] = std::string(kv.value() | "");
                                }
                            }

                            ESP_LOGD(TAG, "MQTT Path in variable: %s, URL: %s, Hostname: %s", 
                                _mqtt_path.c_str(), _mqtt_url.c_str(), _hostname.c_str());
                            ESP_LOGD(TAG, "Parsing JSON successful");
                            _configLoaded = true; // Set the flag to true
                            returnFlag = true;
                        } catch (const char* errorMessage) {
                            ESP_LOGE(TAG, "Error opening file: %s", errorMessage);
                            ESP_LOGD(TAG, "MQTT Path: %s, URL: %s, Hostname: %s", 
                                json["mqtt_path"].as<const char*>(), json["mqtt_url"].as<const char*>(), json["hostname"].as<const char*>());
                        }
                    } else {
                        ESP_LOGW(TAG, "Parsing JSON not successful, missing some keys");
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to load json config");
                    initConfigFile();
                }
                configFile.close();
            } else {
                ESP_LOGE(TAG, "Config file cannot be opened");
                initConfigFile();
            }
        } else {
            ESP_LOGW(TAG, "No Config file exists");
        }
    } else {
        ESP_LOGE(TAG, "Failed to mount FS");
    }
   
    return returnFlag;
}

// Getters
const std::string ConfigHelper::getMqttUrl() const {
    return _mqtt_url;
}

const std::string ConfigHelper::getMqttPath() const {
    return _mqtt_path;
}

const std::string ConfigHelper::getHostname() const {
    return _hostname;
}

// Setters
void ConfigHelper::setMqttUrl(const std::string mqtt_url) {
    _mqtt_url = std::move(mqtt_url);
}

void ConfigHelper::setMqttPath(const std::string mqtt_path) {
    _mqtt_path = std::move(mqtt_path);

}

void ConfigHelper::setHostname(const std::string hostname) {
    _hostname = std::move(hostname);
}


std::string ConfigHelper::getSetting(const std::string& section, const std::string& key,
                                     const std::string& fallback) const {
    const std::string k = dmd::normaliseKey(key);
    if (section == "DMDRenderer" && k == "brightness") return std::to_string(_brightness);
    if (section == "DMDRenderer" && k == "brightnesshours") return _brightnessHours.empty() ? fallback : _brightnessHours;
    auto it = _settings.find(section + "." + k);
    return it == _settings.end() || it->second.empty() ? fallback : it->second;
}

long ConfigHelper::getSettingInt(const std::string& section, const std::string& key, long fallback) const {
    const std::string value = getSetting(section, key, "");
    if (value.empty()) {
        return fallback;
    }
    char* end = nullptr;
    long parsed = strtol(value.c_str(), &end, 10);
    return (end == value.c_str() || *end != '\0') ? fallback : parsed;
}

void ConfigHelper::setSetting(const std::string& section, const std::string& key, const std::string& value) {
    const std::string k = dmd::normaliseKey(key);
    if (section == "DMDRenderer" && k == "brightness") {
        _brightness = static_cast<int>(strtol(value.c_str(), nullptr, 10));
        return;
    }
    if (section == "DMDRenderer" && k == "brightnesshours") {
        _brightnessHours = value;
        return;
    }
    _settings[section + "." + k] = value;
}

// Default constructor definition
ConfigHelper::ConfigHelper() {
    // Initialize your member variables here if needed
}

ConfigHelper& ConfigHelper::getInstance() {
    if (_instance == nullptr) {
        _instance = new ConfigHelper();
    }
    return *_instance;
}

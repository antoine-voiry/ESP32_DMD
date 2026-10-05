#include "ConfigHelper.h"

#include <ArduinoJson.h>
#include <esp_log.h>

#include <cstdlib>

#include "Storage.h"
#include "core/ConfigSchema.h"

static const char* TAG = "Config";

namespace {

bool isBrightness(const std::string& section, const std::string& key) {
    return section == "DMDRenderer" && key == "brightness";
}

bool isBrightnessHours(const std::string& section, const std::string& key) {
    return section == "DMDRenderer" && key == "brightnesshours";
}

}  // namespace

ConfigHelper& ConfigHelper::getInstance() {
    static ConfigHelper instance;
    return instance;
}

bool ConfigHelper::loadConfigFile() {
    *this = ConfigHelper();
    if (!storageBegin()) {
        return false;
    }
    File file = storage().open(kFile, "r");
    if (!file) {
        ESP_LOGW(TAG, "No %s yet", kFile);
        return false;
    }
    JsonDocument json;
    const DeserializationError error = deserializeJson(json, file);
    file.close();
    if (error) {
        ESP_LOGE(TAG, "%s is not valid JSON (%s)", kFile, error.c_str());
        return false;
    }
    // `| ""` also covers values of the wrong type.
    _mqttUrl = json["mqtt_url"] | "";
    _mqttPath = json["mqtt_path"] | "";
    _hostname = json["hostname"] | "";
    _brightness = json["brightness"] | 90;
    _brightnessHours = json["brightnesshours"] | "";
    for (JsonPairConst kv : json["settings"].as<JsonObjectConst>()) {
        std::string key = kv.key().c_str();
        const size_t dot = key.find('.');
        if (dot != std::string::npos) {
            key = key.substr(0, dot + 1) + dmd::normaliseKey(key.substr(dot + 1));
        }
        _settings[key] = kv.value() | "";
    }
    const bool complete = json["mqtt_url"].is<const char*>() && json["mqtt_path"].is<const char*>() &&
                          json["hostname"].is<const char*>();
    if (!complete) {
        ESP_LOGW(TAG, "%s lacks the MQTT broker, topic or hostname", kFile);
    }
    return complete;
}

bool ConfigHelper::saveConfigFile() const {
    JsonDocument json;
    json["mqtt_url"] = _mqttUrl;
    json["mqtt_path"] = _mqttPath;
    json["hostname"] = _hostname;
    json["brightness"] = _brightness;
    json["brightnesshours"] = _brightnessHours;
    JsonObject settings = json["settings"].to<JsonObject>();
    for (const auto& kv : _settings) {
        settings[kv.first] = kv.second;
    }
    File file = storage().open(kFile, "w");
    if (!file) {
        ESP_LOGE(TAG, "Cannot write %s", kFile);
        return false;
    }
    const bool ok = serializeJson(json, file) > 0;
    file.close();
    return ok;
}

std::string ConfigHelper::getSetting(const std::string& section, const std::string& key,
                                     const std::string& fallback) const {
    const std::string k = dmd::normaliseKey(key);
    if (isBrightness(section, k)) return std::to_string(_brightness);
    if (isBrightnessHours(section, k)) return _brightnessHours.empty() ? fallback : _brightnessHours;
    auto it = _settings.find(section + "." + k);
    return it == _settings.end() || it->second.empty() ? fallback : it->second;
}

long ConfigHelper::getSettingInt(const std::string& section, const std::string& key, long fallback) const {
    const std::string value = getSetting(section, key, "");
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    return value.empty() || *end != '\0' ? fallback : parsed;
}

void ConfigHelper::setSetting(const std::string& section, const std::string& key, const std::string& value) {
    const std::string k = dmd::normaliseKey(key);
    if (isBrightness(section, k)) {
        _brightness = static_cast<int>(std::strtol(value.c_str(), nullptr, 10));
    } else if (isBrightnessHours(section, k)) {
        _brightnessHours = value;
    } else {
        _settings[section + "." + k] = value;
    }
}

#ifndef CONFIG_HELPER_H
#define CONFIG_HELPER_H

// /config.json: the portal values (MQTT broker, topic, hostname), the brightness, and the Raspy2DMD
// settings set with conf|Section|key:value or the settings page, stored under "settings" as
// "Section.key" (keys lower-cased, like configparser).

#include <map>
#include <string>

class ConfigHelper {
public:
    static constexpr const char* kFile = "/config.json";

    static ConfigHelper& getInstance();

    // Reads the file, replacing everything in memory. False when the file is missing or lacks
    // the portal values (the setup portal is then opened).
    bool loadConfigFile();
    bool saveConfigFile() const;

    const std::string& getMqttUrl() const { return _mqttUrl; }
    const std::string& getMqttPath() const { return _mqttPath; }
    const std::string& getHostname() const { return _hostname; }
    void setMqttUrl(const std::string& value) { _mqttUrl = value; }
    void setMqttPath(const std::string& value) { _mqttPath = value; }
    void setHostname(const std::string& value) { _hostname = value; }

    // [DMDRenderer] brightness (0..100 %) and brightnesshours (24 comma-separated percents).
    int getBrightness() const { return _brightness; }
    void setBrightness(int percent) { _brightness = percent; }
    const std::string& getBrightnessHours() const { return _brightnessHours; }
    void setBrightnessHours(const std::string& hours) { _brightnessHours = hours; }

    // `fallback` when the setting is absent or empty.
    std::string getSetting(const std::string& section, const std::string& key, const std::string& fallback) const;
    long getSettingInt(const std::string& section, const std::string& key, long fallback) const;
    void setSetting(const std::string& section, const std::string& key, const std::string& value);

private:
    ConfigHelper() = default;

    std::string _mqttUrl;
    std::string _mqttPath;
    std::string _hostname;
    int _brightness = 90;
    std::string _brightnessHours;
    std::map<std::string, std::string> _settings;
};

#endif

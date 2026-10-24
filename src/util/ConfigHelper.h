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
    // Optional LAN domain the web pages also answer on (see core/WebGuard.h).
    const std::string& getLanDomain() const { return _lanDomain; }
    void setLanDomain(const std::string& value) { _lanDomain = value; }

    // [DMDRenderer] brightness (0..100 %) and brightnesshours (24 comma-separated percents);
    // set them with setSetting(), which checks the brightness.
    int getBrightness() const { return _brightness; }
    const std::string& getBrightnessHours() const { return _brightnessHours; }

    // `fallback` when the setting is absent or empty.
    std::string getSetting(const std::string& section, const std::string& key, const std::string& fallback) const;
    long getSettingInt(const std::string& section, const std::string& key, long fallback) const;
    // False (and nothing stored) for a brightness that is not a whole number from 0 to 100.
    bool setSetting(const std::string& section, const std::string& key, const std::string& value);

private:
    ConfigHelper() = default;

    std::string _mqttUrl;
    std::string _mqttPath;
    std::string _hostname;
    std::string _lanDomain;
    int _brightness = 90;
    std::string _brightnessHours;
    std::map<std::string, std::string> _settings;
};

#endif

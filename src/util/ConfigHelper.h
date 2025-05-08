/**
 * This file load a configuration file header file
 * * @param _mqtt_url_str The MQTT URL string to be loaded
 * * @param _mqtt_path_str The MQTT path string to be loaded
 * * @param _hostname The hostname string to be loaded
 * * @return true if the configuration was loaded successfully, false otherwise
 * 
 *  */

 // File System Library
#ifndef CONFIG_HELPER_H
#define CONFIG_HELPER_H

// SPI Flash Syetem Library
#include <string>
#include <SPIFFS.h>
#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <esp_log.h>


// JSON configuration file
#define JSON_CONFIG_FILE "/config.json"


class ConfigHelper
{
  private:
    //nothing
    // Variables to hold data from custom textboxes
    std::string  _mqtt_url;
    std::string  _mqtt_path;
    std::string  _hostname;
    boolean _configLoaded = false; // Flag to check if config was saved

    static ConfigHelper* _instance; // Static instance pointer
    // Private constructor to prevent public instantiation
    ConfigHelper();
    // Private method
    bool initConfigFile();

  public:
    void saveConfigFile(); 
    bool loadConfigFile();
    // Getter and Setter declarations for _mqtt_url
    static ConfigHelper& getInstance();
    const std::string getMqttUrl() const;
    void setMqttUrl(const std::string mqttUrl);

    // Getter and Setter declarations for _mqtt_path
    const std::string getMqttPath() const;
    void setMqttPath(std::string mqttPath);

    // Getter and Setter declarations for _hostname
    const std::string getHostname() const;
    void setHostname(std::string hostname);

    boolean isConfigLoaded() const {
        return _configLoaded;
    }
    ~ConfigHelper();


};
#endif
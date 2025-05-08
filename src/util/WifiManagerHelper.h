#ifndef WIFI_MANAGER_HELPER_H
#define WIFI_MANAGER_HELPER_H
#include <Arduino.h>
#include <WiFiManager.h> 
#include <esp_log.h>
#define ESP_DRD_USE_SPIFFS true
#define FORCE_RESET false
 
// Include Libraries
 
// WiFi Library
#include <WiFi.h>
// File System Library
#include <FS.h>
// SPI Flash Syetem Library
#include <SPIFFS.h>
// WiFiManager Library
#include <WiFiManager.h>
// Arduino JSON library
#include <ArduinoJson.h>
#include "ConfigHelper.h"

#include <string>
 

class WifiManagerHelper
{
  private:
    // Define the portal name
    // Variables to hold data from custom textboxes
    bool _shouldSaveConfig = false;    
    bool _callbackRegistered = false;  
    static void WiFiEvent(WiFiEvent_t event);
    void applydefaultWifiSettings();
    WiFiClient _wifiClient;  // Add this member
  public:
    void saveConfigFile(); 
    bool loadConfigFile();
    void saveConfigCallback();
    void configModeCallback(WiFiManager *myWiFiManager);
    void setWMUp(boolean forceConfig, char* hostname) ;
    WiFiClient& getWIFIClient() {  // Return reference instead
      return _wifiClient;
    }
};
#endif
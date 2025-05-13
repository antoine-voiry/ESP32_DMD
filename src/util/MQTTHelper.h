#ifndef MQTTHELPER_H
#define MQTTHELPER_H
#include <ESPmDNS.h>  // Add this include for mDNS support
#include <PubSubClient.h>
#include <WiFi.h>
#include <vector>
#include <string>
#include <esp_log.h>

#define MQTT_MAX_RETRIES 5
#define MQTT_RETRY_DELAY 500
#define MAX_MESSAGE_COUNT 10

class MQTTHelper {
    private:
        MQTTHelper() = delete; // Prevent default constructor
        PubSubClient _mqttClient;
        std::string _mqtt_url;
        std::string _mqtt_client_id;
        std::string _mqtt_topic;
        std::vector<std::string> messageStack;
        
        // Reconnection handling
        unsigned long lastReconnectAttempt = 0;
        uint16_t retryCount = 0;
        uint16_t getRetryCount() const { return retryCount; }
        void resetRetryCount() { retryCount = 0; }
        boolean shouldRetry();
        

        void handleCallback(char* topic, byte* payload, unsigned int length);
     
        boolean connect(); 
    public:
        MQTTHelper(std::string mqtt_url, 
                   std::string mqtt_client_id, 
                   std::string mqtt_topic);
                   
        std::vector<std::string> unStackMessages(int maxCount = MAX_MESSAGE_COUNT);
        void loop();
        boolean handleConnect();
        ~MQTTHelper();
    };

#endif // MQTTHELPER_H

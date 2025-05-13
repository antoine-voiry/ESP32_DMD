#include "MQTTHelper.h"


static const char* TAG = "MQTTHelper";
WiFiClient wifiClient;

MQTTHelper::MQTTHelper(std::string mqtt_url, 
                       std::string mqtt_client_id, 
                       std::string mqtt_topic) 
        : _mqttClient(wifiClient) // Initialize _mqttClient with the WiFiClient
        , _mqtt_url(mqtt_url)
        , _mqtt_client_id(mqtt_client_id)
        , _mqtt_topic(mqtt_topic)
        , lastReconnectAttempt(0)
        , retryCount(0) {
    
    //store passed variables in class variables
    this->_mqtt_url = mqtt_url;
    this->_mqtt_client_id = mqtt_client_id;    
    this->_mqtt_topic = mqtt_topic;
    ESP_LOGE(TAG, "MQTTHelper tag is: '%s'", TAG);
    ESP_LOGI(TAG, "Initializing MQTT Helper with URL: %s, Client: %s, Topic: %s", 
             _mqtt_url.c_str(), _mqtt_client_id.c_str(), _mqtt_topic.c_str());
    ESP_LOGI(TAG, "MQTTHelper tag is: '%s'", TAG);

    // Initialize PubSubClient with WiFi client

    ///handleConnect();
}


boolean MQTTHelper::connect() {
    unsigned long now = millis();
    if (!_mqttClient.connected()) {
        if (now - lastReconnectAttempt > MQTT_RETRY_DELAY) {
            lastReconnectAttempt = now;
            retryCount++;
            
            ESP_LOGI(TAG, "Attempting MQTT connection... (Attempt %d/%d)", retryCount, MQTT_MAX_RETRIES);
            
            //while (!_mqttClient.connected() && shouldRetry()) {
                const char* clientId =  "ESP32DMDClient";
                ESP_LOGI(TAG, "TRYING to connect with server: '%s', Client: '%s', Topic: '%s'", 
                    _mqtt_url.c_str(), clientId, _mqtt_topic.c_str());
                // Log ESP.getFreeHeap()
                ESP_LOGI(TAG, "Free heap: %d", ESP.getFreeHeap());
                // Attempt to connect                
                if (_mqttClient.connect(clientId)) {
                    resetRetryCount();
                    _mqttClient.subscribe(_mqtt_topic.c_str());
                    ESP_LOGI(TAG, "MQTT Connected successfully");
                } else {
                    int state = _mqttClient.state();
                    ESP_LOGE(TAG, "Failed to connect, rc=%d", state);
                    retryCount++;
                    delay(MQTT_RETRY_DELAY);            
                }            
           // }
            
            if (!_mqttClient.connected()) {
                ESP_LOGE(TAG, "Failed to connect after %d attempts", retryCount);
                return false;
            }
        }
    }
    return _mqttClient.connected();
   
}


boolean  MQTTHelper::handleConnect() {
    // Resolve the hostname first

    if(_mqttClient.connected()) {
        //ESP_LOGD(TAG, "Already connected to MQTT broker: '%s'", _mqtt_url.c_str());
        return true;
    }else {
        ESP_LOGE(TAG, "Not connected to: '%s'", _mqtt_url.c_str());
        _mqttClient.setServer(_mqtt_url.c_str(), 1883);
        // Set callback for incoming messages
        _mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
            this->handleCallback(topic, payload, length);
        });

        // Set buffer size
        _mqttClient.setBufferSize(1024);        
        ESP_LOGI(TAG, "MQTT Helper parameters set, we will try to connect");
        // Initial connection attempt
        return connect();            
    }


}

std::vector<std::string> MQTTHelper::unStackMessages(int maxCount) {
    ESP_LOGV(TAG, "Unstacking up to %d messages", maxCount);
    
    std::vector<std::string> messages;
    int messageCount = 0;
    // Get up to maxCount messages from the stack
    while (!messageStack.empty() && messageCount < maxCount) {
        messages.push_back(messageStack.back());
        messageStack.pop_back();
        messageCount++;
    }
    if(messageCount > 0) {
        ESP_LOGI(TAG, "Unstacked %d messages", messageCount);
    }
    return messages;
}


void MQTTHelper::handleCallback(char* topic, byte* payload, unsigned int length) {
    std::string message(reinterpret_cast<char*>(payload), length);
    if(!_mqttClient.connected()) {
        ESP_LOGE(TAG, "MQTT client not connected, trying to reconnect");
        connect();
    }

    messageStack.push_back(message);
    ESP_LOGD(TAG, "Received message on topic %s: %s", topic, message.c_str());
}

void MQTTHelper::loop() {
    // Call the loop function to process incoming messages
    if (_mqttClient.connected()) {
        _mqttClient.loop();
    }
}


MQTTHelper::~MQTTHelper() {
    if (_mqttClient.connected()) {
        ESP_LOGI(TAG, "Disconnecting from MQTT broker");
        _mqttClient.disconnect();
    }
}




boolean MQTTHelper::shouldRetry() { 
    return retryCount < MQTT_MAX_RETRIES; 
}
#include "LocalMQTTBroker.h"
static const char* TAG = "LocalMQTTBroker";

void LocalMQTTBroker::startBroker() {
    // Assume Wi-Fi is already connected.  Do not configure it here.
    ESP_LOGI(TAG, "Starting MQTT Broker on port %d", MQTT_PORT);
    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGE(TAG, "Cannot start MQTT Broker - WiFi not connected");
        return;
    }
    ESP_LOGI(TAG, "Starting MQTT Broker on %s:%d", 
        WiFi.localIP().toString().c_str(), MQTT_PORT);

    brokerStarted = broker.init(MQTT_PORT);
    if (!brokerStarted) {
        ESP_LOGE(TAG, "Failed to start LOCAL MQTT Broker on port %d", MQTT_PORT);
    }else {
        ESP_LOGI(TAG, "LOCAL MQTT Broker started successfully on port %d", MQTT_PORT);
    }
}

bool LocalMQTTBroker::attemptRestart() {
    unsigned long currentMillis = millis();
    
    if (currentMillis - lastReconnectAttempt >= RECONNECT_INTERVAL) {
        lastReconnectAttempt = currentMillis;
        
        if (WiFi.status() == WL_CONNECTED) {
            ESP_LOGI(TAG, "Attempting broker restart...");
            brokerStarted = broker.init(MQTT_PORT);
            
            if (brokerStarted) {
                ESP_LOGI(TAG, "Broker successfully restarted");
                return true;
            }
        }
    }
    return false;
}

void LocalMQTTBroker::handleClientConnections() {
    if (!brokerStarted) {
        ESP_LOGE(TAG, "MQTT broker will not work as not started or WIFI not connected");
        //TODO: handle the reconnection 
        attemptRestart();
    } 
    if(this->isBrokerRunning()) {
        ESP_LOGI(TAG, "MQTT Broker UPDATING clients");
        broker.update(); // Call this regularly in the loop() function
    }
}


bool LocalMQTTBroker::isBrokerRunning() { 
    return brokerStarted; 
}
uint16_t LocalMQTTBroker::getBrokerPort() { 
    return MQTT_PORT; 
}
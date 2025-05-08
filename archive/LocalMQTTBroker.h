#ifndef LOCALMQTT_BROKER_H
#define LOCALMQTT_BROKER_H
#include <sMQTTBroker.h>
#include <esp_log.h>
#include <WiFi.h>




class LocalMQTTBroker {
private:
    sMQTTBroker broker;
    boolean brokerStarted = false;
    const unsigned short MQTT_PORT = 1883;
    unsigned long lastReconnectAttempt = 0;
    const unsigned long RECONNECT_INTERVAL = 5000;
    bool attemptRestart();
public:
    

    void startBroker();
    void handleClientConnections();
    bool isBrokerRunning();
    uint16_t getBrokerPort();

};

#endif
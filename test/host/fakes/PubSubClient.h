#ifndef FAKE_PUBSUBCLIENT_H
#define FAKE_PUBSUBCLIENT_H

// Talks to fake::mqtt instead of a broker.

#include <functional>
#include <string>

#include "WiFi.h"

class PubSubClient {
public:
    using Callback = std::function<void(char*, uint8_t*, unsigned int)>;

    explicit PubSubClient(WiFiClient&) {}
    PubSubClient& setServer(const char* host, uint16_t port);
    PubSubClient& setCallback(Callback callback);
    bool setBufferSize(uint16_t size);
    bool connect(const char* clientId);
    bool connected();
    int state();
    bool subscribe(const char* topic);
    bool publish(const char* topic, const char* payload);
    // Delivers the messages queued in fake::mqtt.incoming.
    bool loop();

private:
    Callback _callback;
    uint16_t _bufferSize = 256;
};

#endif

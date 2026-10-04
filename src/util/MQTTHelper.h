#ifndef MQTTHELPER_H
#define MQTTHELPER_H

// MQTT connection to the Raspydarts broker: subscribes to the DMD topic and queues the payloads
// for loop(), which renders them in arrival order.

#include <PubSubClient.h>
#include <WiFi.h>

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

class MQTTHelper {
public:
    static constexpr uint16_t kPort = 1883;
    // Payloads beyond this many waiting messages are dropped (oldest first), so a flood of
    // messages cannot exhaust the heap.
    static constexpr size_t kMaxQueued = 32;
    static constexpr uint32_t kRetryMs = 5000;

    MQTTHelper(std::string host, std::string clientId, std::string topic);

    // Keeps the connection up (one attempt every kRetryMs) and reads incoming packets.
    // Returns true while connected.
    bool loop(uint32_t nowMs);

    // Received payloads, oldest first, at most maxCount.
    std::vector<std::string> takeMessages(size_t maxCount = 10);

    // False when disconnected or when the message does not fit the client buffer.
    bool publish(const std::string& topic, const std::string& payload);

    size_t droppedMessages() const { return _dropped; }

private:
    void onMessage(const char* topic, const uint8_t* payload, unsigned int length);

    WiFiClient _net;
    PubSubClient _client;
    std::string _host;
    std::string _clientId;
    std::string _topic;
    std::deque<std::string> _queue;
    size_t _dropped = 0;
    uint32_t _lastAttemptMs = 0;
    bool _attempted = false;
};

#endif

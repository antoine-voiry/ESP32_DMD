#include "MQTTHelper.h"

#include <esp_log.h>

#include <utility>

static const char* TAG = "MQTT";

MQTTHelper::MQTTHelper(std::string host, std::string clientId, std::string topic)
    : _client(_net), _host(std::move(host)), _clientId(std::move(clientId)), _topic(std::move(topic)) {
    _client.setServer(_host.c_str(), kPort);
    _client.setBufferSize(1024);
    _client.setSocketTimeout(kSocketTimeoutSec);  // see MQTTHelper.h
    _client.setCallback([this](char* t, uint8_t* payload, unsigned int length) { onMessage(t, payload, length); });
}

bool MQTTHelper::loop(uint32_t nowMs) {
    if (!_client.connected()) {
        if (_attempted && nowMs - _lastAttemptMs < _retryMs) {
            return false;
        }
        _attempted = true;
        const unsigned long before = millis();
        const bool ok = _client.connect(_clientId.c_str());
        // From the end of the attempt: a slow failure must not eat into the wait.
        _lastAttemptMs = nowMs + static_cast<uint32_t>(millis() - before);
        if (!ok) {
            // 5 s after the first failure in a row, then 10, 20, 30, 30...
            _retryMs = _failures == 0 ? kRetryMs : (_retryMs >= kMaxRetryMs / 2 ? kMaxRetryMs : _retryMs * 2);
            ++_failures;
            ESP_LOGW(TAG, "Cannot connect to %s:%u as %s (state %d), next try in %u s", _host.c_str(), kPort,
                     _clientId.c_str(), _client.state(), static_cast<unsigned>(_retryMs / 1000));
            return false;
        }
        _failures = 0;
        _retryMs = kRetryMs;
        _client.subscribe(_topic.c_str());
        ESP_LOGI(TAG, "Connected to %s, subscribed to %s", _host.c_str(), _topic.c_str());
    }
    _client.loop();
    return _client.connected();
}

void MQTTHelper::onMessage(const char* topic, const uint8_t* payload, unsigned int length) {
    // Runs inside PubSubClient::loop(): only queue the payload.
    if (_queue.size() >= kMaxQueued) {
        _queue.pop_front();
        ++_dropped;
        ESP_LOGW(TAG, "Too many waiting messages, dropped the oldest (%u so far)", static_cast<unsigned>(_dropped));
    }
    _queue.emplace_back(reinterpret_cast<const char*>(payload), length);
    ESP_LOGD(TAG, "%s: %s", topic, _queue.back().c_str());
}

std::vector<std::string> MQTTHelper::takeMessages(size_t maxCount) {
    std::vector<std::string> messages;
    while (!_queue.empty() && messages.size() < maxCount) {
        messages.push_back(std::move(_queue.front()));
        _queue.pop_front();
    }
    return messages;
}

bool MQTTHelper::publish(const std::string& topic, const std::string& payload) {
    return _client.connected() && _client.publish(topic.c_str(), payload.c_str());
}

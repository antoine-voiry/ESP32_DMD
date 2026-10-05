#ifndef FAKE_ESPMDNS_H
#define FAKE_ESPMDNS_H
#include "Arduino.h"
class MDNSResponder {
public:
    bool begin(const char* hostname);
    void addService(const char* service, const char* proto, uint16_t port);
    // fake::mdnsHosts, by name without ".local"
    IPAddress queryHost(const char* host, uint32_t timeoutMs = 2000);
};
extern MDNSResponder MDNS;
#endif

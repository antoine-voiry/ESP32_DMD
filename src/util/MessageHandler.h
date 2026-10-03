#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

// Port of UnstackMessages() from Raspy2DMD ServerRaspy2DMD.py: maps "action|args" payloads
// to DMDRenderer calls. Payloads are expected to have passed MessageFilter first.

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "DMDRenderer.h"

class MessageHandler {
private:
    using Handler = std::function<void(const std::vector<std::string>&)>;
    std::map<std::string, Handler> handlers;
    DMDRenderer* dmdRenderer;
    MessageHandler() = delete;
    void setupHandlers();
    void notPortedYet(const char* action, const char* phase);
    void applyConf(const std::vector<std::string>& params);
    // "|N" trailing argument at index i (seconds) -> milliseconds, 0 when absent or invalid.
    static uint32_t holdArg(const std::vector<std::string>& params, size_t i);

public:
    explicit MessageHandler(DMDRenderer* renderer);
    void handleMessage(const std::string& message);
};
#endif

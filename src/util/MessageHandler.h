#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include "MQTTHelper.h"
#include "DMDRenderer.h"
#include <map>
#include <functional>
#include <sstream>

class MessageHandler {
private:
    std::map<std::string, std::function<void(const std::vector<std::string>&)>> handlers;
    DMDRenderer* dmdRenderer;
    MessageHandler() = delete; // Prevent default constructor
    void setupHandlers();

public:
    MessageHandler(DMDRenderer* renderer);
    void handleMessage(const std::string& message);
    std::vector<std::string> parseMessage(const std::string& message);
};
#endif
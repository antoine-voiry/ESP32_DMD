#include "MessageFilter.h"
#include <sstream>

bool MessageFilter::isValidMessage(const std::string& message) {
    // Check if message is empty
    if(message.empty()) {
        return false;
    }

    // Find delimiter position
    size_t delimPos = message.find('|');
    if(delimPos == std::string::npos) {
        return false;
    }

    // Extract action part
    std::string action = message.substr(0, delimPos);

    // Check if action is in valid messages list
    return std::find(validMessages.begin(), validMessages.end(), action) != validMessages.end();
}

std::string MessageFilter::getMessageAction(const std::string& message) {
    size_t delimPos = message.find('|');
    if(delimPos == std::string::npos) {
        return "";
    }
    return message.substr(0, delimPos);
}

std::vector<std::string> MessageFilter::getMessageParameters(const std::string& message) {
    std::vector<std::string> params;
    std::stringstream ss(message);
    std::string item;

    // Split message by '|' delimiter
    while(std::getline(ss, item, '|')) {
        params.push_back(item);
    }

    // Remove action from parameters if we have any parameters
    if(!params.empty()) {
        params.erase(params.begin());
    }

    return params;
}
#include "MessageFilter.h"

#include "core/Protocol.h"

bool MessageFilter::isValidMessage(const std::string& message) {
    return dmd::isAcceptedPayload(message);
}

std::string MessageFilter::getMessageAction(const std::string& message) {
    dmd::Command command;
    return dmd::parseCommand(message, command) ? command.action : "";
}

std::vector<std::string> MessageFilter::getMessageParameters(const std::string& message) {
    dmd::Command command;
    dmd::parseCommand(message, command);
    return command.args;
}

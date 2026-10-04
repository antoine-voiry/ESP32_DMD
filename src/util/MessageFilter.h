#ifndef MESSAGE_FILTER_H
#define MESSAGE_FILTER_H

#include <string>
#include <vector>

// Port of FilterMessages() from Raspy2DMD ServerRaspy2DMD.py (logic lives in core/Protocol).
class MessageFilter {
public:
    // Known "action|..." payload; "score" payloads must carry a valid dart string.
    bool isValidMessage(const std::string& message);
    std::string getMessageAction(const std::string& message);
    std::vector<std::string> getMessageParameters(const std::string& message);
};
#endif

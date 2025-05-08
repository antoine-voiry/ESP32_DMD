#ifndef MESSAGE_FILTER_H
#define MESSAGE_FILTER_H

#include <string>
#include <vector>
#include <algorithm>

class MessageFilter {
private:
    std::vector<std::string> validMessages = {
        "rebt", "shutdwn", "excludeFolder", "excludeFile", 
        "owmzc", "fllcn", "meteo", "meteoPrevi", 
        "receipconf", "rldconf", "conf", "msg", 
        "waiter", "score", "msgmove", "msgmovebcl", 
        "msgcarrou", "msgcolor", "msgimg", "testFont", 
        "testPattern", "rand", "demo", "gif", 
        "gifText", "gifPath", "img", "time",
        "sound", "effet", "soundeffet", "perf", 
        "edfJoursTempo"
    };

public:
    bool isValidMessage(const std::string& message);
    std::string getMessageAction(const std::string& message);
    std::vector<std::string> getMessageParameters(const std::string& message);
};
#endif
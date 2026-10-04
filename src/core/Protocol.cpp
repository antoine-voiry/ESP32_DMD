#include "Protocol.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace dmd {

namespace {

struct ActionSpec {
    const char* name;
    size_t minArgs;
};

// Order and minimum argument counts follow ServerRaspy2DMD.py UnstackMessages().
const ActionSpec kActions[] = {
    {"rebt", 0},       {"shutdwn", 0},    {"excludeFolder", 2}, {"excludeFile", 2},
    {"owmzc", 0},      {"fllcn", 0},      {"meteo", 0},         {"meteoPrevi", 0},
    {"receipconf", 0}, {"rldconf", 0},    {"conf", 1},          {"msg", 1},
    {"waiter", 1},     {"score", 1},      {"msgmove", 2},       {"msgmovebcl", 3},
    {"msgcarrou", 1},  {"msgcolor", 3},   {"msgimg", 2},        {"testFont", 1},
    {"testPattern", 1}, {"rand", 1},      {"demo", 1},          {"gif", 1},
    {"gifText", 2},    {"gifPath", 1},    {"img", 1},           {"time", 1},
    {"sound", 1},      {"effet", 1},      {"soundeffet", 3},    {"perf", 0},
    {"edfJoursTempo", 0},
};

const ActionSpec* findAction(const std::string& action) {
    for (const auto& spec : kActions) {
        if (action == spec.name) {
            return &spec;
        }
    }
    return nullptr;
}

bool isSegment(const std::string& dart) {
    if (dart == "SB" || dart == "DB" || dart == "X") {
        return true;
    }
    if (dart.size() < 2 || dart.size() > 3) {
        return false;
    }
    if (dart[0] != 'S' && dart[0] != 'D' && dart[0] != 'T') {
        return false;
    }
    // Reject leading zeros ("S05") the same way a list lookup would.
    if (dart[1] == '0') {
        return false;
    }
    for (size_t i = 1; i < dart.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(dart[i]))) {
            return false;
        }
    }
    int value = std::atoi(dart.c_str() + 1);
    return value >= 1 && value <= 20;
}

}  // namespace

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

bool parseCommand(const std::string& payload, Command& out) {
    out.action.clear();
    out.args.clear();
    size_t pos = payload.find('|');
    if (pos == std::string::npos) {
        return false;
    }
    out.action = payload.substr(0, pos);
    size_t start = pos + 1;
    while (true) {
        size_t next = payload.find('|', start);
        if (next == std::string::npos) {
            out.args.push_back(payload.substr(start));
            break;
        }
        out.args.push_back(payload.substr(start, next - start));
        start = next + 1;
    }
    // Python's str.split keeps a trailing empty field ("msg|" -> ["msg", ""]), which matches the above.
    return true;
}

bool isKnownAction(const std::string& action) {
    return findAction(action) != nullptr;
}

size_t minArgs(const std::string& action) {
    const ActionSpec* spec = findAction(action);
    return spec ? spec->minArgs : 0;
}

std::vector<std::string> splitScore(const std::string& score) {
    std::vector<std::string> darts;
    const std::string upper = toUpper(score);
    const std::string sep = " - ";
    size_t start = 0;
    while (true) {
        size_t next = upper.find(sep, start);
        if (next == std::string::npos) {
            darts.push_back(upper.substr(start));
            break;
        }
        darts.push_back(upper.substr(start, next - start));
        start = next + sep.size();
    }
    return darts;
}

bool isValidScore(const std::string& score) {
    for (const auto& dart : splitScore(score)) {
        if (!isSegment(dart)) {
            return false;
        }
    }
    return true;
}

bool isAcceptedPayload(const std::string& payload) {
    Command cmd;
    if (!parseCommand(payload, cmd) || !isKnownAction(cmd.action)) {
        return false;
    }
    if (cmd.action == "score") {
        return !cmd.args.empty() && isValidScore(cmd.args[0]);
    }
    return true;
}

long parseIntArg(const std::string& s, long fallback) {
    if (s.empty()) {
        return fallback;
    }
    char* end = nullptr;
    long value = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0') {
        return fallback;
    }
    return value;
}

}  // namespace dmd

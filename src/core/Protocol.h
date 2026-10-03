#ifndef DMD_CORE_PROTOCOL_H
#define DMD_CORE_PROTOCOL_H

// Raspydarts -> DMD MQTT protocol, ported from Raspy2DMD ServerRaspy2DMD.py.
// Payloads are "action|arg1|arg2|...". Pure C++ (no Arduino) so it can be unit tested on the host.

#include <cstdint>
#include <string>
#include <vector>

namespace dmd {

struct Command {
    std::string action;
    std::vector<std::string> args;
};

// Splits a payload on '|'. Returns false if the payload has no '|' (the original rejects those).
bool parseCommand(const std::string& payload, Command& out);

// True if the action is one of the 33 actions Raspy2DMD accepts.
bool isKnownAction(const std::string& action);

// Minimum number of arguments the original dispatcher requires for an action (0 if none).
size_t minArgs(const std::string& action);

// Port of FilterMessages(): known action, and for "score" a valid dart string.
bool isAcceptedPayload(const std::string& payload);

// Port of ScoreReceived(): "S20 - T20 - X" style string, every dart a known segment (case-insensitive).
bool isValidScore(const std::string& score);

// Splits "S20 - T20 - X" into upper-cased darts.
std::vector<std::string> splitScore(const std::string& score);

// Parses an integer argument; returns fallback when empty or not a number.
long parseIntArg(const std::string& s, long fallback);

std::string toUpper(std::string s);

}  // namespace dmd

#endif

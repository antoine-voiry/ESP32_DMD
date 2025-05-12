#ifndef MESSAGE_FILTER_H
#define MESSAGE_FILTER_H

#include <string>
#include <vector>
#include <algorithm>

class MessageFilter {
private:
    std::vector<std::string> validMessages = {
        "rebt",          // Reboot system
        "shutdwn",       // Shutdown system
        "excludeFolder", // Exclude a folder from processing
        "excludeFile",   // Exclude a file from processing
        "owmzc",         // OpenWeatherMap ZIP code
        "fllcn",         // Find latitude/longitude/city name
        "meteo",         // Show current weather
        "meteoPrevi",    // Show weather forecast
        "receipconf",    // Receive configuration
        "rldconf",       // Reload configuration
        "conf",          // Configuration
        "msg",           // Display text message
        "waiter",        // Waiter mode
        "score",         // Display score
        "msgmove",       // Moving text message
        "msgmovebcl",    // Moving text with backcolor
        "msgcarrou",     // Text carousel
        "msgcolor",      // Colored text message
        "msgimg",        // Image with text
        "testFont",      // Test font rendering
        "testPattern",   // Test display pattern
        "rand",          // Random display
        "demo",          // Demo mode
        "gif",           // Display GIF
        "gifText",       // GIF with text overlay
        "gifPath",       // GIF from path
        "img",           // Display image
        "time",          // Show time
        "sound",         // Play sound
        "effet",         // Special effect
        "soundeffet",    // Sound with effect
        "perf",          // Performance test
        "edfJoursTempo"  // EDF Tempo days display
    };

public:
    bool isValidMessage(const std::string& message);
    std::string getMessageAction(const std::string& message);
    std::vector<std::string> getMessageParameters(const std::string& message);
};
#endif
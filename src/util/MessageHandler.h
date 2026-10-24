#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

// Port of FilterMessages() and UnstackMessages() from Raspy2DMD ServerRaspy2DMD.py: checks
// "action|args" payloads and maps them to DMDRenderer calls.

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "AttractController.h"
#include "DMDRenderer.h"
#include "MediaLibrary.h"
#include "OnlineService.h"
#include "TimeService.h"
#include "core/Protocol.h"

class MessageHandler {
private:
    using Handler = std::function<void(const std::vector<std::string>&)>;
    std::map<std::string, Handler> handlers;
    DMDRenderer* dmdRenderer;
    AttractController* attract;
    TimeService* timeService;
    MediaLibrary* media;
    OnlineService* online;
    void lookUpZipCode();
    std::function<bool(const std::string&, const std::string&)> publisher;
    bool restartPending = false;
    MessageHandler() = delete;
    void setupHandlers();
    // False for commands standalone mode ignores.
    bool accepts(const dmd::Command& command) const;
    void applyConf(const std::vector<std::string>& params);
    // RenderSoundEffet(): GIF with text, text, GIF, or (no audio here) nothing.
    void playEffect(const std::string& text, const std::string& gif, const std::string& sound);
    // Effect duration in seconds at index i -> milliseconds (default 5 s, capped at 10 min).
    static uint32_t effectDuration(const std::vector<std::string>& params, size_t i);
    // "|N" trailing argument at index i (seconds) -> milliseconds, 0 when absent or invalid,
    // capped at kMaxHoldSec (a hold cannot be interrupted, so "msg|Hi|99999" would freeze the queue).
    static uint32_t holdArg(const std::vector<std::string>& params, size_t i);

public:
    static constexpr long kMaxHoldSec = 10 * 60;

    MessageHandler(DMDRenderer* renderer, AttractController* attract, TimeService* timeService,
                   MediaLibrary* media, OnlineService* online);
    // The one path for every command, from MQTT or the web page. False (and nothing happens, not
    // even an interrupt) for invalid payloads and for commands standalone mode ignores, and false
    // for a command missing arguments (it interrupts the display, like the original, then stops).
    // Otherwise re-arms the attract mode countdown, runs the command and returns true: true means
    // the command ran.
    // *interrupted (if given) tells whether the display was interrupted, whatever the result.
    bool handleMessage(const std::string& message, bool* interrupted = nullptr);
    // True when there is a handler for the action (every dmd::knownActions() entry must have one).
    bool handles(const std::string& action) const;
    // receipconf answers through this (topic, payload).
    void setPublisher(std::function<bool(const std::string&, const std::string&)> publish) {
        publisher = std::move(publish);
    }
    // receipconf: every setting as "Section:key:value", like SendConfigToRaspydarts().
    void sendConfig();
    // rldconf: re-applies the settings cached at start-up (text style, brightness, timezone, caches).
    void reloadSettings();
};
#endif

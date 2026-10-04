#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

// Port of UnstackMessages() from Raspy2DMD ServerRaspy2DMD.py: maps "action|args" payloads
// to DMDRenderer calls. Payloads are expected to have passed
// dmd::isAcceptedPayload() first.

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "AttractController.h"
#include "DMDRenderer.h"
#include "MediaLibrary.h"
#include "OnlineService.h"
#include "TimeService.h"

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
    void applyConf(const std::vector<std::string>& params);
    // RenderSoundEffet(): GIF with text, text, GIF, or (no audio here) nothing.
    void playEffect(const std::string& text, const std::string& gif, const std::string& sound);
    // Effect duration in seconds at index i -> milliseconds (default 5 s, capped at 10 min).
    static uint32_t effectDuration(const std::vector<std::string>& params, size_t i);
    // "|N" trailing argument at index i (seconds) -> milliseconds, 0 when absent or invalid.
    static uint32_t holdArg(const std::vector<std::string>& params, size_t i);

public:
    MessageHandler(DMDRenderer* renderer, AttractController* attract, TimeService* timeService,
                   MediaLibrary* media, OnlineService* online);
    void handleMessage(const std::string& message);
    // False for commands standalone mode ignores: they must not even interrupt the display.
    bool accepts(const std::string& message) const;
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

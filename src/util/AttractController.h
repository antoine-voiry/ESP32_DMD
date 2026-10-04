#ifndef DMD_ATTRACT_CONTROLLER_H
#define DMD_ATTRACT_CONTROLLER_H

// Port of the attract mode ("waiter", DMDRenderer.run()) and the text carousel
// (RenderCarrouselText): while active, queues the next show whenever the renderer is idle.

#include <cstdint>
#include <memory>
#include <string>

#include "core/Attract.h"
#include "core/Fx.h"

class DMDRenderer;
class MediaLibrary;
class OnlineService;

class AttractController {
public:
    AttractController(DMDRenderer* renderer, MediaLibrary* media, OnlineService* online);

    // waiter|start: play Running.scrollOrder. msgcarrou|start: play the carousel only ("4").
    void start(const std::string& codes);
    void stop();
    bool running() const { return _running; }

    // Every incoming message stops the attract mode (as Stop() ended run() on the Pi)
    // and restarts the Running.attract_mode idle countdown.
    void onMessage(uint32_t nowMs);

    void loop(uint32_t nowMs);

    // Show codes this port can play: 1 random GIF, 2 random image, T clock, 4 text carousel,
    // M weather, P forecast, E EDF Tempo, S board status, F random effect (ESP32 extension).
    static constexpr const char* kSupportedCodes = "12T4MPESF";

private:
    bool play(char code);
    bool playCarousel();

    DMDRenderer* _renderer;
    MediaLibrary* _media;
    OnlineService* _online;
    std::unique_ptr<dmd::AttractPlaylist> _playlist;
    dmd::IdleTimer _idle;
    dmd::Rng _rng;
    bool _running = false;
};

#endif

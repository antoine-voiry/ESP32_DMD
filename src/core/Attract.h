#ifndef DMD_CORE_ATTRACT_H
#define DMD_CORE_ATTRACT_H

// Attract mode ("waiter") and text carousel, from DMDRenderer.run() / RenderCarrousel().

#include <cstdint>
#include <string>
#include <vector>

#include "Motion.h"

namespace dmd {

// Running.scrollOrder: comma-separated show codes, played in a loop while idle.
// Original codes: 1 random GIF, 2 random image, 4 text carousel, T clock, M weather,
// P forecast, E EDF Tempo, S SoC monitor. ESP32 extension: F random fx animation.
class AttractPlaylist {
public:
    // Keeps only the codes listed in `supported`, in order. Unknown/unsupported codes are skipped.
    AttractPlaylist(const std::string& scrollOrder, const std::string& supported);
    bool empty() const { return _codes.empty(); }
    // Next code, looping forever. Returns '\0' when empty.
    char next();
    const std::vector<char>& codes() const { return _codes; }

private:
    std::vector<char> _codes;
    size_t _pos = 0;
};

// One text file of the carousel (Directory.textes): the first line holds ';'-separated options,
// the remaining lines the message.
struct CarouselEntry {
    bool valid = false;           // false when the file has no message lines
    bool hasOptions = false;      // the original waits 4 s after a message without options
    Motion motion = Motion::None;
    bool randomMotion = false;    // option "A": pick a movement at random
    int iterations = 1;           // option "ITn"
    std::string gifBackground;    // option "♠name.gif" (needs GIF playback, phase 3)
    std::string message;
};

CarouselEntry parseCarouselFile(const std::string& content);

// Every movement the carousel's "A" option can pick from, in the original order.
Motion carouselRandomMotion(uint32_t random);

// Raspydarts-side idle timer: attract mode starts attractAfterMs after the last message.
// 0 disables it (Running.attract_mode = 0, the default).
class IdleTimer {
public:
    void configure(uint32_t attractAfterMs) { _afterMs = attractAfterMs; }
    void touch(uint32_t nowMs) { _last = nowMs; _armed = true; }
    // True once, when the delay has elapsed since the last touch().
    bool expired(uint32_t nowMs);

private:
    uint32_t _afterMs = 0;
    uint32_t _last = 0;
    bool _armed = false;
};

}  // namespace dmd

#endif

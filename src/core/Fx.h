#ifndef DMD_CORE_FX_H
#define DMD_CORE_FX_H

// Arduino-free maths for the visual effects (not part of the original Raspy2DMD).
// Everything is deterministic for a given seed so it can be unit tested on the host.

#include <cstdint>
#include <string>
#include <vector>

#include "TextUtil.h"

namespace dmd {

// Background animations, drawn behind (optional) text.
enum class FxBackground { None, Plasma, Fireworks, Starfield, MatrixRain };
// How the text pixels are coloured / revealed.
enum class FxText { Solid, Rainbow, Wave, Typewriter, Sparkle };

// "plasma", "fireworks", "stars", "matrix"; returns false for anything else.
bool parseFxBackground(const std::string& name, FxBackground& out);
// "solid", "rainbow", "wave", "typewriter", "sparkle"; returns false for anything else.
bool parseFxText(const std::string& name, FxText& out);

// Small, fast PRNG (xorshift32). Never seeded with 0.
class Rng {
public:
    explicit Rng(uint32_t seed = 0x9E3779B9u) : _state(seed ? seed : 0x9E3779B9u) {}
    uint32_t next();
    // Uniform integer in [lo, hi].
    int range(int lo, int hi);
    // Uniform float in [0, 1).
    float unit();

private:
    uint32_t _state;
};

// hue/sat/val 0..255 -> RGB (rainbow wheel).
Rgb hsv(uint8_t hue, uint8_t sat = 255, uint8_t val = 255);
// 0..255 sine approximation: sin8(0) = 128, sin8(64) = 255, sin8(192) = 0.
uint8_t sin8(uint8_t angle);
// Scales an RGB colour by level/255.
Rgb scale(const Rgb& c, uint8_t level);

// Number of text columns visible after elapsedMs when revealing one column every msPerColumn.
int typewriterColumns(uint32_t elapsedMs, uint32_t msPerColumn, int totalColumns);
// Vertical offset (pixels) of column x for the wave text effect at time t.
int waveOffset(int x, uint32_t tMs, int amplitude = 2);

// Rockets rise from the bottom and burst into fading sparks with gravity.
class Fireworks {
public:
    struct Particle {
        float x, y, vx, vy;
        uint8_t hue;
        uint16_t lifeMs;     // remaining life
        uint16_t maxLifeMs;  // for fading
        bool rocket;
    };

    Fireworks(int width, int height, uint32_t seed = 1);
    // Advances the simulation; launches a rocket roughly every launchEveryMs.
    void step(uint32_t dtMs);
    const std::vector<Particle>& particles() const { return _particles; }
    // Brightness 0..255 of a particle (sparks fade out with age).
    static uint8_t level(const Particle& p);

    static constexpr size_t kMaxParticles = 220;
    uint32_t launchEveryMs = 600;

private:
    void launch();
    void burst(const Particle& rocket);

    int _w, _h;
    Rng _rng;
    uint32_t _sinceLaunch;
    std::vector<Particle> _particles;
};

// Parallax stars scrolling right to left.
class Starfield {
public:
    struct Star {
        float x;
        int y;
        uint8_t speed;  // 1..3, also used for brightness
    };
    Starfield(int width, int height, size_t count = 40, uint32_t seed = 2);
    void step(uint32_t dtMs);
    const std::vector<Star>& stars() const { return _stars; }

private:
    int _w, _h;
    Rng _rng;
    std::vector<Star> _stars;
};

// "Digital rain": per column, a bright head falling with a fading green trail.
class MatrixRain {
public:
    struct Drop {
        float head;     // y of the head (may be above or below the panel)
        float speed;    // pixels per second
        uint8_t trail;  // trail length in pixels
    };
    MatrixRain(int width, int height, uint32_t seed = 3);
    void step(uint32_t dtMs);
    const std::vector<Drop>& drops() const { return _drops; }  // one per column
    // Brightness 0..255 of pixel (x, y): 255 at the head, fading along the trail, 0 elsewhere.
    uint8_t level(int x, int y) const;

private:
    void respawn(Drop& d, bool anywhere);

    int _w, _h;
    Rng _rng;
    std::vector<Drop> _drops;
};

}  // namespace dmd

#endif

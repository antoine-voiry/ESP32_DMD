#include "Fx.h"

#include <cmath>

namespace dmd {

bool parseFxBackground(const std::string& name, FxBackground& out) {
    if (name == "plasma") { out = FxBackground::Plasma; return true; }
    if (name == "fireworks") { out = FxBackground::Fireworks; return true; }
    if (name == "stars") { out = FxBackground::Starfield; return true; }
    if (name == "matrix") { out = FxBackground::MatrixRain; return true; }
    return false;
}

bool parseFxText(const std::string& name, FxText& out) {
    if (name == "solid") { out = FxText::Solid; return true; }
    if (name == "rainbow") { out = FxText::Rainbow; return true; }
    if (name == "wave") { out = FxText::Wave; return true; }
    if (name == "typewriter") { out = FxText::Typewriter; return true; }
    if (name == "sparkle") { out = FxText::Sparkle; return true; }
    return false;
}

uint32_t Rng::next() {
    _state ^= _state << 13;
    _state ^= _state >> 17;
    _state ^= _state << 5;
    return _state;
}

int Rng::range(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + static_cast<int>(next() % static_cast<uint32_t>(hi - lo + 1));
}

float Rng::unit() {
    return (next() >> 8) / 16777216.0f;
}

Rgb hsv(uint8_t hue, uint8_t sat, uint8_t val) {
    const uint8_t region = hue / 43;
    const uint8_t rem = static_cast<uint8_t>((hue - region * 43) * 6);
    const uint8_t p = static_cast<uint8_t>((val * (255 - sat)) / 255);
    const uint8_t q = static_cast<uint8_t>((val * (255 - (sat * rem) / 255)) / 255);
    const uint8_t t = static_cast<uint8_t>((val * (255 - (sat * (255 - rem)) / 255)) / 255);
    switch (region) {
        case 0: return {val, t, p};
        case 1: return {q, val, p};
        case 2: return {p, val, t};
        case 3: return {p, q, val};
        case 4: return {t, p, val};
        default: return {val, p, q};
    }
}

uint8_t sin8(uint8_t angle) {
    const float v = std::sin(angle * 6.28318530718f / 256.0f);
    return static_cast<uint8_t>(std::lround(127.5f + 127.5f * v));
}

Rgb scale(const Rgb& c, uint8_t level) {
    return {static_cast<uint8_t>(c.r * level / 255), static_cast<uint8_t>(c.g * level / 255),
            static_cast<uint8_t>(c.b * level / 255)};
}

int typewriterColumns(uint32_t elapsedMs, uint32_t msPerColumn, int totalColumns) {
    if (msPerColumn == 0) return totalColumns;
    const uint32_t cols = elapsedMs / msPerColumn;
    return cols >= static_cast<uint32_t>(totalColumns) ? totalColumns : static_cast<int>(cols);
}

int waveOffset(int x, uint32_t tMs, int amplitude) {
    const uint8_t phase = static_cast<uint8_t>(x * 12 + tMs / 6);
    // sin8 is 0..255 around 128 -> -amplitude..amplitude
    return static_cast<int>(std::lround((sin8(phase) - 127.5f) * amplitude / 127.5f));
}

////////////////////////////////////////////////////////////////////////////////
// Fireworks

Fireworks::Fireworks(int width, int height, uint32_t seed)
    : _w(width), _h(height), _rng(seed), _sinceLaunch(launchEveryMs) {
    _particles.reserve(kMaxParticles);
}

uint8_t Fireworks::level(const Particle& p) {
    if (p.rocket || p.maxLifeMs == 0) return 255;
    // Ease-out: sparks stay bright most of their life, then fade quickly.
    const uint32_t f = 255u * p.lifeMs / p.maxLifeMs;  // 255 -> 0
    const uint32_t inv = 255u - f;
    return static_cast<uint8_t>(255u - inv * inv / 255u);
}

void Fireworks::launch() {
    if (_particles.size() >= kMaxParticles) return;
    Particle r;
    r.x = static_cast<float>(_rng.range(_w / 6, _w - 1 - _w / 6));
    r.y = static_cast<float>(_h - 1);
    r.vx = (_rng.unit() - 0.5f) * 8.0f;
    // Burst roughly in the upper half: v^2 = 2 * g * height
    r.vy = -(std::sqrt(2.0f * 60.0f * (_h * (0.45f + 0.3f * _rng.unit()))));
    r.hue = static_cast<uint8_t>(_rng.next());
    r.lifeMs = 3000;
    r.maxLifeMs = 3000;
    r.rocket = true;
    _particles.push_back(r);
}

void Fireworks::burst(const Particle& rocket) {
    const int sparks = _rng.range(24, 40);
    for (int i = 0; i < sparks && _particles.size() < kMaxParticles; ++i) {
        const float angle = _rng.unit() * 6.28318530718f;
        const float speed = 8.0f + _rng.unit() * 22.0f;
        Particle s;
        s.x = rocket.x;
        s.y = rocket.y;
        s.vx = std::cos(angle) * speed;
        s.vy = std::sin(angle) * speed;
        s.hue = static_cast<uint8_t>(rocket.hue + _rng.range(-12, 12));
        s.maxLifeMs = static_cast<uint16_t>(_rng.range(700, 1300));
        s.lifeMs = s.maxLifeMs;
        s.rocket = false;
        _particles.push_back(s);
    }
}

void Fireworks::step(uint32_t dtMs) {
    if (dtMs > 100) dtMs = 100;  // avoid huge jumps after a stall
    const float dt = dtMs / 1000.0f;
    const float gravity = 60.0f;  // pixels / s^2

    _sinceLaunch += dtMs;
    if (_sinceLaunch >= launchEveryMs) {
        _sinceLaunch = 0;
        launch();
    }

    std::vector<Particle> bursts;
    size_t keep = 0;
    for (size_t i = 0; i < _particles.size(); ++i) {
        Particle p = _particles[i];
        p.vy += gravity * dt;
        if (!p.rocket) {
            p.vx *= 0.98f;  // air drag
        }
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.lifeMs = p.lifeMs > dtMs ? static_cast<uint16_t>(p.lifeMs - dtMs) : 0;

        if (p.rocket && p.vy >= 0.0f) {
            bursts.push_back(p);  // apex reached
            continue;
        }
        const bool offscreen = p.y > _h + 2 || p.x < -4 || p.x > _w + 4;
        if (p.lifeMs == 0 || offscreen) {
            continue;
        }
        _particles[keep++] = p;
    }
    _particles.resize(keep);
    for (const auto& r : bursts) {
        burst(r);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Starfield

Starfield::Starfield(int width, int height, size_t count, uint32_t seed)
    : _w(width), _h(height), _rng(seed) {
    _stars.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        Star s;
        s.x = static_cast<float>(_rng.range(0, _w - 1));
        s.y = _rng.range(0, _h - 1);
        s.speed = static_cast<uint8_t>(_rng.range(1, 3));
        _stars.push_back(s);
    }
}

void Starfield::step(uint32_t dtMs) {
    if (dtMs > 100) dtMs = 100;
    for (auto& s : _stars) {
        s.x -= s.speed * 12.0f * dtMs / 1000.0f;
        if (s.x < 0) {
            s.x += _w;
            s.y = _rng.range(0, _h - 1);
            s.speed = static_cast<uint8_t>(_rng.range(1, 3));
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Matrix rain

MatrixRain::MatrixRain(int width, int height, uint32_t seed) : _w(width), _h(height), _rng(seed) {
    _drops.resize(static_cast<size_t>(_w));
    for (auto& d : _drops) {
        respawn(d, true);
    }
}

void MatrixRain::respawn(Drop& d, bool anywhere) {
    d.trail = static_cast<uint8_t>(_rng.range(4, 14));
    d.speed = 10.0f + _rng.unit() * 30.0f;
    d.head = anywhere ? static_cast<float>(_rng.range(-_h, _h)) : -static_cast<float>(_rng.range(0, _h));
}

void MatrixRain::step(uint32_t dtMs) {
    if (dtMs > 100) dtMs = 100;
    for (auto& d : _drops) {
        d.head += d.speed * dtMs / 1000.0f;
        if (d.head - d.trail > _h) {
            respawn(d, false);
        }
    }
}

uint8_t MatrixRain::level(int x, int y) const {
    if (x < 0 || x >= _w) return 0;
    const Drop& d = _drops[static_cast<size_t>(x)];
    const int head = static_cast<int>(d.head);
    const int dist = head - y;  // 0 at the head, growing up the trail
    if (dist < 0 || dist >= d.trail) return 0;
    return static_cast<uint8_t>(255 - dist * 230 / d.trail);
}

}  // namespace dmd

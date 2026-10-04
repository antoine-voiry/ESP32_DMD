#include "FxRender.h"

namespace dmd {

FxBackgroundRenderer::FxBackgroundRenderer(FxBackground kind, int width, int height, uint32_t seed) : _kind(kind) {
    switch (kind) {
        case FxBackground::Fireworks:
            _fireworks.reset(new Fireworks(width, height, seed));
            _trail.reset(new Canvas(width, height));
            break;
        case FxBackground::Starfield:
            _stars.reset(new Starfield(width, height, 40, seed));
            break;
        case FxBackground::MatrixRain:
            _rain.reset(new MatrixRain(width, height, seed));
            break;
        default:
            break;
    }
}

void FxBackgroundRenderer::render(Canvas& c, uint32_t elapsedMs, uint32_t dtMs, uint8_t dim) {
    switch (_kind) {
        case FxBackground::Plasma: {
            const uint8_t t = static_cast<uint8_t>(elapsedMs / 16);
            for (int y = 0; y < c.h; ++y) {
                for (int x = 0; x < c.w; ++x) {
                    const uint8_t v = static_cast<uint8_t>((sin8(static_cast<uint8_t>(x * 8 + t)) +
                                                            sin8(static_cast<uint8_t>(y * 11 - t * 2)) +
                                                            sin8(static_cast<uint8_t>((x + y) * 6 + t))) / 3);
                    const Rgb col = hsv(static_cast<uint8_t>(v + t), 255, dim);
                    c.set(x, y, rgb565(col.r, col.g, col.b));
                }
            }
            break;
        }
        case FxBackground::Fireworks: {
            _fireworks->step(dtMs);
            // Each frame keeps ~70 % of the previous one: sparks leave short glowing trails.
            _trail->fade(180);
            for (const auto& p : _fireworks->particles()) {
                const uint8_t level = static_cast<uint8_t>(Fireworks::level(p) * dim / 255);
                const Rgb col = p.rocket ? Rgb(level, level, level / 2) : hsv(p.hue, 220, level);
                _trail->set(static_cast<int>(p.x), static_cast<int>(p.y), rgb565(col.r, col.g, col.b));
            }
            c.blit(*_trail, 0, 0, true, 0);
            break;
        }
        case FxBackground::Starfield: {
            _stars->step(dtMs);
            for (const auto& s : _stars->stars()) {
                const uint8_t level = static_cast<uint8_t>((60 + s.speed * 65) * dim / 255);
                c.set(static_cast<int>(s.x), s.y, rgb565(level, level, level));
            }
            break;
        }
        case FxBackground::MatrixRain: {
            _rain->step(dtMs);
            for (int y = 0; y < c.h; ++y) {
                for (int x = 0; x < c.w; ++x) {
                    const uint8_t level = _rain->level(x, y);
                    if (level == 0) continue;
                    const uint8_t g = static_cast<uint8_t>(level * dim / 255);
                    // The head is nearly white, the trail green.
                    const uint8_t rb = level > 240 ? static_cast<uint8_t>(180 * dim / 255) : 0;
                    c.set(x, y, rgb565(rb, g, rb));
                }
            }
            break;
        }
        case FxBackground::None:
        default:
            break;
    }
}

}  // namespace dmd

#ifndef DMD_CORE_FX_RENDER_H
#define DMD_CORE_FX_RENDER_H

// Draws the fx backgrounds (plasma, fireworks, stars, matrix rain) into a Canvas. Shared by the
// firmware (FxScene) and the host preview tool, so the README previews are the real thing.

#include <cstdint>
#include <memory>

#include "Canvas.h"
#include "Fx.h"

namespace dmd {

class FxBackgroundRenderer {
public:
    FxBackgroundRenderer(FxBackground kind, int width, int height, uint32_t seed);

    // Advances the animation by dtMs and draws frame `elapsedMs` (dim 0..255 scales brightness,
    // e.g. behind text). Does not clear the canvas first.
    void render(Canvas& canvas, uint32_t elapsedMs, uint32_t dtMs, uint8_t dim = 255);

    FxBackground kind() const { return _kind; }

private:
    FxBackground _kind;
    std::unique_ptr<Fireworks> _fireworks;
    std::unique_ptr<Starfield> _stars;
    std::unique_ptr<MatrixRain> _rain;
    std::unique_ptr<Canvas> _trail;  // fireworks: last frames fading out
};

}  // namespace dmd

#endif

#ifndef DMD_RENDER_FX_SCENE_H
#define DMD_RENDER_FX_SCENE_H

// Animated effects (ESP32 extension, not in the original Raspy2DMD): a background animation
// (plasma, fireworks, stars, matrix rain) with optional text on top, coloured or revealed
// by a text effect (rainbow, wave, typewriter, sparkle). Runs for a fixed duration.

#include <memory>
#include <string>

#include "core/Fx.h"
#include "core/FxRender.h"
#include "render/Bitmap.h"
#include "core/SceneRunner.h"
#include "core/TextUtil.h"
#include "matrix/Hub75_Matrix.h"

class GFXcanvas1;

struct FxSpec {
    dmd::FxBackground background = dmd::FxBackground::None;
    dmd::FxText textFx = dmd::FxText::Solid;
    std::string text;         // empty: background only
    dmd::Rgb fg{0, 0, 255};   // used by Solid, Wave, Typewriter and Sparkle
    uint32_t durationMs = 4000;
    size_t maxCharsPerLine = 22;
    int maxFontPx = 32;
};

class FxScene : public dmd::Scene {
public:
    FxScene(Hub75_Matrix& matrix, FxSpec spec);
    ~FxScene() override;

    void start(uint32_t nowMs) override;
    bool tick(uint32_t nowMs) override;
    void abort() override;

private:
    void drawFrame(uint32_t elapsedMs, uint32_t dtMs);
    void drawText(uint32_t elapsedMs);
    bool textPixel(int x, int y) const;

    Hub75_Matrix& _matrix;
    FxSpec _spec;
    std::unique_ptr<GFXcanvas1> _image;
    std::unique_ptr<dmd::FxBackgroundRenderer> _background;
    std::unique_ptr<Bitmap565> _frame;
    dmd::Rng _rng;
    uint32_t _startMs = 0;
    uint32_t _lastFrameMs = 0;
};

#endif

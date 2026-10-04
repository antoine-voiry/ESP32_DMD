#ifndef DMD_RENDERER_H
#define DMD_RENDERER_H

// ESP32 counterpart of Raspy2DMD bin/DMDRenderer.py. Rendering is non-blocking: every call queues
// a scene on the SceneRunner, and update() (called from loop()) advances the current animation.

#include <memory>
#include <string>

#include "core/Motion.h"
#include "core/SceneRunner.h"
#include "core/TextUtil.h"
#include "matrix/Hub75_Matrix.h"
#include "render/ClockScene.h"
#include "render/FxScene.h"
#include "render/MediaScenes.h"
#include "render/TextScene.h"

class MediaLibrary;

struct TextRequest {
    std::string text;
    dmd::Motion motion = dmd::Motion::None;
    int iterations = 1;
    bool hasFg = false;
    dmd::Rgb fg;
    bool hasBg = false;
    dmd::Rgb bg;
};

class DMDRenderer {
public:
    explicit DMDRenderer(Hub75_Matrix* matrix);
    DMDRenderer() = delete;

    // Advance the current animation. Call on every loop().
    void update();

    // Port of Stop(): abort the animation on screen because a newer message arrived.
    void interrupt();

    // Port of RenderText(). holdMs is the "|N" trailing argument (N seconds) of the original.
    void renderText(const TextRequest& request, uint32_t holdMs = 0);
    void renderText(const std::string& text, uint32_t holdMs = 0);

    // Port of RenderText(val=True): special move name (if any) for 2 s, then the darts.
    void renderScore(const std::string& score, uint32_t holdMs = 0);

    // Animated effect (ESP32 extension): fx|... and msgfx|... commands, score celebrations.
    void renderFx(FxSpec spec, uint32_t holdMs = 0);

    // Port of RenderGif() / RenderGifWithText(): plays an already resolved GIF once, optional text on top.
    void renderGif(const std::string& path, uint32_t holdMs = 0, const std::string& text = "");
    // Port of RenderImage() / msgimg: shows an already resolved PNG, optional text on top.
    void renderImage(const std::string& path, uint32_t holdMs = 0, const std::string& text = "");

    // Score and special-move animations come from here (optional).
    void setMediaLibrary(MediaLibrary* media) { _media = media; }
    void setCenterImages(bool center) { _centerImages = center; }

    // Any other scene (weather, Tempo, perf...).
    void renderScene(std::unique_ptr<dmd::Scene> scene, uint32_t holdMs = 0);
    Hub75_Matrix& matrix() { return *_dmd; }

    // Port of RunTime(): date and/or time for the configured durations.
    void renderClock(ClockSpec spec, uint32_t holdMs = 0);

    // Status line shown by the firmware itself (MQTT down, ...), replaces whatever is queued.
    void renderStatus(const std::string& text);

    void clear();
    void setBrightnessPercent(int percent);

    // TextRenderer defaults ([TextRenderer] section of Raspy2DMD.cfg).
    TextStyle& defaultStyle() { return _defaults; }

    bool idle() const;

private:
    Hub75_Matrix* _dmd;
    dmd::SceneRunner _runner;
    TextStyle _defaults;
    bool _centerImages = true;
    MediaLibrary* _media = nullptr;
};

#endif

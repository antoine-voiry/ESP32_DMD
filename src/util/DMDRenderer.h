#ifndef DMD_RENDERER_H
#define DMD_RENDERER_H

// ESP32 counterpart of Raspy2DMD bin/DMDRenderer.py. Rendering is non-blocking: every call queues
// a scene on the SceneRunner, and update() (called from loop()) advances the current animation.

#include <string>

#include "core/Motion.h"
#include "core/SceneRunner.h"
#include "core/TextUtil.h"
#include "matrix/Hub75_Matrix.h"
#include "render/FxScene.h"
#include "render/TextScene.h"

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

    // Celebrate special moves with an animation instead of plain text (default on).
    void setCelebrations(bool enabled) { _celebrations = enabled; }

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
    bool _celebrations = true;
};

#endif

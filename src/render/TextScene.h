#ifndef DMD_RENDER_TEXT_SCENE_H
#define DMD_RENDER_TEXT_SCENE_H

// Port of DMDRenderer.RenderText(): fits the text to the panel with the largest font that fits,
// renders it once into a 1-bit image, then plays the requested movement frame by frame.

#include <memory>
#include <string>

#include "core/Motion.h"
#include "core/SceneRunner.h"
#include "core/TextUtil.h"
#include "matrix/Hub75_Matrix.h"

class GFXcanvas1;

struct TextStyle {
    dmd::Rgb fg{0, 0, 255};     // TextRenderer.defaultfontcolor
    dmd::Rgb bg{0, 0, 0};       // TextRenderer.picturebackgroundcolor
    dmd::Motion motion = dmd::Motion::None;
    int iterations = 1;
    size_t maxCharsPerLine = 22;  // TextRenderer.maxcharacter
    int maxFontPx = 32;           // TextRenderer.maxfontsize
};

class TextScene : public dmd::Scene {
public:
    TextScene(Hub75_Matrix& matrix, std::string text, TextStyle style);
    ~TextScene() override;

    void start(uint32_t nowMs) override;
    bool tick(uint32_t nowMs) override;
    void abort() override;

private:
    void render();
    void drawFrame(const dmd::Frame& frame);

    Hub75_Matrix& _matrix;
    std::string _text;
    TextStyle _style;
    std::unique_ptr<GFXcanvas1> _image;
    std::unique_ptr<dmd::Timeline> _timeline;
    size_t _frame = 0;
    uint32_t _nextFrameAt = 0;
};

#endif

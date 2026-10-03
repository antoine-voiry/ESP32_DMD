#ifndef DMD_RENDER_MEDIA_SCENES_H
#define DMD_RENDER_MEDIA_SCENES_H

// Ports of RenderGif() / RenderGifWithText() (GifScene) and RenderImage() / msgimg (ImageScene).

#include <memory>
#include <string>

#include "Bitmap.h"
#include "core/SceneRunner.h"
#include "core/TextUtil.h"
#include "matrix/Hub75_Matrix.h"

class AnimatedGIF;
class GFXcanvas1;

struct MediaOverlay {
    std::string text;  // empty: no text
    dmd::Rgb color{0, 0, 255};
    size_t maxCharsPerLine = 22;
    int maxFontPx = 32;
};

// Plays a GIF once, frame by frame, scaled to fit the panel (center_images).
class GifScene : public dmd::Scene {
public:
    GifScene(Hub75_Matrix& matrix, std::string path, bool center, MediaOverlay overlay = MediaOverlay());
    ~GifScene() override;

    void start(uint32_t nowMs) override;
    bool tick(uint32_t nowMs) override;
    void abort() override;

private:
    bool decodeFrame();  // false after the last frame or on error
    void close();

    Hub75_Matrix& _matrix;
    std::string _path;
    bool _center;
    MediaOverlay _overlay;
    std::unique_ptr<AnimatedGIF> _gif;
    std::unique_ptr<Bitmap565> _frame;
    std::unique_ptr<GFXcanvas1> _text;
    bool _lastFrame = false;
    uint32_t _nextFrameAt = 0;
    int _frameDelayMs = 0;

    friend struct GifCallbacks;
};

// Shows a PNG (optionally with text on top) and stays on screen.
class ImageScene : public dmd::Scene {
public:
    ImageScene(Hub75_Matrix& matrix, std::string path, bool center, MediaOverlay overlay = MediaOverlay());
    ~ImageScene() override;

    void start(uint32_t nowMs) override;
    bool tick(uint32_t nowMs) override { (void)nowMs; return false; }
    void abort() override;

private:
    Hub75_Matrix& _matrix;
    std::string _path;
    bool _center;
    MediaOverlay _overlay;
};

#endif

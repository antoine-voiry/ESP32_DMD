#include "TextScene.h"

#include <Adafruit_GFX.h>
#include <esp_log.h>

#include <algorithm>
#include <cmath>

#include "TextImage.h"

static const char* TAG = "TextScene";

namespace {

// After a stall longer than this, resume the animation instead of fast-forwarding through it.
constexpr int32_t kMaxCatchUpMs = 250;

bool reached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

}  // namespace

TextScene::TextScene(Hub75_Matrix& matrix, std::string text, TextStyle style)
    : _matrix(matrix), _text(dmd::toDisplayAscii(text)), _style(style) {}

TextScene::~TextScene() = default;

void TextScene::render() {
    const int dispW = _matrix.width();
    const int dispH = _matrix.height();
    TextLayoutOptions options;
    options.singleLine = dmd::isHorizontalScroll(_style.motion);
    options.maxCharsPerLine = _style.maxCharsPerLine;
    options.maxFontPx = _style.maxFontPx;
    std::string text = _text;
    if (options.singleLine) {
        // The original pads with a space on the side the text enters from.
        text = _style.motion == dmd::Motion::Right ? " " + _text : _text + " ";
    }
    _image = renderTextImage(text, dispW, dispH, options);
    if (!_image) {
        ESP_LOGE(TAG, "No image for '%s'", _text.c_str());
        return;
    }
    _timeline.reset(new dmd::Timeline(_style.motion, _image->width(), _image->height(), dispW, dispH,
                                      _style.iterations));
}

void TextScene::drawFrame(const dmd::Frame& f) {
    const int dispW = _matrix.width();
    const int dispH = _matrix.height();
    const uint16_t fg = _matrix.color(_style.fg.r, _style.fg.g, _style.fg.b);
    const uint16_t bg = _matrix.color(_style.bg.r, _style.bg.g, _style.bg.b);

    _matrix.fillScreen(bg);
    if (_image && f.w > 0 && f.h > 0) {
        const int imgW = _image->width();
        const int imgH = _image->height();
        const int angle = ((f.angle % 360) + 360) % 360;
        if (angle == 0) {
            // Scaled (flip/twirl) or translated (scroll) copy, clipped to the panel.
            const int dx0 = f.x < 0 ? -f.x : 0;
            const int dy0 = f.y < 0 ? -f.y : 0;
            const int dx1 = std::min<int>(f.w, dispW - f.x);
            const int dy1 = std::min<int>(f.h, dispH - f.y);
            for (int dy = dy0; dy < dy1; ++dy) {
                int sy = dy * imgH / f.h;
                if (f.flipY) sy = imgH - 1 - sy;
                for (int dx = dx0; dx < dx1; ++dx) {
                    int sx = dx * imgW / f.w;
                    if (f.flipX) sx = imgW - 1 - sx;
                    if (_image->getPixel(sx, sy)) {
                        _matrix.drawPixel(f.x + dx, f.y + dy, fg);
                    }
                }
            }
        } else {
            // Rotation around the image centre, counter-clockwise like PIL's Image.rotate().
            const float rad = angle * 3.14159265f / 180.0f;
            const float c = std::cos(rad);
            const float s = std::sin(rad);
            const float cx = (imgW - 1) / 2.0f;
            const float cy = (imgH - 1) / 2.0f;
            for (int py = 0; py < dispH; ++py) {
                for (int px = 0; px < dispW; ++px) {
                    const float rx = (px - f.x) - cx;
                    const float ry = (py - f.y) - cy;
                    const int sx = static_cast<int>(std::lround(c * rx - s * ry + cx));
                    const int sy = static_cast<int>(std::lround(s * rx + c * ry + cy));
                    if (sx >= 0 && sy >= 0 && sx < imgW && sy < imgH && _image->getPixel(sx, sy)) {
                        _matrix.drawPixel(px, py, fg);
                    }
                }
            }
        }
    }
    _matrix.present();
}

void TextScene::start(uint32_t nowMs) {
    render();
    _frame = 0;
    if (!_timeline) {
        _matrix.fillScreen(_matrix.color(_style.bg.r, _style.bg.g, _style.bg.b));
        _matrix.present();
        return;
    }
    const dmd::Frame first = _timeline->at(0);
    drawFrame(first);
    _nextFrameAt = nowMs + first.delayMs;
}

bool TextScene::tick(uint32_t nowMs) {
    if (!_timeline || _timeline->size() <= 1) {
        return false;  // static text: drawn once in start(), stays on screen
    }
    if (!reached(nowMs, _nextFrameAt)) {
        return true;
    }
    if (static_cast<int32_t>(nowMs - _nextFrameAt) > kMaxCatchUpMs) {
        _nextFrameAt = nowMs;
    }
    while (reached(nowMs, _nextFrameAt)) {
        ++_frame;
        if (_frame >= _timeline->size()) {
            return false;
        }
        const uint16_t delay = _timeline->at(_frame).delayMs;
        _nextFrameAt += delay > 0 ? delay : 1;
    }
    drawFrame(_timeline->at(_frame));
    return true;
}

void TextScene::abort() {
    // The original clears the panel when an animation is stopped.
    _matrix.clearScreen();
}

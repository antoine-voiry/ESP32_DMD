#include "FxScene.h"

#include <Adafruit_GFX.h>
#include <esp_log.h>
#include <esp_random.h>

#include <utility>

#include "TextImage.h"

static const char* TAG = "FxScene";

namespace {

constexpr uint32_t kFrameMs = 33;            // ~30 fps
constexpr uint32_t kTypewriterColumnMs = 18; // reveal speed
constexpr uint8_t kBackgroundDimText = 90;   // background brightness behind text (0..255)

}  // namespace

FxScene::FxScene(Hub75_Matrix& matrix, FxSpec spec)
    : _matrix(matrix), _spec(std::move(spec)), _rng(esp_random()) {
    _spec.text = dmd::toDisplayAscii(_spec.text);
}

FxScene::~FxScene() = default;

void FxScene::start(uint32_t nowMs) {
    const int w = _matrix.width();
    const int h = _matrix.height();
    if (!_spec.text.empty()) {
        TextLayoutOptions options;
        options.maxCharsPerLine = _spec.maxCharsPerLine;
        options.maxFontPx = _spec.maxFontPx;
        _image = renderTextImage(_spec.text, w, h, options);
        if (!_image) {
            ESP_LOGE(TAG, "No text image, showing the background only");
        }
    }
    _background.reset(new dmd::FxBackgroundRenderer(_spec.background, w, h, esp_random()));
    _frame.reset(new Bitmap565(w, h));
    _startMs = nowMs;
    _lastFrameMs = nowMs;
    drawFrame(0, 0);
}

bool FxScene::tick(uint32_t nowMs) {
    const uint32_t elapsed = nowMs - _startMs;
    if (elapsed >= _spec.durationMs) {
        // Leave the text on screen, without the animated background.
        if (_image) {
            _matrix.fillScreen(0);
            const uint16_t fg = _matrix.color(_spec.fg.r, _spec.fg.g, _spec.fg.b);
            for (int y = 0; y < _matrix.height(); ++y) {
                for (int x = 0; x < _matrix.width(); ++x) {
                    if (textPixel(x, y)) _matrix.drawPixel(x, y, fg);
                }
            }
            _matrix.present();
        }
        return false;
    }
    const uint32_t dt = nowMs - _lastFrameMs;
    if (dt >= kFrameMs) {
        _lastFrameMs = nowMs;
        drawFrame(elapsed, dt);
    }
    return true;
}

void FxScene::abort() {
    _matrix.clearScreen();
}

bool FxScene::textPixel(int x, int y) const {
    if (!_image || x < 0 || y < 0 || x >= _image->width() || y >= _image->height()) {
        return false;
    }
    return _image->getPixel(x, y);
}


void FxScene::drawText(uint32_t elapsedMs) {
    if (!_image) {
        return;
    }
    const int w = _matrix.width();
    const int h = _matrix.height();
    const bool overBackground = _spec.background != dmd::FxBackground::None;
    const int revealed = _spec.textFx == dmd::FxText::Typewriter
                             ? dmd::typewriterColumns(elapsedMs, kTypewriterColumnMs, w)
                             : w;
    const uint16_t fg = _matrix.color(_spec.fg.r, _spec.fg.g, _spec.fg.b);
    const uint16_t black = 0;

    for (int x = 0; x < revealed; ++x) {
        const int dy = _spec.textFx == dmd::FxText::Wave ? dmd::waveOffset(x, elapsedMs) : 0;
        for (int y = 0; y < h; ++y) {
            const int sy = y - dy;
            if (!textPixel(x, sy)) {
                // 1-pixel black outline keeps the text readable over a busy background.
                if (overBackground && (textPixel(x - 1, sy) || textPixel(x + 1, sy) ||
                                       textPixel(x, sy - 1) || textPixel(x, sy + 1))) {
                    _matrix.drawPixel(x, y, black);
                }
                continue;
            }
            uint16_t color = fg;
            switch (_spec.textFx) {
                case dmd::FxText::Rainbow: {
                    const dmd::Rgb c = dmd::hsv(static_cast<uint8_t>(x * 4 + elapsedMs / 8));
                    color = _matrix.color(c.r, c.g, c.b);
                    break;
                }
                case dmd::FxText::Sparkle:
                    if (_rng.range(0, 99) < 6) color = _matrix.color(255, 255, 255);
                    break;
                default:
                    break;
            }
            _matrix.drawPixel(x, y, color);
        }
    }
    // Typewriter cursor.
    if (_spec.textFx == dmd::FxText::Typewriter && revealed < w && (elapsedMs / 250) % 2 == 0) {
        for (int y = h / 4; y < h - h / 4; ++y) {
            _matrix.drawPixel(revealed, y, fg);
        }
    }
}

void FxScene::drawFrame(uint32_t elapsedMs, uint32_t dtMs) {
    // Background into the frame buffer (shared with the host preview tool), then text on top.
    _frame->clear();
    _background->render(*_frame, elapsedMs, dtMs, _image ? kBackgroundDimText : 255);
    for (int y = 0; y < _frame->h; ++y) {
        for (int x = 0; x < _frame->w; ++x) {
            _matrix.drawPixel(x, y, _frame->get(x, y));
        }
    }
    drawText(elapsedMs);
    _matrix.present();
}

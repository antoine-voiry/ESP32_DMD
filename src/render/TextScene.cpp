#include "TextScene.h"

#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <esp_log.h>

#include <algorithm>
#include <cmath>
#include <vector>

static const char* TAG = "TextScene";

namespace {

// Largest first. The original shrinks a TrueType font (Impact by default) until the text fits;
// here we pick the first bitmap font that fits. nullptr is the built-in 6x8 font, scaled by size.
struct FontChoice {
    const GFXfont* font;
    uint8_t size;
};

const FontChoice kFonts[] = {
    {&FreeSansBold18pt7b, 1},
    {&FreeSansBold12pt7b, 1},
    {&FreeSansBold9pt7b, 1},
    {nullptr, 2},
    {nullptr, 1},
};
constexpr size_t kFontCount = sizeof(kFonts) / sizeof(kFonts[0]);

// Widest text image we allocate for left/right scrolls (1 bit per pixel: 4096x32 = 16 KB).
constexpr int kMaxScrollWidth = 4096;
// After a stall longer than this, resume the animation instead of fast-forwarding through it.
constexpr int32_t kMaxCatchUpMs = 250;

struct LineMetrics {
    int height;  // pixel height of a line of text
    int top;     // y1 of the text bounds relative to the cursor (negative for baseline fonts)
};

// Small canvas used only to measure text; getTextBounds does not touch the pixel buffer.
GFXcanvas1& measurer() {
    static GFXcanvas1 canvas(8, 8);
    return canvas;
}

void applyFont(Adafruit_GFX& gfx, const FontChoice& choice) {
    gfx.setFont(choice.font);
    gfx.setTextSize(choice.size);
    gfx.setTextWrap(false);
}

int textWidth(const FontChoice& choice, const std::string& s) {
    if (s.empty()) {
        return 0;
    }
    GFXcanvas1& m = measurer();
    applyFont(m, choice);
    int16_t x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;
    m.getTextBounds(s.c_str(), 0, 0, &x1, &y1, &w, &h);
    return static_cast<int>(w);
}

int textLeft(const FontChoice& choice, const std::string& s) {
    GFXcanvas1& m = measurer();
    applyFont(m, choice);
    int16_t x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;
    m.getTextBounds(s.c_str(), 0, 0, &x1, &y1, &w, &h);
    return x1;
}

LineMetrics lineMetrics(const FontChoice& choice) {
    GFXcanvas1& m = measurer();
    applyFont(m, choice);
    int16_t x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;
    m.getTextBounds("AQgjy", 0, 0, &x1, &y1, &w, &h);
    return {static_cast<int>(h), static_cast<int>(y1)};
}

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
    const bool horizontal = dmd::isHorizontalScroll(_style.motion);

    // Pick the font: the largest one whose lines fit the panel.
    size_t fontIndex = kFontCount - 1;
    std::vector<std::string> lines;
    std::string single = _text;
    if (horizontal) {
        // The original pads with a space on the side the text enters from.
        single = _style.motion == dmd::Motion::Right ? " " + _text : _text + " ";
    }
    for (size_t i = 0; i < kFontCount; ++i) {
        const LineMetrics lm = lineMetrics(kFonts[i]);
        const bool last = i + 1 == kFontCount;
        if (lm.height > _style.maxFontPx && !last) {
            continue;
        }
        if (horizontal) {
            if (lm.height <= dispH || last) {
                fontIndex = i;
                break;
            }
            continue;
        }
        auto measure = [&](const std::string& s) { return textWidth(kFonts[i], s); };
        std::vector<std::string> candidate = dmd::wrapText(_text, dispW, _style.maxCharsPerLine, measure);
        if (static_cast<int>(candidate.size()) * lm.height <= dispH || last) {
            fontIndex = i;
            lines = candidate;
            break;
        }
    }
    const FontChoice& font = kFonts[fontIndex];
    const LineMetrics lm = lineMetrics(font);

    int imgW = dispW;
    if (horizontal) {
        lines = {single};
        imgW = textWidth(font, single);
        if (imgW < 1) imgW = 1;
        if (imgW > kMaxScrollWidth) {
            ESP_LOGW(TAG, "Text too long to scroll (%d px), truncating to %d px", imgW, kMaxScrollWidth);
            imgW = kMaxScrollWidth;
        }
    }
    const int imgH = dispH;

    _image.reset(new GFXcanvas1(imgW, imgH));
    if (!_image->getBuffer()) {
        ESP_LOGE(TAG, "Out of memory for a %dx%d text image", imgW, imgH);
        _image.reset();
        return;
    }
    _image->fillScreen(0);
    applyFont(*_image, font);
    _image->setTextColor(1);

    // Vertically centre the block of lines; centre each line horizontally (left-align scrolls).
    int y = (imgH - static_cast<int>(lines.size()) * lm.height) / 2;
    for (const auto& line : lines) {
        const int w = textWidth(font, line);
        const int x = horizontal ? 0 : (imgW - w) / 2;
        _image->setCursor(x - textLeft(font, line), y - lm.top);
        _image->print(line.c_str());
        y += lm.height;
    }
    ESP_LOGD(TAG, "Rendered '%s' in font %u, %u line(s), image %dx%d", _text.c_str(),
             static_cast<unsigned>(fontIndex), static_cast<unsigned>(lines.size()), imgW, imgH);

    _timeline.reset(new dmd::Timeline(_style.motion, imgW, imgH, dispW, dispH, _style.iterations));
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

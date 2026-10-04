#include "TextImage.h"

#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <esp_log.h>

#include <vector>

#include "core/TextUtil.h"

static const char* TAG = "TextImage";

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

struct Bounds {
    int x1;
    int y1;
    int w;
    int h;
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

Bounds measure(const FontChoice& choice, const std::string& s) {
    if (s.empty()) {
        return {0, 0, 0, 0};
    }
    GFXcanvas1& m = measurer();
    applyFont(m, choice);
    int16_t x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;
    m.getTextBounds(s.c_str(), 0, 0, &x1, &y1, &w, &h);
    return {x1, y1, w, h};
}

// Height of a line and its top relative to the cursor (negative for baseline fonts).
Bounds lineMetrics(const FontChoice& choice) {
    return measure(choice, "AQgjy");
}

}  // namespace

std::unique_ptr<GFXcanvas1> renderTextImage(const std::string& text, int dispW, int dispH,
                                            const TextLayoutOptions& options) {
    size_t fontIndex = kFontCount - 1;
    std::vector<std::string> lines;
    for (size_t i = 0; i < kFontCount; ++i) {
        const int lineH = lineMetrics(kFonts[i]).h;
        const bool last = i + 1 == kFontCount;
        if (lineH > options.maxFontPx && !last) {
            continue;
        }
        if (options.singleLine) {
            if (lineH <= dispH || last) {
                fontIndex = i;
                break;
            }
            continue;
        }
        auto width = [&](const std::string& s) { return measure(kFonts[i], s).w; };
        std::vector<std::string> candidate = dmd::wrapText(text, dispW, options.maxCharsPerLine, width);
        if (static_cast<int>(candidate.size()) * lineH <= dispH || last) {
            fontIndex = i;
            lines = candidate;
            break;
        }
    }
    const FontChoice& font = kFonts[fontIndex];
    const Bounds line = lineMetrics(font);

    int imgW = dispW;
    if (options.singleLine) {
        lines = {text};
        imgW = measure(font, text).w;
        if (imgW < 1) imgW = 1;
        if (imgW > kMaxScrollWidth) {
            ESP_LOGW(TAG, "Text too long to scroll (%d px), truncating to %d px", imgW, kMaxScrollWidth);
            imgW = kMaxScrollWidth;
        }
    }

    std::unique_ptr<GFXcanvas1> image(new GFXcanvas1(imgW, dispH));
    if (!image->getBuffer()) {
        ESP_LOGE(TAG, "Out of memory for a %dx%d text image", imgW, dispH);
        return nullptr;
    }
    image->fillScreen(0);
    applyFont(*image, font);
    image->setTextColor(1);

    // Vertically centre the block of lines; centre each line horizontally (left-align scrolls).
    int y = (dispH - static_cast<int>(lines.size()) * line.h) / 2;
    for (const auto& l : lines) {
        const Bounds b = measure(font, l);
        const int x = options.singleLine ? 0 : (imgW - b.w) / 2;
        image->setCursor(x - b.x1, y - line.y1);
        image->print(l.c_str());
        y += line.h;
    }
    ESP_LOGD(TAG, "Rendered '%s' in font %u, %u line(s), image %dx%d", text.c_str(),
             static_cast<unsigned>(fontIndex), static_cast<unsigned>(lines.size()), imgW, dispH);
    return image;
}

#include "Bitmap.h"

#include <Adafruit_GFX.h>
#include <PNGdec.h>
#include <esp_log.h>

#include "TextImage.h"
#include "core/Media.h"
#include "util/Storage.h"

static const char* TAG = "Bitmap";

namespace {

// Uploaded PNGs are meant to be panel-sized; this bounds the memory a decode can take.
constexpr int kMaxPngSide = 1024;

struct PngContext {
    PNG* png;
    Bitmap565* bitmap;
    dmd::FitRect rect;
    int srcW;
    int srcH;
    std::vector<uint16_t> line;
};

void* pngOpen(const char* path, int32_t* size) {
    File* f = new File(storage().open(path, "r"));
    if (!*f) {
        delete f;
        return nullptr;
    }
    *size = static_cast<int32_t>(f->size());
    return f;
}

void pngClose(void* handle) {
    File* f = static_cast<File*>(handle);
    if (f) {
        f->close();
        delete f;
    }
}

int32_t pngRead(PNGFILE* file, uint8_t* buffer, int32_t length) {
    return static_cast<int32_t>(static_cast<File*>(file->fHandle)->read(buffer, length));
}

int32_t pngSeek(PNGFILE* file, int32_t position) {
    return static_cast<File*>(file->fHandle)->seek(position) ? position : -1;
}

int pngDraw(PNGDRAW* draw) {
    PngContext* ctx = static_cast<PngContext*>(draw->pUser);
    const dmd::FitRect& r = ctx->rect;
    if (r.w <= 0 || r.h <= 0) return 0;
    // Blend transparency with black, as the matrix background is black.
    ctx->png->getLineAsRGB565(draw, ctx->line.data(), PNG_RGB565_LITTLE_ENDIAN, 0x00000000);
    int first = 0, count = 0;
    dmd::destRows(draw->y, ctx->srcH, r.h, first, count);
    for (int dy = first; dy < first + count; ++dy) {
        for (int dx = 0; dx < r.w; ++dx) {
            const int sx = dmd::srcIndex(dx, ctx->srcW, r.w);
            ctx->bitmap->set(r.x + dx, r.y + dy, ctx->line[static_cast<size_t>(sx)]);
        }
    }
    return 1;
}

}  // namespace

std::shared_ptr<Bitmap565> loadPng(const std::string& path, int w, int h, bool center) {
    std::unique_ptr<PNG> png(new PNG());  // ~40 KB of decoder state: keep it off the stack
    // open() returns PNG_SUCCESS when the file cannot be opened, and keeps the file open when the
    // header is invalid: check the size and always close.
    const int opened = png->open(path.c_str(), pngOpen, pngClose, pngRead, pngSeek, pngDraw);
    const int srcW = png->getWidth();
    const int srcH = png->getHeight();
    if (opened != PNG_SUCCESS || srcW <= 0 || srcH <= 0 || srcW > kMaxPngSide || srcH > kMaxPngSide) {
        ESP_LOGW(TAG, "Cannot show %s (%dx%d, error %d)", path.c_str(), srcW, srcH, opened);
        png->close();
        return nullptr;
    }
    std::shared_ptr<Bitmap565> bitmap = std::make_shared<Bitmap565>(w, h);
    PngContext ctx;
    ctx.png = png.get();
    ctx.bitmap = bitmap.get();
    ctx.srcW = srcW;
    ctx.srcH = srcH;
    ctx.rect = dmd::fitImage(srcW, srcH, w, h, center);
    ctx.line.assign(static_cast<size_t>(srcW), 0);
    const int rc = png->decode(&ctx, 0);
    png->close();
    if (rc != PNG_SUCCESS) {
        ESP_LOGW(TAG, "PNG decode error %d for %s", rc, path.c_str());
        return nullptr;
    }
    return bitmap;
}

void drawTextBox(dmd::Canvas& canvas, const std::string& text, int x, int y, int w, int h, const dmd::Rgb& color,
                 int maxFontPx) {
    if (text.empty() || w <= 0 || h <= 0) return;
    TextLayoutOptions options;
    options.maxFontPx = maxFontPx;
    options.maxCharsPerLine = 64;
    std::unique_ptr<GFXcanvas1> image = renderTextImage(dmd::toDisplayAscii(text), w, h, options);
    if (!image) return;
    const uint16_t c = dmd::rgb565(color.r, color.g, color.b);
    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            if (image->getPixel(xx, yy)) canvas.set(x + xx, y + yy, c);
        }
    }
}

void presentFrame(Hub75_Matrix& matrix, const Bitmap565* background, const GFXcanvas1* text,
                  const dmd::Rgb& textColor) {
    const int w = matrix.width();
    const int h = matrix.height();
    if (background) {
        for (int y = 0; y < h && y < background->h; ++y) {
            for (int x = 0; x < w && x < background->w; ++x) {
                matrix.drawPixel(x, y, background->px[static_cast<size_t>(y) * background->w + x]);
            }
        }
    } else {
        matrix.fillScreen(0);
    }
    if (text) {
        auto on = [&](int x, int y) {
            return x >= 0 && y >= 0 && x < text->width() && y < text->height() && text->getPixel(x, y);
        };
        const uint16_t fg = matrix.color(textColor.r, textColor.g, textColor.b);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (on(x, y)) {
                    matrix.drawPixel(x, y, fg);
                } else if (background && (on(x - 1, y) || on(x + 1, y) || on(x, y - 1) || on(x, y + 1))) {
                    matrix.drawPixel(x, y, 0);
                }
            }
        }
    }
    matrix.present();
}

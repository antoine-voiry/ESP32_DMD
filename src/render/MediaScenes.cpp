#include "MediaScenes.h"

#include <Adafruit_GFX.h>
#include <AnimatedGIF.h>
#include <esp_log.h>

#include <utility>

#include "TextImage.h"
#include "core/Media.h"
#include "util/Storage.h"

static const char* TAG = "MediaScenes";

namespace {

// GIFs with a 0 frame delay are shown at 10 fps, as browsers do.
constexpr int kDefaultFrameDelayMs = 100;
constexpr int kMinFrameDelayMs = 20;

std::unique_ptr<GFXcanvas1> overlayImage(const MediaOverlay& overlay, int w, int h) {
    if (overlay.text.empty()) {
        return nullptr;
    }
    TextLayoutOptions options;
    options.maxCharsPerLine = overlay.maxCharsPerLine;
    options.maxFontPx = overlay.maxFontPx;
    return renderTextImage(dmd::toDisplayAscii(overlay.text), w, h, options);
}

}  // namespace

// AnimatedGIF reads through these callbacks; the scene is passed as pUser.
struct GifCallbacks {
    struct DrawContext {
        GifScene* scene;
        dmd::FitRect rect;
        int canvasW;
        int canvasH;
    };

    static void* open(const char* path, int32_t* size) {
        File* f = new File(storage().open(path, "r"));
        if (!*f) {
            delete f;
            return nullptr;
        }
        *size = static_cast<int32_t>(f->size());
        return f;
    }

    static void close(void* handle) {
        File* f = static_cast<File*>(handle);
        if (f) {
            f->close();
            delete f;
        }
    }

    static int32_t read(GIFFILE* file, uint8_t* buffer, int32_t length) {
        File* f = static_cast<File*>(file->fHandle);
        int32_t n = length;
        if (file->iSize - file->iPos < n) n = file->iSize - file->iPos;
        if (n <= 0) return 0;
        n = static_cast<int32_t>(f->read(buffer, n));
        file->iPos = static_cast<int32_t>(f->position());
        return n;
    }

    static int32_t seek(GIFFILE* file, int32_t position) {
        File* f = static_cast<File*>(file->fHandle);
        f->seek(position);
        file->iPos = static_cast<int32_t>(f->position());
        return file->iPos;
    }

    // One line of the current frame, as palette indices. Transparent pixels keep the previous
    // frame, which is why the scene keeps its own frame buffer (the panel is double buffered).
    static void draw(GIFDRAW* d) {
        DrawContext* ctx = static_cast<DrawContext*>(d->pUser);
        Bitmap565* frame = ctx->scene->_frame.get();
        const dmd::FitRect& r = ctx->rect;
        const int srcY = d->iY + d->y;
        int first = 0, count = 0;
        dmd::destRows(srcY, ctx->canvasH, r.h, first, count);
        if (count == 0) return;
        for (int dx = 0; dx < r.w; ++dx) {
            const int srcX = dmd::srcIndex(dx, ctx->canvasW, r.w) - d->iX;
            if (srcX < 0 || srcX >= d->iWidth) continue;
            const uint8_t index = d->pPixels[srcX];
            if (d->ucHasTransparency && index == d->ucTransparent) continue;
            const uint16_t color = d->pPalette[index];
            for (int dy = first; dy < first + count; ++dy) {
                frame->set(r.x + dx, r.y + dy, color);
            }
        }
    }
};

GifScene::GifScene(Hub75_Matrix& matrix, std::string path, bool center, MediaOverlay overlay)
    : _matrix(matrix), _path(std::move(path)), _center(center), _overlay(std::move(overlay)) {}

GifScene::~GifScene() {
    close();
}

void GifScene::close() {
    if (_gif) {
        _gif->close();
        _gif.reset();
    }
}

bool GifScene::decodeFrame() {
    if (!_gif) return false;
    GifCallbacks::DrawContext ctx;
    ctx.scene = this;
    ctx.canvasW = _gif->getCanvasWidth();
    ctx.canvasH = _gif->getCanvasHeight();
    ctx.rect = dmd::fitImage(ctx.canvasW, ctx.canvasH, _matrix.width(), _matrix.height(), _center);
    int delayMs = 0;
    const int rc = _gif->playFrame(false, &delayMs, &ctx);
    if (rc < 0) {
        ESP_LOGW(TAG, "GIF decode error %d in %s", _gif->getLastError(), _path.c_str());
        return false;
    }
    _frameDelayMs = delayMs <= 0 ? kDefaultFrameDelayMs : (delayMs < kMinFrameDelayMs ? kMinFrameDelayMs : delayMs);
    _lastFrame = rc == 0;
    presentFrame(_matrix, _frame.get(), _text.get(), _overlay.color);
    return true;
}

void GifScene::start(uint32_t nowMs) {
    _frame.reset(new Bitmap565(_matrix.width(), _matrix.height()));
    _text = overlayImage(_overlay, _matrix.width(), _matrix.height());
    _gif.reset(new AnimatedGIF());  // ~25 KB of decoder state: keep it off the stack
    _gif->begin(GIF_PALETTE_RGB565_LE);
    if (!_gif->open(_path.c_str(), GifCallbacks::open, GifCallbacks::close, GifCallbacks::read,
                    GifCallbacks::seek, GifCallbacks::draw)) {
        ESP_LOGW(TAG, "Cannot open GIF %s", _path.c_str());
        close();
        _matrix.clearScreen();
        return;
    }
    ESP_LOGD(TAG, "Playing %s (%dx%d)", _path.c_str(), _gif->getCanvasWidth(), _gif->getCanvasHeight());
    if (!decodeFrame()) {
        close();
        return;
    }
    _nextFrameAt = nowMs + static_cast<uint32_t>(_frameDelayMs);
}

bool GifScene::tick(uint32_t nowMs) {
    if (!_gif) {
        return false;
    }
    if (static_cast<int32_t>(nowMs - _nextFrameAt) < 0) {
        return true;
    }
    if (_lastFrame || !decodeFrame()) {
        // The original clears the panel once the GIF has played.
        close();
        _matrix.clearScreen();
        return false;
    }
    _nextFrameAt = nowMs + static_cast<uint32_t>(_frameDelayMs);
    return true;
}

void GifScene::abort() {
    close();
    _matrix.clearScreen();
}

ImageScene::ImageScene(Hub75_Matrix& matrix, std::string path, bool center, MediaOverlay overlay)
    : _matrix(matrix), _path(std::move(path)), _center(center), _overlay(std::move(overlay)) {}

ImageScene::~ImageScene() = default;

void ImageScene::start(uint32_t nowMs) {
    (void)nowMs;
    std::shared_ptr<Bitmap565> bitmap = loadPng(_path, _matrix.width(), _matrix.height(), _center);
    std::unique_ptr<GFXcanvas1> text = overlayImage(_overlay, _matrix.width(), _matrix.height());
    presentFrame(_matrix, bitmap.get(), text.get(), _overlay.color);
}

void ImageScene::abort() {
    _matrix.clearScreen();
}

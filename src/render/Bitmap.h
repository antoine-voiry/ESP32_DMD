#ifndef DMD_RENDER_BITMAP_H
#define DMD_RENDER_BITMAP_H

// Panel-sized RGB565 frame buffer plus helpers shared by the image, GIF and clock scenes.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/TextUtil.h"
#include "matrix/Hub75_Matrix.h"

class GFXcanvas1;

struct Bitmap565 {
    Bitmap565(int width, int height) : w(width), h(height), px(static_cast<size_t>(width) * height, 0) {}
    int w;
    int h;
    std::vector<uint16_t> px;  // row-major RGB565

    void set(int x, int y, uint16_t c) {
        if (x >= 0 && y >= 0 && x < w && y < h) px[static_cast<size_t>(y) * w + x] = c;
    }
};

// Decodes a PNG, shrunk to fit w x h (never enlarged, centred when `center`, like PIL thumbnail +
// center_images). Transparent pixels are blended with black. nullptr if the file cannot be decoded.
std::shared_ptr<Bitmap565> loadPng(const std::string& path, int w, int h, bool center);

// Shows a frame: the bitmap (or black), then optional 1-bit text centred on top with a black
// outline for readability, then present().
void presentFrame(Hub75_Matrix& matrix, const Bitmap565* background, const GFXcanvas1* text,
                  const dmd::Rgb& textColor);

#endif

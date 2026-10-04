#ifndef DMD_CORE_CANVAS_H
#define DMD_CORE_CANVAS_H

// Minimal RGB565 canvas with the few primitives the generated screens need (weather icons,
// wind arrow, EDF Tempo days). Plain C++ so drawings can be unit tested on the host.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dmd {

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

struct Canvas {
    Canvas(int width, int height) : w(width), h(height), px(static_cast<size_t>(width) * height, 0) {}
    int w;
    int h;
    std::vector<uint16_t> px;  // row-major RGB565

    void set(int x, int y, uint16_t c) {
        if (x >= 0 && y >= 0 && x < w && y < h) px[static_cast<size_t>(y) * w + x] = c;
    }
    uint16_t get(int x, int y) const {
        return (x >= 0 && y >= 0 && x < w && y < h) ? px[static_cast<size_t>(y) * w + x] : 0;
    }
    void clear(uint16_t c = 0);
    void fillRect(int x, int y, int rw, int rh, uint16_t c);
    void drawRect(int x, int y, int rw, int rh, uint16_t c);
    void fillCircle(int cx, int cy, int r, uint16_t c);
    void drawLine(int x0, int y0, int x1, int y1, uint16_t c);
    // Copies src at (x, y), clipped; pixels equal to `transparent` are skipped when skipTransparent.
    void blit(const Canvas& src, int x, int y, bool skipTransparent = false, uint16_t transparent = 0);
    // Scales every pixel's brightness by keep/255 (fading trails).
    void fade(uint8_t keep);
    // Number of pixels of colour c (handy in tests).
    int count(uint16_t c) const;
};

}  // namespace dmd

#endif

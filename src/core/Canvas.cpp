#include "Canvas.h"

#include <cstdlib>

namespace dmd {

void Canvas::clear(uint16_t c) {
    for (auto& p : px) p = c;
}

void Canvas::fillRect(int x, int y, int rw, int rh, uint16_t c) {
    for (int yy = y; yy < y + rh; ++yy) {
        for (int xx = x; xx < x + rw; ++xx) {
            set(xx, yy, c);
        }
    }
}

void Canvas::drawRect(int x, int y, int rw, int rh, uint16_t c) {
    if (rw <= 0 || rh <= 0) return;
    for (int xx = x; xx < x + rw; ++xx) {
        set(xx, y, c);
        set(xx, y + rh - 1, c);
    }
    for (int yy = y; yy < y + rh; ++yy) {
        set(x, yy, c);
        set(x + rw - 1, yy, c);
    }
}

void Canvas::fillCircle(int cx, int cy, int r, uint16_t c) {
    if (r < 0) return;
    for (int yy = -r; yy <= r; ++yy) {
        for (int xx = -r; xx <= r; ++xx) {
            if (xx * xx + yy * yy <= r * r + r) set(cx + xx, cy + yy, c);
        }
    }
}

void Canvas::drawLine(int x0, int y0, int x1, int y1, uint16_t c) {
    // Bresenham
    const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        set(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void Canvas::blit(const Canvas& src, int x, int y, bool skipTransparent, uint16_t transparent) {
    for (int yy = 0; yy < src.h; ++yy) {
        for (int xx = 0; xx < src.w; ++xx) {
            const uint16_t c = src.px[static_cast<size_t>(yy) * src.w + xx];
            if (skipTransparent && c == transparent) continue;
            set(x + xx, y + yy, c);
        }
    }
}

int Canvas::count(uint16_t c) const {
    int n = 0;
    for (auto p : px) n += p == c ? 1 : 0;
    return n;
}

}  // namespace dmd

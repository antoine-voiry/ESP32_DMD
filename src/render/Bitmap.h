#ifndef DMD_RENDER_BITMAP_H
#define DMD_RENDER_BITMAP_H

// Panel-sized RGB565 frame buffer plus helpers shared by the image, GIF and clock scenes.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/Canvas.h"
#include "core/TextUtil.h"
#include "matrix/Hub75_Matrix.h"

class GFXcanvas1;

using Bitmap565 = dmd::Canvas;

// Decodes a PNG, shrunk to fit w x h (never enlarged, centred when `center`, like PIL thumbnail +
// center_images). Transparent pixels are blended with black. nullptr if the file cannot be decoded.
std::shared_ptr<Bitmap565> loadPng(const std::string& path, int w, int h, bool center);

// Draws text (largest font that fits) into the box (x, y, w, h) of the canvas, centred.
void drawTextBox(dmd::Canvas& canvas, const std::string& text, int x, int y, int w, int h, const dmd::Rgb& color,
                 int maxFontPx = 32);

// Shows a frame: the bitmap (or black), then optional 1-bit text centred on top with a black
// outline for readability, then present().
void presentFrame(Hub75_Matrix& matrix, const Bitmap565* background, const GFXcanvas1* text,
                  const dmd::Rgb& textColor);

#endif

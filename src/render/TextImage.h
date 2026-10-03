#ifndef DMD_RENDER_TEXT_IMAGE_H
#define DMD_RENDER_TEXT_IMAGE_H

// Lays text out with the largest bitmap font that fits, into a 1-bit image.
// Shared by TextScene (movements) and FxScene (effects).

#include <cstddef>
#include <memory>
#include <string>

class GFXcanvas1;

struct TextLayoutOptions {
    bool singleLine = false;      // left/right scrolls: one line, image as wide as the text
    size_t maxCharsPerLine = 22;  // TextRenderer.maxcharacter
    int maxFontPx = 32;           // TextRenderer.maxfontsize
};

// text must already be ASCII (see dmd::toDisplayAscii). Returns nullptr if out of memory.
// Multi-line layouts are dispW x dispH, lines centred; single-line images are textWidth x dispH.
std::unique_ptr<GFXcanvas1> renderTextImage(const std::string& text, int dispW, int dispH,
                                            const TextLayoutOptions& options);

#endif

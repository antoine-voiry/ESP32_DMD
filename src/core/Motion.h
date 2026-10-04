#ifndef DMD_CORE_MOTION_H
#define DMD_CORE_MOTION_H

// Frame-by-frame description of the RenderText() movements ("sens") from DMDRenderer.py.
// A Timeline says where/how to draw the pre-rendered text image for each frame; the device side
// only has to blit it. Frame positions and delays follow the Python loops one-to-one.

#include <cstddef>
#include <cstdint>
#include <string>

namespace dmd {

enum class Motion { None, Left, Right, Up, Down, Rotate, AntiRotate, Flip, Twirl };

// "left", "right", "up", "down", "rotate", "antirotate", "flip", "twirl"; anything else -> None.
Motion parseMotion(const std::string& sens);

// Carousel option codes: DG, GD, BH, HB, ROT, ARO, FLI, TWI. "A" (random) is resolved by the caller.
Motion motionFromCarouselCode(const std::string& code);

// Left/right scrolls render the text on a single line whose width is the text width.
bool isHorizontalScroll(Motion m);

struct Frame {
    int16_t x = 0;       // top-left of the (scaled) image on the display
    int16_t y = 0;
    uint16_t w = 0;      // scaled size of the image
    uint16_t h = 0;
    bool flipX = false;  // image mirrored left/right (twirl)
    bool flipY = false;  // image mirrored top/bottom (flip)
    int16_t angle = 0;   // counter-clockwise degrees around the image centre (PIL convention)
    uint16_t delayMs = 0;  // how long the frame stays before the next one
};

class Timeline {
public:
    // imgW/imgH: size of the rendered text image. dispW/dispH: panel size. iterations >= 1.
    Timeline(Motion motion, int imgW, int imgH, int dispW, int dispH, int iterations = 1);

    size_t size() const { return _perIteration * static_cast<size_t>(_iterations); }
    Frame at(size_t index) const;
    Motion motion() const { return _motion; }

private:
    Frame frameInIteration(size_t i) const;

    Motion _motion;
    int _imgW;
    int _imgH;
    int _dispW;
    int _dispH;
    int _iterations;
    size_t _perIteration;
};

}  // namespace dmd

#endif

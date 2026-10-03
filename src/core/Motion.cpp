#include "Motion.h"

namespace dmd {

namespace {

// Delays used by DMDRenderer.py RenderText() (DMD branch).
constexpr uint16_t kVerticalDelayMs = 40;
constexpr uint16_t kHorizontalDelayMs = 10;
constexpr uint16_t kRotateDelayMs = 100;
constexpr uint16_t kFlipDelayMs = 9;
constexpr uint16_t kTwirlDelayMs = 5;  // 4.5 ms in the original
constexpr int kRotateSteps = 9;        // 0..360 by 45 degrees

Frame makeFrame(int x, int y, int w, int h, uint16_t delay) {
    Frame f;
    f.x = static_cast<int16_t>(x);
    f.y = static_cast<int16_t>(y);
    f.w = static_cast<uint16_t>(w < 0 ? 0 : w);
    f.h = static_cast<uint16_t>(h < 0 ? 0 : h);
    f.delayMs = delay;
    return f;
}

// Flip and twirl share the same 6-step choreography, along Y (flip) or X (twirl):
//   full image, shrink to 1px, unfold mirrored, shrink while sliding, unfold back to normal, full image.
// n is the size along the animated axis. Returns {offset, size, mirrored} for step i.
struct FoldStep {
    int offset;
    int size;
    bool mirrored;
};

size_t foldFrameCount(int n) {
    return 1 + static_cast<size_t>(n) + static_cast<size_t>(n - 1) + static_cast<size_t>(n) +
           static_cast<size_t>(n - 1) + 1;
}

FoldStep foldStep(int n, size_t i) {
    if (i == 0) return {0, n, false};
    i -= 1;
    if (i < static_cast<size_t>(n)) return {0, n - static_cast<int>(i), false};  // n .. 1
    i -= n;
    if (i < static_cast<size_t>(n - 1)) return {0, 1 + static_cast<int>(i), true};  // 1 .. n-1
    i -= n - 1;
    if (i < static_cast<size_t>(n)) return {1 + static_cast<int>(i), n - static_cast<int>(i), true};
    i -= n;
    if (i < static_cast<size_t>(n - 1)) return {n - static_cast<int>(i), 1 + static_cast<int>(i), false};
    return {0, n, false};
}

}  // namespace

Motion parseMotion(const std::string& sens) {
    if (sens == "left") return Motion::Left;
    if (sens == "right") return Motion::Right;
    if (sens == "up") return Motion::Up;
    if (sens == "down") return Motion::Down;
    if (sens == "rotate") return Motion::Rotate;
    if (sens == "antirotate") return Motion::AntiRotate;
    if (sens == "flip") return Motion::Flip;
    if (sens == "twirl") return Motion::Twirl;
    return Motion::None;
}

Motion motionFromCarouselCode(const std::string& code) {
    if (code == "GD") return Motion::Right;
    if (code == "DG") return Motion::Left;
    if (code == "HB") return Motion::Down;
    if (code == "BH") return Motion::Up;
    if (code == "ROT") return Motion::Rotate;
    if (code == "ARO") return Motion::AntiRotate;
    if (code == "FLI") return Motion::Flip;
    if (code == "TWI") return Motion::Twirl;
    return Motion::None;
}

bool isHorizontalScroll(Motion m) {
    return m == Motion::Left || m == Motion::Right;
}

Timeline::Timeline(Motion motion, int imgW, int imgH, int dispW, int dispH, int iterations)
    : _motion(motion), _imgW(imgW), _imgH(imgH), _dispW(dispW), _dispH(dispH),
      _iterations(iterations < 1 ? 1 : iterations) {
    switch (_motion) {
        case Motion::Up:
        case Motion::Down:
            _perIteration = static_cast<size_t>(2 * _dispH);
            break;
        case Motion::Left:
        case Motion::Right:
            _perIteration = static_cast<size_t>(_dispW + _imgW);
            break;
        case Motion::Rotate:
        case Motion::AntiRotate:
            _perIteration = kRotateSteps;
            break;
        case Motion::Flip:
            _perIteration = _imgH > 0 ? foldFrameCount(_imgH) : 1;
            break;
        case Motion::Twirl:
            _perIteration = _imgW > 0 ? foldFrameCount(_imgW) : 1;
            break;
        case Motion::None:
        default:
            _perIteration = 1;
            break;
    }
    if (_motion == Motion::None) {
        _iterations = 1;
    }
}

Frame Timeline::at(size_t index) const {
    return frameInIteration(_perIteration ? index % _perIteration : 0);
}

Frame Timeline::frameInIteration(size_t i) const {
    const int k = static_cast<int>(i);
    switch (_motion) {
        case Motion::Up:  // y = H .. -H+1
            return makeFrame(0, _dispH - k, _imgW, _imgH, kVerticalDelayMs);
        case Motion::Down:  // y = -H .. H-1
            return makeFrame(0, -_dispH + k, _imgW, _imgH, kVerticalDelayMs);
        case Motion::Right:  // x = -imgW .. W-1
            return makeFrame(-_imgW + k, 0, _imgW, _imgH, kHorizontalDelayMs);
        case Motion::Left:  // x = W .. -imgW+1
            return makeFrame(_dispW - k, 0, _imgW, _imgH, kHorizontalDelayMs);
        case Motion::AntiRotate: {
            Frame f = makeFrame(0, 0, _imgW, _imgH, kRotateDelayMs);
            f.angle = static_cast<int16_t>(45 * k);  // 0 .. 360
            return f;
        }
        case Motion::Rotate: {
            Frame f = makeFrame(0, 0, _imgW, _imgH, kRotateDelayMs);
            f.angle = static_cast<int16_t>(360 - 45 * k);  // 360 .. 0
            return f;
        }
        case Motion::Flip: {
            if (_imgH <= 0) break;
            FoldStep s = foldStep(_imgH, i);
            Frame f = makeFrame(0, s.offset, _imgW, s.size, kFlipDelayMs);
            f.flipY = s.mirrored;
            return f;
        }
        case Motion::Twirl: {
            if (_imgW <= 0) break;
            FoldStep s = foldStep(_imgW, i);
            Frame f = makeFrame(s.offset, 0, s.size, _imgH, kTwirlDelayMs);
            f.flipX = s.mirrored;
            return f;
        }
        case Motion::None:
        default:
            break;
    }
    return makeFrame(0, 0, _imgW, _imgH, 0);
}

}  // namespace dmd

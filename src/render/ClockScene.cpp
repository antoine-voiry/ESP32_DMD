#include "ClockScene.h"

#include <Adafruit_GFX.h>
#include <time.h>

#include <utility>

#include "TextImage.h"
#include "core/Clock.h"

namespace {

// Before NTP has answered, time() is close to 1970.
constexpr int kFirstValidYear = 2024;

}  // namespace

ClockScene::ClockScene(Hub75_Matrix& matrix, ClockSpec spec) : _matrix(matrix), _spec(std::move(spec)) {
    if (_spec.mode != 3 && _spec.mode != 4) {
        _spec.mode = 2;  // 1 (the GetConfig() default) did nothing on the Pi; alternate instead
    }
}

ClockScene::~ClockScene() = default;

uint32_t ClockScene::durationMs() const {
    switch (_spec.mode) {
        case 3: return _spec.dateMs;
        case 4: return _spec.hoursMs;
        default: return _spec.dateMs + _spec.hoursMs;
    }
}

bool ClockScene::showingDate(uint32_t elapsedMs) const {
    if (_spec.mode == 3) return true;
    if (_spec.mode == 4) return false;
    return elapsedMs < _spec.dateMs;  // the original shows the date first
}

std::string ClockScene::currentText(bool date) const {
    const time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    if (t.tm_year + 1900 < kFirstValidYear) {
        return date ? "--/--" : "--:--";  // not synchronised yet
    }
    dmd::DateTime dt;
    dt.year = t.tm_year + 1900;
    dt.month = t.tm_mon + 1;
    dt.day = t.tm_mday;
    dt.hour = t.tm_hour;
    dt.minute = t.tm_min;
    dt.second = t.tm_sec;
    dt.weekday = t.tm_wday;
    return dmd::toDisplayAscii(dmd::formatDateTime(date ? _spec.dateFormat : _spec.hourFormat, dt, _spec.lang));
}

void ClockScene::draw(const std::string& text) {
    const int w = _matrix.width();
    const int h = _matrix.height();
    TextLayoutOptions options;
    options.maxFontPx = _spec.maxFontPx;
    _image = renderTextImage(text, w, h, options);

    const uint16_t fg = _matrix.color(_spec.fg.r, _spec.fg.g, _spec.fg.b);
    const uint16_t shadow = _matrix.color(_spec.shadow.r, _spec.shadow.g, _spec.shadow.b);
    _matrix.fillScreen(0);
    if (_image) {
        // Shadow first, offset by one pixel, then the text itself.
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (_image->getPixel(x, y)) _matrix.drawPixel(x + 1, y + 1, shadow);
            }
        }
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (_image->getPixel(x, y)) _matrix.drawPixel(x, y, fg);
            }
        }
    }
    _matrix.present();
    _shown = text;
}

void ClockScene::start(uint32_t nowMs) {
    _startMs = nowMs;
    _lastCheckMs = nowMs;
    draw(currentText(showingDate(0)));
}

bool ClockScene::tick(uint32_t nowMs) {
    const uint32_t elapsed = nowMs - _startMs;
    if (elapsed >= durationMs()) {
        return false;  // last frame stays until the next scene
    }
    // Check the time 10 times a second and redraw only when the text changes.
    if (nowMs - _lastCheckMs < 100) {
        return true;
    }
    _lastCheckMs = nowMs;
    const std::string text = currentText(showingDate(elapsed));
    if (text != _shown) {
        draw(text);
    }
    return true;
}

void ClockScene::abort() {
    _matrix.clearScreen();
}

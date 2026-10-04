#ifndef DMD_RENDER_CLOCK_SCENE_H
#define DMD_RENDER_CLOCK_SCENE_H

// Port of DMDRenderer.RunTime(): shows the date and/or the time for a fixed duration.
// The background pattern (ClockRenderer.clockBackgroundImage) needs PNG support (phase 3).

#include <memory>
#include <string>

#include "core/SceneRunner.h"
#include "core/TextUtil.h"
#include "matrix/Hub75_Matrix.h"

class GFXcanvas1;

struct ClockSpec {
    int mode = 2;               // showing_datehours: 2 date then time, 3 date only, 4 time only
    uint32_t dateMs = 2000;     // timeShow_Date
    uint32_t hoursMs = 4000;    // timeShow_Hours
    std::string dateFormat = "%d %b %Y";
    std::string hourFormat = "%H:%M:%S";
    std::string lang = "fr";
    dmd::Rgb fg{0, 0, 255};     // defaultfontcolor_clock
    dmd::Rgb shadow{255, 0, 0}; // defaultfontcolor_clockshadow
    int maxFontPx = 32;
};

class ClockScene : public dmd::Scene {
public:
    ClockScene(Hub75_Matrix& matrix, ClockSpec spec);
    ~ClockScene() override;

    void start(uint32_t nowMs) override;
    bool tick(uint32_t nowMs) override;
    void abort() override;

    uint32_t durationMs() const;

private:
    bool showingDate(uint32_t elapsedMs) const;
    std::string currentText(bool date) const;
    void draw(const std::string& text);

    Hub75_Matrix& _matrix;
    ClockSpec _spec;
    std::unique_ptr<GFXcanvas1> _image;
    std::string _shown;
    uint32_t _startMs = 0;
    uint32_t _lastCheckMs = 0;
};

#endif

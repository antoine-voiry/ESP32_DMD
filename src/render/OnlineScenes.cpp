#include "OnlineScenes.h"

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include <algorithm>
#include <utility>

#include "util/Storage.h"

namespace {

constexpr uint32_t kTimeoutMs = 20000;
constexpr uint32_t kLoadingFrameMs = 200;
const dmd::Rgb kDimDate(60, 60, 60);  // the original drew the forecast date in grey behind the slots

dmd::DateTime localToday() {
    const time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    dmd::DateTime dt;
    dt.year = t.tm_year + 1900;
    dt.month = t.tm_mon + 1;
    dt.day = t.tm_mday;
    dt.weekday = t.tm_wday;
    return dt;
}

// A PNG from `dir` when the user uploaded one (the Pi's icon sets), else a generated drawing.
bool blitIconPng(Bitmap565& page, const std::string& dir, const std::string& name, int x, int y, int size) {
    const std::string path = dir + "/" + name + ".png";
    if (!storageExists(path)) return false;
    std::shared_ptr<Bitmap565> icon = loadPng(path, size, size, true);
    if (!icon) return false;
    page.blit(*icon, x, y, true, 0);
    return true;
}

std::string formatUptime(uint32_t ms) {
    const uint32_t minutes = ms / 60000;
    if (minutes < 60) return std::to_string(minutes) + "min";
    return std::to_string(minutes / 60) + "h" + (minutes % 60 < 10 ? "0" : "") + std::to_string(minutes % 60);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OnlineScene

OnlineScene::OnlineScene(Hub75_Matrix& matrix, OnlineService& service, OnlineService::Kind kind, uint32_t pageMs,
                         dmd::Rgb color)
    : _matrix(matrix), _service(service), _color(color), _kind(kind), _pageMs(pageMs < 500 ? 500 : pageMs) {}

void OnlineScene::start(uint32_t nowMs) {
    _startMs = nowMs;
    _service.request(_kind);
    drawLoading(nowMs);
}

void OnlineScene::drawLoading(uint32_t nowMs) {
    // Three dots lighting up in turn.
    Bitmap565 frame(_matrix.width(), _matrix.height());
    const int lit = static_cast<int>((nowMs - _startMs) / kLoadingFrameMs) % 3;
    const int cy = frame.h / 2, cx = frame.w / 2;
    for (int i = 0; i < 3; ++i) {
        const uint8_t level = i == lit ? 255 : 60;
        frame.fillCircle(cx + (i - 1) * 7, cy, 2,
                         dmd::rgb565(_color.r * level / 255, _color.g * level / 255, _color.b * level / 255));
    }
    presentFrame(_matrix, &frame, nullptr, _color);
}

void OnlineScene::showError(const std::string& message) {
    _failed = true;
    Bitmap565 frame(_matrix.width(), _matrix.height());
    drawTextBox(frame, message, 0, 0, frame.w, frame.h, dmd::Rgb(255, 80, 0));
    presentFrame(_matrix, &frame, nullptr, _color);
}

bool OnlineScene::tick(uint32_t nowMs) {
    if (_failed) {
        return false;  // the message stays until the next scene
    }
    if (!_loaded) {
        const OnlineService::State state = _service.state(_kind);
        if (state == OnlineService::State::Ready) {
            _loaded = true;
            _pages.clear();
            buildPages(_pages);
            if (_pages.empty()) {
                showError("Aucune donnee");
                return false;
            }
            _page = 0;
            _pageStartMs = nowMs;
            presentFrame(_matrix, &_pages[0], nullptr, _color);
            return true;
        }
        if (state == OnlineService::State::Failed) {
            showError(_service.error(_kind));
            return false;
        }
        if (nowMs - _startMs > kTimeoutMs) {
            showError("Delai depasse");
            return false;
        }
        drawLoading(nowMs);
        return true;
    }
    if (nowMs - _pageStartMs < _pageMs) {
        return true;
    }
    if (++_page >= _pages.size()) {
        return false;  // last page stays on screen
    }
    _pageStartMs = nowMs;
    presentFrame(_matrix, &_pages[_page], nullptr, _color);
    return true;
}

void OnlineScene::abort() {
    _matrix.clearScreen();
}

////////////////////////////////////////////////////////////////////////////////
// Current weather

CurrentWeatherScene::CurrentWeatherScene(Hub75_Matrix& m, OnlineService& s, const dmd::OwmConfig& cfg, dmd::Rgb color)
    : OnlineScene(m, s, OnlineService::Kind::Current, static_cast<uint32_t>(cfg.seeDuringSec) * 1000u, color),
      _units(cfg.units) {}

void CurrentWeatherScene::buildPages(std::vector<Bitmap565>& pages) {
    dmd::CurrentWeather w;
    if (!_service.currentWeather(w)) return;
    Bitmap565 page(_matrix.width(), _matrix.height());
    const int iconSize = std::min(page.h - 6, page.w / 3);
    const int iconY = (page.h - iconSize) / 2;
    if (!blitIconPng(page, "/meteo", w.icon, 0, iconY, iconSize)) {
        dmd::drawWeatherIcon(page, dmd::weatherKind(w.icon), dmd::isNightIcon(w.icon), 0, iconY, iconSize);
    }
    const int x0 = iconSize + 2, rw = page.w - x0;
    drawTextBox(page, dmd::formatTemperature(w.temp), x0, 0, rw, page.h / 2, _color);
    const int arrowLen = page.h / 2 - 4;
    dmd::drawWindArrow(page, x0 + arrowLen / 2 + 1, page.h * 3 / 4, arrowLen, w.windDeg,
                       dmd::rgb565(_color.r, _color.g, _color.b));
    drawTextBox(page, dmd::formatWind(w.windSpeed, _units), x0 + arrowLen + 3, page.h / 2, rw - arrowLen - 3,
                page.h / 2, _color);
    pages.push_back(page);
}

////////////////////////////////////////////////////////////////////////////////
// Forecast

ForecastScene::ForecastScene(Hub75_Matrix& m, OnlineService& s, const dmd::OwmConfig& cfg, dmd::Rgb color)
    : OnlineScene(m, s, OnlineService::Kind::Forecast, static_cast<uint32_t>(cfg.seeDuringSec) * 1000u, color),
      _days(cfg.prevision) {}

void ForecastScene::buildPages(std::vector<Bitmap565>& pages) {
    std::vector<dmd::ForecastItem> items;
    if (!_service.forecast(items)) return;
    const int w = _matrix.width(), h = _matrix.height();
    // The Pi showed 4 slots on 128 px; keep ~32 px per slot.
    const size_t perPage = static_cast<size_t>(std::max(1, w / 32));
    const int cw = w / static_cast<int>(perPage);
    const int iconSize = std::max(8, h - 18);
    for (const dmd::ForecastPage& fp : dmd::forecastPages(items, dmd::forecastDates(localToday(), _days), perPage)) {
        Bitmap565 page(w, h);
        drawTextBox(page, fp.date, 0, 0, w, h, kDimDate);
        for (size_t i = 0; i < fp.slots.size(); ++i) {
            const dmd::ForecastSlot& slot = fp.slots[i];
            const int x = static_cast<int>(i) * cw;
            drawTextBox(page, slot.time, x, 0, cw, 8, _color, 8);
            const int ix = x + (cw - iconSize) / 2;
            if (!blitIconPng(page, "/meteo", slot.icon, ix, 9, iconSize)) {
                dmd::drawWeatherIcon(page, dmd::weatherKind(slot.icon), dmd::isNightIcon(slot.icon), ix, 9, iconSize);
            }
            drawTextBox(page, std::to_string(slot.temp) + "\xC2\xB0", x, h - 8, cw, 8, _color, 8);
        }
        pages.push_back(page);
    }
}

////////////////////////////////////////////////////////////////////////////////
// EDF Tempo

TempoScene::TempoScene(Hub75_Matrix& m, OnlineService& s, uint32_t pageMs, dmd::Rgb color)
    : OnlineScene(m, s, OnlineService::Kind::Tempo, pageMs, color) {}

void TempoScene::buildPages(std::vector<Bitmap565>& pages) {
    std::vector<dmd::TempoDay> days;
    if (!_service.tempo(days)) return;
    Bitmap565 page(_matrix.width(), _matrix.height());
    const int half = page.w / 2;
    const int square = std::min(half - 4, page.h - 10);
    for (size_t i = 0; i < days.size() && i < 2; ++i) {
        const int x = static_cast<int>(i) * half;
        const int sx = x + (half - square) / 2;
        if (!blitIconPng(page, "/edfjourstempo", std::to_string(days[i].code), sx, 0, square)) {
            const dmd::Rgb c = dmd::tempoColor(days[i].code);
            page.fillRect(sx, 0, square, square, dmd::rgb565(c.r, c.g, c.b));
        }
        drawTextBox(page, dmd::formatTempoDate(days[i].date), x, square + 1, half, page.h - square - 1, _color, 8);
    }
    pages.push_back(page);
}

////////////////////////////////////////////////////////////////////////////////
// Geocoding

GeoScene::GeoScene(Hub75_Matrix& m, OnlineService& s, dmd::Rgb color, std::function<void(const dmd::GeoResult&)> apply)
    : OnlineScene(m, s, OnlineService::Kind::Geo, 4000, color), _apply(std::move(apply)) {}

void GeoScene::buildPages(std::vector<Bitmap565>& pages) {
    dmd::GeoResult g;
    if (!_service.geo(g)) return;
    if (_apply) _apply(g);
    Bitmap565 page(_matrix.width(), _matrix.height());
    drawTextBox(page, g.name + " " + g.lat + " " + g.lon, 0, 0, page.w, page.h, _color);
    pages.push_back(page);
}

////////////////////////////////////////////////////////////////////////////////
// perf

PerfScene::PerfScene(Hub75_Matrix& m, uint32_t pageMs, dmd::Rgb color)
    : _matrix(m), _pageMs(pageMs < 1000 ? 1000 : pageMs), _color(color) {}

void PerfScene::draw(int page) {
    Bitmap565 frame(_matrix.width(), _matrix.height());
    std::string text;
    switch (page) {
        case 0:
            text = "Temp " + std::to_string(static_cast<int>(temperatureRead())) + "\xC2\xB0" "C CPU " +
                   std::to_string(getCpuFrequencyMhz()) + "MHz";
            break;
        case 1:
            text = "RAM " + std::to_string(ESP.getFreeHeap() / 1024) + "K Up " + formatUptime(millis());
            break;
        default:
            text = WiFi.status() == WL_CONNECTED
                       ? std::string("WiFi ") + std::to_string(WiFi.RSSI()) + "dB " + WiFi.localIP().toString().c_str()
                       : std::string("WiFi deconnecte");
            break;
    }
    drawTextBox(frame, text, 0, 0, frame.w, frame.h, _color);
    presentFrame(_matrix, &frame, nullptr, _color);
}

void PerfScene::start(uint32_t nowMs) {
    _startMs = nowMs;
    _lastDrawMs = nowMs;
    _page = 0;
    draw(0);
}

bool PerfScene::tick(uint32_t nowMs) {
    const int page = static_cast<int>((nowMs - _startMs) / _pageMs);
    if (page >= 3) return false;
    // Values change: refresh every second.
    if (page != _page || nowMs - _lastDrawMs >= 1000) {
        _page = page;
        _lastDrawMs = nowMs;
        draw(page);
    }
    return true;
}

void PerfScene::abort() {
    _matrix.clearScreen();
}

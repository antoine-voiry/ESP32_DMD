#ifndef DMD_RENDER_ONLINE_SCENES_H
#define DMD_RENDER_ONLINE_SCENES_H

// Ports of RenderMeteoInTime (meteo), RenderMeteoPrevisionnelle (meteoPrevi), RenderEDFJoursTempo
// (edfJoursTempo), RenderSOC (perf) and ZipPostCodeGeocoding / FindLatLonCityname (owmzc, fllcn).
// Data comes from OnlineService; while it loads, a small animation is shown (the Pi's waitLoading.png).

#include <functional>
#include <string>
#include <vector>

#include "Bitmap.h"
#include "core/Online.h"
#include "core/SceneRunner.h"
#include "matrix/Hub75_Matrix.h"
#include "util/OnlineService.h"

class OnlineScene : public dmd::Scene {
public:
    OnlineScene(Hub75_Matrix& matrix, OnlineService& service, OnlineService::Kind kind, uint32_t pageMs,
                dmd::Rgb color);

    void start(uint32_t nowMs) override;
    bool tick(uint32_t nowMs) override;
    void abort() override;

protected:
    // Called once the data is ready: builds the pages (one canvas each).
    virtual void buildPages(std::vector<Bitmap565>& pages) = 0;

    Hub75_Matrix& _matrix;
    OnlineService& _service;
    dmd::Rgb _color;

private:
    void drawLoading(uint32_t nowMs);
    void showError(const std::string& message);

    OnlineService::Kind _kind;
    uint32_t _pageMs;
    uint32_t _startMs = 0;
    uint32_t _pageStartMs = 0;
    size_t _page = 0;
    bool _loaded = false;
    bool _failed = false;
    std::vector<Bitmap565> _pages;
};

class CurrentWeatherScene : public OnlineScene {
public:
    CurrentWeatherScene(Hub75_Matrix& m, OnlineService& s, const dmd::OwmConfig& cfg, dmd::Rgb color);

protected:
    void buildPages(std::vector<Bitmap565>& pages) override;

private:
    std::string _units;
};

class ForecastScene : public OnlineScene {
public:
    ForecastScene(Hub75_Matrix& m, OnlineService& s, const dmd::OwmConfig& cfg, dmd::Rgb color);

protected:
    void buildPages(std::vector<Bitmap565>& pages) override;

private:
    int _days;
};

class TempoScene : public OnlineScene {
public:
    TempoScene(Hub75_Matrix& m, OnlineService& s, uint32_t pageMs, dmd::Rgb color);

protected:
    void buildPages(std::vector<Bitmap565>& pages) override;
};

// owmzc / fllcn: looks the zip code up and hands the result to `apply` (stores the settings).
class GeoScene : public OnlineScene {
public:
    GeoScene(Hub75_Matrix& m, OnlineService& s, dmd::Rgb color, std::function<void(const dmd::GeoResult&)> apply);

protected:
    void buildPages(std::vector<Bitmap565>& pages) override;

private:
    std::function<void(const dmd::GeoResult&)> _apply;
};

// perf: board status pages (temperature, CPU, memory, Wi-Fi, uptime), refreshed while shown.
class PerfScene : public dmd::Scene {
public:
    PerfScene(Hub75_Matrix& m, uint32_t pageMs, dmd::Rgb color);
    void start(uint32_t nowMs) override;
    bool tick(uint32_t nowMs) override;
    void abort() override;

private:
    void draw(int page);

    Hub75_Matrix& _matrix;
    uint32_t _pageMs;
    dmd::Rgb _color;
    uint32_t _startMs = 0;
    uint32_t _lastDrawMs = 0;
    int _page = -1;
};

#endif

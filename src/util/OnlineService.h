#ifndef DMD_ONLINE_SERVICE_H
#define DMD_ONLINE_SERVICE_H

// Fetches OpenWeatherMap and EDF Tempo data on a background FreeRTOS task, so that HTTPS requests
// (often 1-3 s) never block loop(): MQTT, the web server and animations keep running.
// Results are cached like the Pi's token files: weather for OpenWeatherMap.callevery minutes,
// Tempo until the next day.

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <string>
#include <vector>

#include "core/Online.h"

class OnlineService {
public:
    enum class Kind { Current = 0, Forecast, Tempo, Geo, Count };
    enum class State { Idle, Loading, Ready, Failed };

    void begin();

    // Asks for fresh data (no-op if the cached data is still valid or a request is pending).
    void request(Kind kind);
    // Drops the cache (settings changed).
    void invalidate();

    State state(Kind kind) const;
    std::string error(Kind kind) const;

    bool currentWeather(dmd::CurrentWeather& out) const;
    bool forecast(std::vector<dmd::ForecastItem>& out) const;
    bool tempo(std::vector<dmd::TempoDay>& out) const;
    bool geo(dmd::GeoResult& out) const;

private:
    struct Slot {
        State state = State::Idle;
        std::string error;
        uint32_t fetchedAtMs = 0;
        bool pending = false;
    };

    static void taskEntry(void* self);
    void run();
    void fetch(Kind kind);
    bool httpGet(const std::string& url, std::string& body, std::string& error);
    bool fresh(Kind kind, uint32_t nowMs) const;

    mutable SemaphoreHandle_t _lock = nullptr;
    TaskHandle_t _task = nullptr;
    Slot _slots[static_cast<int>(Kind::Count)];
    dmd::CurrentWeather _current;
    std::vector<dmd::ForecastItem> _forecast;
    std::vector<dmd::TempoDay> _tempo;
    std::string _tempoDate;  // day the Tempo answer is for
    dmd::GeoResult _geo;
};

#endif

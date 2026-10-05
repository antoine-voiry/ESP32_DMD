#include "OnlineService.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_log.h>
#include <time.h>

#include "Settings.h"
#include "net/OnlineJson.h"

static const char* TAG = "Online";

namespace {

constexpr uint32_t kHttpTimeoutMs = 8000;
constexpr size_t kMaxBodyBytes = 48 * 1024;
constexpr uint32_t kTaskStackBytes = 12288;  // TLS handshakes need room

class Lock {
public:
    explicit Lock(SemaphoreHandle_t m) : _m(m) { xSemaphoreTake(_m, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(_m); }

private:
    SemaphoreHandle_t _m;
};

// Collects a response body, refusing anything past `cap` bytes.
class BodySink : public Stream {
public:
    BodySink(std::string& out, size_t cap) : _out(out), _cap(cap) { _out.clear(); }
    size_t write(uint8_t c) override { return write(&c, 1); }
    size_t write(const uint8_t* data, size_t size) override {
        if (_out.size() + size > _cap) {
            _overflowed = true;
            return 0;
        }
        _out.append(reinterpret_cast<const char*>(data), size);
        return size;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    bool overflowed() const { return _overflowed; }

private:
    std::string& _out;
    size_t _cap;
    bool _overflowed = false;
};

// What a request needs, captured on the main task (settings are not thread safe).
struct PendingRequest {
    std::string url;
    std::string error;  // set when the request cannot be made (no appid, ...)
    uint32_t cacheMs = 0;
    std::string day;
};

PendingRequest g_requests[static_cast<int>(OnlineService::Kind::Count)];

std::string todayIso(int offset) {
    const time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    dmd::DateTime dt;
    dt.year = t.tm_year + 1900;
    dt.month = t.tm_mon + 1;
    dt.day = t.tm_mday;
    return dmd::isoDate(dt, offset);
}

}  // namespace

void OnlineService::begin() {
    _lock = xSemaphoreCreateMutex();
    // Core 0: the Arduino loop() runs on core 1.
    xTaskCreatePinnedToCore(taskEntry, "online", kTaskStackBytes, this, 1, &_task, 0);
}

bool OnlineService::fresh(Kind kind, uint32_t nowMs) const {
    const Slot& s = _slots[static_cast<int>(kind)];
    if (s.state != State::Ready) return false;
    switch (kind) {
        case Kind::Current:
        case Kind::Forecast:
            return nowMs - s.fetchedAtMs < g_requests[static_cast<int>(kind)].cacheMs;
        case Kind::Tempo:
            return _tempoDate == todayIso(0);
        default:
            return false;  // geocoding is always asked explicitly
    }
}

void OnlineService::request(Kind kind) {
    const int i = static_cast<int>(kind);
    const dmd::OwmConfig cfg = settings::owmConfig();
    {
        Lock lock(_lock);
        Slot& s = _slots[i];
        if (s.pending) return;
        g_requests[i].cacheMs = static_cast<uint32_t>(cfg.callEveryMin > 0 ? cfg.callEveryMin : 1) * 60000u;
        if (fresh(kind, millis())) return;
        PendingRequest& r = g_requests[i];
        r.error.clear();
        switch (kind) {
            case Kind::Current:
                r.url = dmd::currentWeatherUrl(cfg);
                break;
            case Kind::Forecast:
                r.url = dmd::forecastUrl(cfg);
                break;
            case Kind::Geo:
                r.url = dmd::zipGeocodingUrl(cfg);
                break;
            case Kind::Tempo:
                r.day = todayIso(0);
                r.url = dmd::tempoUrl(r.day, todayIso(1));
                break;
            default:
                return;
        }
        if (kind != Kind::Tempo && !dmd::owmConfigured(cfg)) {
            r.error = "Impossible de recuperer la meteo, appid OpenWeatherMap manquant";
        }
        s.pending = true;
        s.state = State::Loading;
    }
    xTaskNotifyGive(_task);
}

void OnlineService::invalidate() {
    Lock lock(_lock);
    for (auto& s : _slots) {
        if (!s.pending) s.state = State::Idle;
    }
    _tempoDate.clear();
}

OnlineService::State OnlineService::state(Kind kind) const {
    Lock lock(_lock);
    return _slots[static_cast<int>(kind)].state;
}

std::string OnlineService::error(Kind kind) const {
    Lock lock(_lock);
    return _slots[static_cast<int>(kind)].error;
}

bool OnlineService::currentWeather(dmd::CurrentWeather& out) const {
    Lock lock(_lock);
    if (_slots[static_cast<int>(Kind::Current)].state != State::Ready) return false;
    out = _current;
    return true;
}

bool OnlineService::forecast(std::vector<dmd::ForecastItem>& out) const {
    Lock lock(_lock);
    if (_slots[static_cast<int>(Kind::Forecast)].state != State::Ready) return false;
    out = _forecast;
    return true;
}

bool OnlineService::tempo(std::vector<dmd::TempoDay>& out) const {
    Lock lock(_lock);
    if (_slots[static_cast<int>(Kind::Tempo)].state != State::Ready) return false;
    out = _tempo;
    return true;
}

bool OnlineService::geo(dmd::GeoResult& out) const {
    Lock lock(_lock);
    if (_slots[static_cast<int>(Kind::Geo)].state != State::Ready) return false;
    out = _geo;
    return true;
}

void OnlineService::taskEntry(void* self) {
    static_cast<OnlineService*>(self)->run();
}

void OnlineService::run() {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        for (int i = 0; i < static_cast<int>(Kind::Count); ++i) {
            bool pending;
            {
                Lock lock(_lock);
                pending = _slots[i].pending;
            }
            if (pending) fetch(static_cast<Kind>(i));
        }
    }
}

bool OnlineService::httpGet(const std::string& url, std::string& body, std::string& error) {
    if (WiFi.status() != WL_CONNECTED) {
        error = "Pas connecte au web";
        return false;
    }
    // No certificate check: the ESP32 has no CA store configured here (see docs/PORTING.md).
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(client, url.c_str())) {
        error = "URL invalide";
        return false;
    }
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        error = code > 0 ? "Erreur HTTP " + std::to_string(code) : "Serveur injoignable";
        http.end();
        return false;
    }
    // Read at most kMaxBodyBytes, whether the size is announced or the body is chunked.
    BodySink sink(body, kMaxBodyBytes);
    const bool tooBig = http.getSize() > static_cast<int>(kMaxBodyBytes);
    const int rc = tooBig ? 0 : http.writeToStream(&sink);
    http.end();
    if (tooBig || sink.overflowed()) {
        error = "Reponse trop grande";
        return false;
    }
    if (rc < 0) {
        error = "Reponse incomplete";
        return false;
    }
    return true;
}

void OnlineService::fetch(Kind kind) {
    const int i = static_cast<int>(kind);
    PendingRequest req;
    {
        Lock lock(_lock);
        req = g_requests[i];
    }
    std::string body, error = req.error;
    bool ok = error.empty() && httpGet(req.url, body, error);
    if (ok) {
        Lock lock(_lock);
        switch (kind) {
            case Kind::Current:
                ok = dmd::parseCurrentWeather(body.data(), body.size(), _current);
                break;
            case Kind::Forecast:
                ok = dmd::parseForecast(body.data(), body.size(), _forecast);
                break;
            case Kind::Tempo:
                ok = dmd::parseTempo(body.data(), body.size(), _tempo);
                if (ok) _tempoDate = req.day;
                break;
            case Kind::Geo:
                ok = dmd::parseGeo(body.data(), body.size(), _geo);
                break;
            default:
                ok = false;
        }
        if (!ok) error = "Reponse illisible";
    }
    {
        Lock lock(_lock);
        Slot& s = _slots[i];
        s.pending = false;
        s.state = ok ? State::Ready : State::Failed;
        s.error = error;
        s.fetchedAtMs = millis();
    }
    if (ok) {
        ESP_LOGI(TAG, "Fetched %d (%u bytes)", i, static_cast<unsigned>(body.size()));
    } else {
        ESP_LOGW(TAG, "Fetch %d failed: %s", i, error.c_str());
    }
}

// OnlineService (background fetches, cache, errors) and the weather / Tempo / geocoding / perf scenes.

#include "fixtures.h"
#include "fw_test.h"
#include "matrix/Hub75_Matrix.h"
#include "render/OnlineScenes.h"
#include "util/OnlineService.h"
#include "util/Settings.h"

namespace {

const char* kOwm = "https://api.openweathermap.org/data/2.5/weather";
const char* kOwmForecast = "https://api.openweathermap.org/data/2.5/forecast";
const char* kOwmZip = "https://api.openweathermap.org/geo/1.0/zip";
const char* kTempo = "https://www.api-couleur-tempo.fr/";

const char* kCurrentJson =
    R"({"weather":[{"icon":"10d"}],"main":{"temp":12.6},"wind":{"speed":4.12,"deg":230},"name":"Paris"})";
const char* kForecastJson =
    R"({"list":[)"
    R"({"main":{"temp":10.5},"weather":[{"icon":"01d"}],"dt_txt":"2026-10-04 12:00:00"},)"
    R"({"main":{"temp":9},"weather":[{"icon":"02n"}],"dt_txt":"2026-10-04 15:00:00"},)"
    R"({"main":{"temp":8},"weather":[{"icon":"03d"}],"dt_txt":"2026-10-04 18:00:00"},)"
    R"({"main":{"temp":7},"weather":[{"icon":"09d"}],"dt_txt":"2026-10-04 21:00:00"}]})";
const char* kGeoJson = R"({"zip":"75001","name":"Paris","lat":48.8592,"lon":2.3417,"country":"FR"})";
const char* kTempoJson = R"([{"dateJour":"2026-10-04","codeJour":1},{"dateJour":"2026-10-05","codeJour":3}])";

void configureOwm() {
    setSetting("OpenWeatherMap", "appid", "secret-key");
    setSetting("OpenWeatherMap", "lat", "48.85");
    setSetting("OpenWeatherMap", "lon", "2.35");
}

// Ticks a scene every 50 ms, running the online task once, until it ends or maxMs.
int playOnline(dmd::Scene& scene, unsigned long maxMs = 30000) {
    scene.start(millis());
    int ticks = 0;
    for (unsigned long t = 0; t < maxMs; t += 50) {
        fake::advance(50);
        if (t == 200) fake::runTasks();
        if (!scene.tick(millis())) return ticks;
        ++ticks;
    }
    return ticks;
}

}  // namespace

static void testOnlineService() {
    freshBoard();
    OnlineService online;
    online.begin();
    dmd::CurrentWeather w;
    CHECK(!online.currentWeather(w));

    // No API key: fails without a request.
    online.request(OnlineService::Kind::Current);
    CHECK(online.state(OnlineService::Kind::Current) == OnlineService::State::Loading);
    fake::runTasks();
    CHECK(online.state(OnlineService::Kind::Current) == OnlineService::State::Failed);
    CHECK(online.error(OnlineService::Kind::Current).find("appid") != std::string::npos);
    CHECK(fake::httpRequests.empty());

    configureOwm();
    fake::httpReplies[kOwm] = {200, kCurrentJson, false};
    online.request(OnlineService::Kind::Current);
    online.request(OnlineService::Kind::Current);  // already pending: one fetch
    fake::runTasks();
    CHECK(online.state(OnlineService::Kind::Current) == OnlineService::State::Ready);
    CHECK_EQ(fake::httpRequests.size(), 1u);
    CHECK(fake::httpRequests[0].find("appid=secret-key") != std::string::npos);
    CHECK(online.currentWeather(w));
    CHECK_EQ(w.icon, std::string("10d"));

    // Cached for OpenWeatherMap.callevery minutes.
    online.request(OnlineService::Kind::Current);
    fake::runTasks();
    CHECK_EQ(fake::httpRequests.size(), 1u);
    fake::advance(16 * 60000);
    online.request(OnlineService::Kind::Current);
    fake::runTasks();
    CHECK_EQ(fake::httpRequests.size(), 2u);
    online.invalidate();
    online.request(OnlineService::Kind::Current);
    fake::runTasks();
    CHECK_EQ(fake::httpRequests.size(), 3u);

    // Chunked replies are read too.
    fake::httpReplies[kOwmForecast] = {200, kForecastJson, true};
    online.request(OnlineService::Kind::Forecast);
    fake::runTasks();
    std::vector<dmd::ForecastItem> items;
    CHECK(online.forecast(items));
    CHECK_EQ(items.size(), 4u);

    // Tempo is cached until the day changes.
    fake::httpReplies[kTempo] = {200, kTempoJson, false};
    std::vector<dmd::TempoDay> days;
    CHECK(!online.tempo(days));
    online.request(OnlineService::Kind::Tempo);
    fake::runTasks();
    CHECK(online.tempo(days));
    CHECK_EQ(days.size(), 2u);
    const size_t before = fake::httpRequests.size();
    online.request(OnlineService::Kind::Tempo);
    fake::runTasks();
    CHECK_EQ(fake::httpRequests.size(), before);

    // Geocoding is always fetched.
    fake::httpReplies[kOwmZip] = {200, kGeoJson, false};
    dmd::GeoResult geo;
    CHECK(!online.geo(geo));
    online.request(OnlineService::Kind::Geo);
    fake::runTasks();
    CHECK(online.geo(geo));
    CHECK_EQ(geo.name, std::string("Paris"));
}

static void testOnlineErrors() {
    struct Case {
        const char* what;
        fake::HttpReply reply;
        bool hasReply;
        bool wifi;
        const char* error;
    };
    const std::string big(60 * 1024, ' ');
    const Case cases[] = {
        {"HTTP error", {401, "{}", false}, true, true, "Erreur HTTP 401"},
        {"no server", {}, false, true, "Serveur injoignable"},
        {"no Wi-Fi", {200, kCurrentJson, false}, true, false, "Pas connecte au web"},
        {"announced too big", {200, big, false}, true, true, "Reponse trop grande"},
        {"chunked too big", {200, big, true}, true, true, "Reponse trop grande"},
        {"not JSON", {200, "<html>", false}, true, true, "Reponse illisible"},
    };
    for (const Case& c : cases) {
        freshBoard();
        configureOwm();
        fake::wifi.connected = c.wifi;
        if (c.hasReply) fake::httpReplies[kOwm] = c.reply;
        OnlineService online;
        online.begin();
        online.request(OnlineService::Kind::Current);
        fake::runTasks();
        CHECK(online.state(OnlineService::Kind::Current) == OnlineService::State::Failed);
        CHECK_EQ(online.error(OnlineService::Kind::Current), std::string(c.error));
    }
}

static void testWeatherScenes() {
    freshBoard();
    configureOwm();
    fake::httpReplies[kOwm] = {200, kCurrentJson, false};
    fake::httpReplies[kOwmForecast] = {200, kForecastJson, false};
    fake::httpReplies[kTempo] = {200, kTempoJson, false};
    fake::httpReplies[kOwmZip] = {200, kGeoJson, false};
    Hub75_Matrix matrix(128, 32, 1);
    MatrixPanel_I2S_DMA* p = fake::panel();
    OnlineService online;
    online.begin();
    const dmd::Rgb blue(0, 0, 255);

    CurrentWeatherScene current(matrix, online, settings::owmConfig(), blue);
    CHECK(playOnline(current) > 70);  // loading, then one page for seeduring (4 s)
    CHECK(p->litPixels() > 0);
    current.abort();
    CHECK_EQ(p->litPixels(), 0);

    // An uploaded icon set replaces the drawn icons.
    writeBytes("/meteo/10d.png", kPngHalfRed, sizeof(kPngHalfRed));
    online.invalidate();
    CurrentWeatherScene withIcon(matrix, online, settings::owmConfig(), blue);
    playOnline(withIcon);
    CHECK(std::count(p->shown.begin(), p->shown.end(), 0xF800) > 0);

    ForecastScene forecast(matrix, online, settings::owmConfig(), blue);
    CHECK(playOnline(forecast) > 70);

    setSetting("OpenWeatherMap", "prevision", "3");  // no item for the next days: still one page
    online.invalidate();
    ForecastScene longer(matrix, online, settings::owmConfig(), blue);
    CHECK(playOnline(longer) > 70);

    writeBytes("/edfjourstempo/1.png", kPngHalfRed, sizeof(kPngHalfRed));
    TempoScene tempo(matrix, online, 1000, blue);
    CHECK(playOnline(tempo) > 10);

    dmd::GeoResult applied;
    GeoScene geo(matrix, online, blue, [&](const dmd::GeoResult& g) { applied = g; });
    playOnline(geo);
    CHECK_EQ(applied.lat, std::string("48.8592"));
    GeoScene noApply(matrix, online, blue, nullptr);
    playOnline(noApply);
}

static void testOnlineSceneFailures() {
    freshBoard();
    Hub75_Matrix matrix;
    MatrixPanel_I2S_DMA* p = fake::panel();
    OnlineService online;
    online.begin();

    // No API key: the error stays on screen.
    CurrentWeatherScene noKey(matrix, online, settings::owmConfig(), dmd::Rgb(0, 0, 255));
    CHECK(playOnline(noKey) < 10);
    CHECK(p->litPixels() > 0);

    // No answer within 20 s.
    configureOwm();
    fake::httpReplies[kOwm] = {200, kCurrentJson, false};
    OnlineService silent;
    silent.begin();
    CurrentWeatherScene timeout(matrix, silent, settings::owmConfig(), dmd::Rgb(0, 0, 255));
    timeout.start(millis());
    int ticks = 0;
    while (ticks < 1000 && (fake::advance(100), timeout.tick(millis()))) ++ticks;
    CHECK(ticks >= 199 && ticks < 210);
    fake::advance(100);
    CHECK(!timeout.tick(millis()));  // the message stays

    // Data without anything to show.
    fake::httpReplies[kOwmForecast] = {200, R"({"list":[{"main":{"temp":1},"weather":[{"icon":"01d"}],)"
                                            R"("dt_txt":"2020-01-01 12:00:00"}]})",
                                       false};
    ForecastScene empty(matrix, online, settings::owmConfig(), dmd::Rgb(0, 0, 255));
    CHECK(playOnline(empty) < 10);
}

static void testPerfScene() {
    freshBoard();
    Hub75_Matrix matrix;
    MatrixPanel_I2S_DMA* p = fake::panel();
    PerfScene perf(matrix, 500, dmd::Rgb(0, 255, 0));  // raised to 1 s per page
    perf.start(millis());
    int ticks = 0;
    while (ticks < 1000 && (fake::advance(100), perf.tick(millis()))) ++ticks;
    CHECK(ticks >= 28 && ticks <= 30);  // three pages
    CHECK(p->litPixels() > 0);
    fake::wifi.connected = false;
    PerfScene offline(matrix, 1000, dmd::Rgb(0, 255, 0));
    offline.start(millis());
    fake::advance(2500);
    CHECK(offline.tick(millis()));
    offline.abort();
    CHECK_EQ(p->litPixels(), 0);
}

void testOnline() {
    testOnlineService();
    testOnlineErrors();
    testWeatherScenes();
    testOnlineSceneFailures();
    testPerfScene();
}

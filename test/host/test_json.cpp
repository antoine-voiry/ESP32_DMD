// Host tests for src/net/OnlineJson (needs ArduinoJson): make -C test/host json ARDUINOJSON=<path>/src
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "net/OnlineJson.h"

TEST_MAIN_COUNTERS;

using namespace dmd;

static bool parse(bool (*fn)(const char*, size_t, CurrentWeather&), const char* s, CurrentWeather& w) {
    return fn(s, std::strlen(s), w);
}

int main() {
    // Trimmed real answers.
    const char* current =
        R"({"coord":{"lon":2.35,"lat":48.85},"weather":[{"id":500,"main":"Rain","description":"pluie","icon":"10d"}],)"
        R"("main":{"temp":12.6,"feels_like":11.9,"humidity":80},"wind":{"speed":4.12,"deg":230},"name":"Paris"})";
    CurrentWeather w;
    CHECK(parse(parseCurrentWeather, current, w));
    CHECK(w.icon == "10d");
    CHECK(w.temp > 12.5f && w.temp < 12.7f);
    CHECK(w.windDeg == 230.0f);
    CHECK(w.windSpeed > 4.1f && w.windSpeed < 4.2f);
    CHECK(!parse(parseCurrentWeather, R"({"cod":401,"message":"Invalid API key"})", w));
    CHECK(!parse(parseCurrentWeather, "not json", w));

    const char* forecast =
        R"({"cod":"200","cnt":2,"list":[)"
        R"({"dt":1,"main":{"temp":10.5},"weather":[{"icon":"01d"}],"dt_txt":"2026-10-03 12:00:00"},)"
        R"({"dt":2,"main":{"temp":8},"weather":[{"icon":"02n"}],"dt_txt":"2026-10-04 00:00:00"}],"city":{"name":"Paris"}})";
    std::vector<ForecastItem> items;
    CHECK(parseForecast(forecast, std::strlen(forecast), items));
    CHECK(items.size() == 2);
    CHECK(items[1].dtTxt == "2026-10-04 00:00:00");
    CHECK(items[1].icon == "02n");
    CHECK(items[1].temp == 8.0f);
    CHECK(forecastPages(items, {"2026-10-03"}).size() == 1);
    CHECK(!parseForecast("{}", 2, items));

    const char* geo = R"({"zip":"75001","name":"Paris","lat":48.8592,"lon":2.3417,"country":"FR"})";
    GeoResult g;
    CHECK(parseGeo(geo, std::strlen(geo), g));
    CHECK(g.name == "Paris");
    CHECK(g.lat == "48.8592");
    CHECK(g.lon == "2.3417");
    const char* notFound = R"({"cod":"404","message":"not found"})";
    CHECK(!parseGeo(notFound, std::strlen(notFound), g));

    const char* tempo =
        R"([{"dateJour":"2026-10-03","codeJour":1,"periode":"2026-2027"},{"dateJour":"2026-10-04","codeJour":0}])";
    std::vector<TempoDay> days;
    CHECK(parseTempo(tempo, std::strlen(tempo), days));
    CHECK(days.size() == 2);
    CHECK(days[0].code == 1);
    CHECK(days[1].date == "2026-10-04");
    CHECK(!parseTempo("[]", 2, days));

    return TEST_REPORT();
}

#ifndef DMD_NET_ONLINE_JSON_H
#define DMD_NET_ONLINE_JSON_H

// JSON -> structs for the OpenWeatherMap and api-couleur-tempo.fr responses (ArduinoJson).
// Kept separate from src/core because it needs ArduinoJson; host-tested in CI (test/host/test_json.cpp).

#include <cstddef>
#include <string>
#include <vector>

#include "core/Online.h"

namespace dmd {

bool parseCurrentWeather(const char* json, size_t length, CurrentWeather& out);
bool parseForecast(const char* json, size_t length, std::vector<ForecastItem>& out);
bool parseGeo(const char* json, size_t length, GeoResult& out);
bool parseTempo(const char* json, size_t length, std::vector<TempoDay>& out);

}  // namespace dmd

#endif

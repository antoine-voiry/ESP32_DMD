#include "OnlineJson.h"

#include <ArduinoJson.h>

#include <cstdio>

namespace dmd {

namespace {

std::string numberText(JsonVariantConst v) {
    if (v.is<const char*>()) return v.as<const char*>();
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%.4f", v.as<double>());
    return buf;
}

}  // namespace

bool parseCurrentWeather(const char* json, size_t length, CurrentWeather& out) {
    JsonDocument filter;
    filter["main"]["temp"] = true;
    filter["weather"][0]["icon"] = true;
    filter["wind"]["deg"] = true;
    filter["wind"]["speed"] = true;
    JsonDocument doc;
    if (deserializeJson(doc, json, length, DeserializationOption::Filter(filter))) return false;
    if (!doc["main"]["temp"].is<float>()) return false;
    out.temp = doc["main"]["temp"].as<float>();
    out.icon = doc["weather"][0]["icon"] | "";
    out.windDeg = doc["wind"]["deg"] | 0.0f;
    out.windSpeed = doc["wind"]["speed"] | 0.0f;
    return true;
}

bool parseForecast(const char* json, size_t length, std::vector<ForecastItem>& out) {
    // The full answer is ~16 KB; keep only what the screens use.
    JsonDocument filter;
    filter["list"][0]["dt_txt"] = true;
    filter["list"][0]["main"]["temp"] = true;
    filter["list"][0]["weather"][0]["icon"] = true;
    JsonDocument doc;
    if (deserializeJson(doc, json, length, DeserializationOption::Filter(filter))) return false;
    JsonArrayConst list = doc["list"].as<JsonArrayConst>();
    if (list.isNull()) return false;
    out.clear();
    for (JsonObjectConst item : list) {
        ForecastItem f;
        f.dtTxt = item["dt_txt"] | "";
        f.temp = item["main"]["temp"] | 0.0f;
        f.icon = item["weather"][0]["icon"] | "";
        if (!f.dtTxt.empty()) out.push_back(f);
    }
    return true;
}

bool parseGeo(const char* json, size_t length, GeoResult& out) {
    JsonDocument doc;
    if (deserializeJson(doc, json, length)) return false;
    if (doc["lat"].isNull() || doc["lon"].isNull()) return false;
    out.zip = doc["zip"] | "";
    out.name = doc["name"] | "";
    out.country = doc["country"] | "";
    out.lat = numberText(doc["lat"]);
    out.lon = numberText(doc["lon"]);
    return true;
}

bool parseTempo(const char* json, size_t length, std::vector<TempoDay>& out) {
    JsonDocument doc;
    if (deserializeJson(doc, json, length)) return false;
    JsonArrayConst days = doc.as<JsonArrayConst>();
    if (days.isNull()) return false;
    out.clear();
    for (JsonObjectConst d : days) {
        TempoDay t;
        t.date = d["dateJour"] | "";
        t.code = d["codeJour"] | 0;
        out.push_back(t);
    }
    return !out.empty();
}

}  // namespace dmd

#ifndef DMD_CORE_ONLINE_H
#define DMD_CORE_ONLINE_H

// OpenWeatherMap and EDF Tempo logic from DMDRenderer.py (RenderMeteoInTime,
// RenderMeteoPrevisionnelle, RenderEDFJoursTempo, ZipPostCodeGeocoding, FindLatLonCityname),
// without networking or JSON so it can be unit tested on the host.

#include <string>
#include <vector>

#include "Canvas.h"
#include "Clock.h"
#include "TextUtil.h"

namespace dmd {

// [OpenWeatherMap] settings with the Raspy2DMD defaults.
struct OwmConfig {
    std::string appid = "0";  // "0" = not configured
    std::string lat = "0.0";
    std::string lon = "0.0";
    std::string zipcode = "0";
    std::string countrycode = "0";
    std::string units = "metric";
    std::string lang = "fr";
    int callEveryMin = 15;  // cache duration
    int seeDuringSec = 4;   // how long each screen stays
    int prevision = 1;      // forecast days
};

bool owmConfigured(const OwmConfig& c);

std::string urlEncode(const std::string& s);
std::string currentWeatherUrl(const OwmConfig& c);
std::string forecastUrl(const OwmConfig& c);
std::string zipGeocodingUrl(const OwmConfig& c);
// api-couleur-tempo.fr, dates as YYYY-MM-DD.
std::string tempoUrl(const std::string& today, const std::string& tomorrow);

struct CurrentWeather {
    float temp = 0;
    std::string icon;  // OWM icon code, e.g. "10d"
    float windDeg = 0;
    float windSpeed = 0;  // as returned (m/s for metric/standard, mph for imperial)
};

struct ForecastItem {
    std::string dtTxt;  // "2026-10-03 21:00:00"
    std::string icon;
    float temp = 0;
};

struct ForecastSlot {
    std::string time;  // "21:00"
    std::string icon;
    int temp = 0;
};

struct ForecastPage {
    std::string date;  // "03/10"
    std::vector<ForecastSlot> slots;
};

struct GeoResult {
    std::string zip, name, country, lat, lon;
};

struct TempoDay {
    std::string date;  // "2026-10-03"
    int code = 0;      // 1 blue, 2 white, 3 red, 0 not known yet
};

// Python's round(): halves go to the even neighbour.
long roundHalfEven(double v);

// "YYYY-MM-DD" for today + offsetDays (handles month/year ends and leap years).
std::string isoDate(const DateTime& today, int offsetDays);

// The `prevision` dates shown by the forecast, starting today.
std::vector<std::string> forecastDates(const DateTime& today, int days);

// Groups the 3-hourly forecast by date (00:00 belongs to the previous day, as in the original)
// and splits each day into pages of at most perPage slots.
std::vector<ForecastPage> forecastPages(const std::vector<ForecastItem>& items,
                                        const std::vector<std::string>& dates, size_t perPage = 4);

std::string formatTemperature(float temp);                               // "12°C"
std::string formatWind(float speed, const std::string& units);           // "18km/h" ("mph" for imperial)
std::string formatTempoDate(const std::string& isoDate);                 // "03/10/26"

// Weather icon families from the OWM icon code (01 clear ... 50 mist).
enum class WeatherKind { Clear, FewClouds, Clouds, Showers, Rain, Thunder, Snow, Mist, Unknown };
WeatherKind weatherKind(const std::string& icon);
bool isNightIcon(const std::string& icon);

Rgb tempoColor(int code);
std::string tempoLabel(int code);  // BLEU / BLANC / ROUGE / ?

// Procedural replacements for the Pi's icon PNGs, drawn in a size x size box at (x, y).
void drawWeatherIcon(Canvas& canvas, WeatherKind kind, bool night, int x, int y, int size);
// Arrow showing where the wind blows to (meteorological degrees = where it comes from).
void drawWindArrow(Canvas& canvas, int cx, int cy, int length, float degrees, uint16_t color);
// End point of that arrow's tip (for tests and layout).
void windArrowTip(int cx, int cy, int length, float degrees, int& tx, int& ty);

}  // namespace dmd

#endif

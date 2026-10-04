#include "Online.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace dmd {

bool owmConfigured(const OwmConfig& c) {
    return !c.appid.empty() && c.appid != "0";
}

std::string urlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

namespace {

std::string owmQuery(const OwmConfig& c) {
    return "lat=" + urlEncode(c.lat) + "&lon=" + urlEncode(c.lon) + "&appid=" + urlEncode(c.appid) +
           "&units=" + urlEncode(c.units) + "&lang=" + urlEncode(c.lang);
}

bool isLeap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

int daysInMonth(int y, int m) {
    static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m == 2 && isLeap(y) ? 29 : kDays[(m - 1) % 12];
}

}  // namespace

std::string currentWeatherUrl(const OwmConfig& c) {
    return "https://api.openweathermap.org/data/2.5/weather?" + owmQuery(c);
}

std::string forecastUrl(const OwmConfig& c) {
    return "https://api.openweathermap.org/data/2.5/forecast?" + owmQuery(c);
}

std::string zipGeocodingUrl(const OwmConfig& c) {
    return "https://api.openweathermap.org/geo/1.0/zip?zip=" + urlEncode(c.zipcode) + "," + urlEncode(c.countrycode) +
           "&appid=" + urlEncode(c.appid);
}

std::string tempoUrl(const std::string& today, const std::string& tomorrow) {
    return "https://www.api-couleur-tempo.fr/api/joursTempo?dateJour%5B%5D=" + urlEncode(today) +
           "&dateJour%5B%5D=" + urlEncode(tomorrow);
}

long roundHalfEven(double v) {
    const double fl = std::floor(v);
    const double diff = v - fl;
    long r = static_cast<long>(fl);
    if (diff > 0.5 || (diff == 0.5 && (r % 2 != 0))) {
        ++r;
    }
    return r;
}

std::string isoDate(const DateTime& today, int offsetDays) {
    int y = today.year, m = today.month, d = today.day;
    while (offsetDays > 0) {
        if (++d > daysInMonth(y, m)) {
            d = 1;
            if (++m > 12) {
                m = 1;
                ++y;
            }
        }
        --offsetDays;
    }
    while (offsetDays < 0) {
        if (--d < 1) {
            if (--m < 1) {
                m = 12;
                --y;
            }
            d = daysInMonth(y, m);
        }
        ++offsetDays;
    }
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
    return buf;
}

std::vector<std::string> forecastDates(const DateTime& today, int days) {
    std::vector<std::string> out;
    for (int i = 0; i < (days < 1 ? 1 : days); ++i) {
        out.push_back(isoDate(today, i));
    }
    return out;
}

std::vector<ForecastPage> forecastPages(const std::vector<ForecastItem>& items,
                                        const std::vector<std::string>& dates, size_t perPage) {
    if (perPage == 0) perPage = 1;
    std::vector<ForecastPage> pages;
    for (const auto& date : dates) {
        std::vector<ForecastSlot> slots;
        for (const auto& item : items) {
            if (item.dtTxt.size() < 16) continue;
            std::string day = item.dtTxt.substr(0, 10);
            const std::string time = item.dtTxt.substr(11, 5);
            if (time == "00:00") {
                // Midnight closes the previous day.
                DateTime dt;
                dt.year = std::atoi(day.substr(0, 4).c_str());
                dt.month = std::atoi(day.substr(5, 2).c_str());
                dt.day = std::atoi(day.substr(8, 2).c_str());
                day = isoDate(dt, -1);
            }
            if (day != date) continue;
            ForecastSlot slot;
            slot.time = time;
            slot.icon = item.icon;
            slot.temp = static_cast<int>(roundHalfEven(item.temp));
            slots.push_back(slot);
        }
        const std::string label = date.size() == 10 ? date.substr(8, 2) + "/" + date.substr(5, 2) : date;
        for (size_t i = 0; i < slots.size(); i += perPage) {
            ForecastPage page;
            page.date = label;
            for (size_t j = i; j < slots.size() && j < i + perPage; ++j) {
                page.slots.push_back(slots[j]);
            }
            pages.push_back(page);
        }
    }
    return pages;
}

std::string formatTemperature(float temp) {
    return std::to_string(roundHalfEven(temp)) + "\xC2\xB0" "C";
}

std::string formatWind(float speed, const std::string& units) {
    if (units == "imperial") {
        return std::to_string(roundHalfEven(speed)) + "mph";
    }
    // The original printed the raw m/s value followed by "km/h"; convert it.
    return std::to_string(roundHalfEven(speed * 3.6)) + "km/h";
}

std::string formatTempoDate(const std::string& iso) {
    if (iso.size() != 10) return iso;
    return iso.substr(8, 2) + "/" + iso.substr(5, 2) + "/" + iso.substr(2, 2);
}

WeatherKind weatherKind(const std::string& icon) {
    if (icon.size() < 2) return WeatherKind::Unknown;
    const int code = std::atoi(icon.substr(0, 2).c_str());
    switch (code) {
        case 1: return WeatherKind::Clear;
        case 2: return WeatherKind::FewClouds;
        case 3:
        case 4: return WeatherKind::Clouds;
        case 9: return WeatherKind::Showers;
        case 10: return WeatherKind::Rain;
        case 11: return WeatherKind::Thunder;
        case 13: return WeatherKind::Snow;
        case 50: return WeatherKind::Mist;
        default: return WeatherKind::Unknown;
    }
}

bool isNightIcon(const std::string& icon) {
    return !icon.empty() && icon.back() == 'n';
}

Rgb tempoColor(int code) {
    switch (code) {
        case 1: return Rgb(0, 80, 255);
        case 2: return Rgb(255, 255, 255);
        case 3: return Rgb(255, 0, 0);
        default: return Rgb(80, 80, 80);
    }
}

std::string tempoLabel(int code) {
    switch (code) {
        case 1: return "BLEU";
        case 2: return "BLANC";
        case 3: return "ROUGE";
        default: return "?";
    }
}

namespace {

const uint16_t kSun = rgb565(255, 200, 0);
const uint16_t kMoon = rgb565(200, 200, 160);
const uint16_t kCloud = rgb565(200, 200, 210);
const uint16_t kDarkCloud = rgb565(110, 110, 130);
const uint16_t kRain = rgb565(40, 120, 255);
const uint16_t kBolt = rgb565(255, 230, 0);
const uint16_t kSnow = rgb565(255, 255, 255);
const uint16_t kMist = rgb565(150, 150, 150);

void drawCloud(Canvas& c, int x, int y, int s, uint16_t color) {
    // Three bumps and a flat base, scaled to the box.
    const int r = s / 5 > 1 ? s / 5 : 1;
    c.fillCircle(x + s * 3 / 10, y + s * 6 / 10, r, color);
    c.fillCircle(x + s / 2, y + s * 5 / 10, r + 1, color);
    c.fillCircle(x + s * 7 / 10, y + s * 6 / 10, r, color);
    c.fillRect(x + s * 3 / 10, y + s * 6 / 10, s * 4 / 10 + 1, r + 1, color);
}

void drawSunOrMoon(Canvas& c, bool night, int cx, int cy, int r) {
    if (night) {
        c.fillCircle(cx, cy, r, kMoon);
        c.fillCircle(cx + r / 2 + 1, cy - r / 3, r, 0);  // crescent
        return;
    }
    c.fillCircle(cx, cy, r, kSun);
    for (int i = 0; i < 8; ++i) {
        const double a = i * 3.14159265 / 4;
        c.drawLine(cx + static_cast<int>(std::lround((r + 2) * std::cos(a))),
                   cy + static_cast<int>(std::lround((r + 2) * std::sin(a))),
                   cx + static_cast<int>(std::lround((r + 3) * std::cos(a))),
                   cy + static_cast<int>(std::lround((r + 3) * std::sin(a))), kSun);
    }
}

void drawDrops(Canvas& c, int x, int y, int s, int n, uint16_t color) {
    for (int i = 0; i < n; ++i) {
        const int dx = x + s * (2 + i * 2) / 10;
        c.drawLine(dx, y + s * 8 / 10, dx - 1, y + s * 9 / 10 + 1, color);
    }
}

}  // namespace

void drawWeatherIcon(Canvas& c, WeatherKind kind, bool night, int x, int y, int s) {
    const int cx = x + s / 2, cy = y + s / 2;
    switch (kind) {
        case WeatherKind::Clear:
            drawSunOrMoon(c, night, cx, cy, s / 4);
            break;
        case WeatherKind::FewClouds:
            drawSunOrMoon(c, night, x + s * 6 / 10, y + s * 4 / 10, s / 5);
            drawCloud(c, x, y + s / 10, s, kCloud);
            break;
        case WeatherKind::Clouds:
            drawCloud(c, x + s / 8, y - s / 10, s * 3 / 4, kDarkCloud);
            drawCloud(c, x, y, s, kCloud);
            break;
        case WeatherKind::Showers:
        case WeatherKind::Rain:
            if (kind == WeatherKind::Rain) drawSunOrMoon(c, night, x + s * 7 / 10, y + s * 3 / 10, s / 6);
            drawCloud(c, x, y - s / 8, s, kDarkCloud);
            drawDrops(c, x, y, s, kind == WeatherKind::Rain ? 3 : 4, kRain);
            break;
        case WeatherKind::Thunder:
            drawCloud(c, x, y - s / 8, s, kDarkCloud);
            c.drawLine(cx + 1, cy + s / 8, cx - 2, cy + s * 3 / 10, kBolt);
            c.drawLine(cx - 2, cy + s * 3 / 10, cx + 2, cy + s * 3 / 10, kBolt);
            c.drawLine(cx + 2, cy + s * 3 / 10, cx - 1, cy + s / 2, kBolt);
            break;
        case WeatherKind::Snow:
            drawCloud(c, x, y - s / 8, s, kCloud);
            for (int i = 0; i < 3; ++i) {
                const int fx = x + s * (3 + i * 2) / 10, fy = y + s * 85 / 100;
                c.set(fx, fy, kSnow);
                c.set(fx - 1, fy, kSnow);
                c.set(fx + 1, fy, kSnow);
                c.set(fx, fy - 1, kSnow);
                c.set(fx, fy + 1, kSnow);
            }
            break;
        case WeatherKind::Mist:
            for (int i = 0; i < 4; ++i) {
                const int ly = y + s * (3 + i * 2) / 10;
                c.drawLine(x + s / 8 + (i % 2) * 2, ly, x + s - s / 8 - ((i + 1) % 2) * 2, ly, kMist);
            }
            break;
        case WeatherKind::Unknown:
        default:
            c.drawRect(x + s / 4, y + s / 4, s / 2, s / 2, kMist);
            break;
    }
}

void windArrowTip(int cx, int cy, int length, float degrees, int& tx, int& ty) {
    // Wind from `degrees` blows towards degrees + 180; screen y grows downwards.
    const double a = (degrees + 180.0) * 3.14159265358979 / 180.0;
    const double half = length / 2.0;
    tx = cx + static_cast<int>(std::lround(half * std::sin(a)));
    ty = cy - static_cast<int>(std::lround(half * std::cos(a)));
}

void drawWindArrow(Canvas& c, int cx, int cy, int length, float degrees, uint16_t color) {
    int tx, ty;
    windArrowTip(cx, cy, length, degrees, tx, ty);
    const int bx = 2 * cx - tx, by = 2 * cy - ty;  // tail, opposite the tip
    c.drawLine(bx, by, tx, ty, color);
    // Head: two short strokes back from the tip, +-35 degrees.
    const double a = std::atan2(static_cast<double>(by - ty), static_cast<double>(bx - tx));
    const double head = length / 3.0 > 2 ? length / 3.0 : 2;
    for (int sgn = -1; sgn <= 1; sgn += 2) {
        const double ha = a + sgn * 0.6;
        c.drawLine(tx, ty, tx + static_cast<int>(std::lround(head * std::cos(ha))),
                   ty + static_cast<int>(std::lround(head * std::sin(ha))), color);
    }
}

}  // namespace dmd

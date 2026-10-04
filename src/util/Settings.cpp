#include "Settings.h"

#include "ConfigHelper.h"
#include "core/Clock.h"
#include "core/Media.h"
#include "core/TextUtil.h"

namespace settings {

namespace {

dmd::Rgb rgbSetting(const char* section, const char* key, dmd::Rgb fallback) {
    dmd::Rgb c;
    return dmd::parseRgb(ConfigHelper::getInstance().getSetting(section, key, ""), c) ? c : fallback;
}

long intSetting(const char* section, const char* key, long fallback) {
    return ConfigHelper::getInstance().getSettingInt(section, key, fallback);
}

std::string strSetting(const char* section, const char* key, const char* fallback) {
    return ConfigHelper::getInstance().getSetting(section, key, fallback);
}

}  // namespace

ClockSpec clockSpec() {
    ClockSpec spec;
    spec.mode = static_cast<int>(intSetting("ClockRenderer", "showing_datehours", 2));
    spec.dateMs = static_cast<uint32_t>(intSetting("ClockRenderer", "timeShow_Date", 2)) * 1000u;
    spec.hoursMs = static_cast<uint32_t>(intSetting("ClockRenderer", "timeShow_Hours", 4)) * 1000u;
    spec.dateFormat = strSetting("ClockRenderer", "format_date", "%d %b %Y");
    spec.hourFormat = strSetting("ClockRenderer", "format_hours", "%H:%M:%S");
    spec.lang = dmd::languageFromLocale(strSetting("ClockRenderer", "format_affichage", "fr_FR"));
    spec.fg = rgbSetting("ClockRenderer", "defaultfontcolor_clock", dmd::Rgb{0, 0, 255});
    spec.shadow = rgbSetting("ClockRenderer", "defaultfontcolor_clockshadow", dmd::Rgb{255, 0, 0});
    spec.maxFontPx = static_cast<int>(intSetting("TextRenderer", "maxfontsize", 30));
    const std::string pattern = strSetting("ClockRenderer", "clockBackgroundImage", "OldGame.png");
    spec.pattern = pattern.empty() ? "" : std::string(dmd::media::kPatterns) + "/" + pattern;
    return spec;
}

std::string timezone() {
    return dmd::posixTimezone(strSetting("ClockRenderer", "timezone", "Europe/Paris"));
}

TextStyle textStyle() {
    TextStyle style;
    style.fg = rgbSetting("TextRenderer", "defaultfontcolor", dmd::Rgb{0, 0, 255});
    style.bg = rgbSetting("TextRenderer", "picturebackgroundcolor", dmd::Rgb{0, 0, 0});
    long maxChars = intSetting("TextRenderer", "maxcharacter", 22);
    style.maxCharsPerLine = maxChars > 0 ? static_cast<size_t>(maxChars) : 22;
    style.maxFontPx = static_cast<int>(intSetting("TextRenderer", "maxfontsize", 30));
    return style;
}

bool centerImages() {
    return intSetting("DMDRenderer", "center_images", 1) != 0;
}

dmd::OwmConfig owmConfig() {
    dmd::OwmConfig c;
    c.appid = strSetting("OpenWeatherMap", "appid", "0");
    c.lat = strSetting("OpenWeatherMap", "lat", "0.0");
    c.lon = strSetting("OpenWeatherMap", "lon", "0.0");
    c.zipcode = strSetting("OpenWeatherMap", "zipcode", "0");
    c.countrycode = strSetting("OpenWeatherMap", "countrycode", "0");
    c.units = strSetting("OpenWeatherMap", "units", "metric");
    c.lang = strSetting("OpenWeatherMap", "lang", "fr");
    c.callEveryMin = static_cast<int>(intSetting("OpenWeatherMap", "callevery", 15));
    c.seeDuringSec = static_cast<int>(intSetting("OpenWeatherMap", "seeduring", 4));
    c.prevision = static_cast<int>(intSetting("OpenWeatherMap", "prevision", 1));
    return c;
}

std::string scrollOrder() {
    return strSetting("Running", "scrollOrder", "1,T");
}

bool standalone() {
    return intSetting("Running", "standalone", 0) == 1;
}

bool showWebAddress() {
    return intSetting("Running", "default", 1) != 0;
}

uint32_t attractAfterMs() {
    long seconds = intSetting("Running", "attract_mode", 0);
    return seconds > 0 ? static_cast<uint32_t>(seconds) * 1000u : 0;
}

}  // namespace settings

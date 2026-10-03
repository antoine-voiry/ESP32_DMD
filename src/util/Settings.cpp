#include "Settings.h"

#include "ConfigHelper.h"
#include "core/Clock.h"
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
    spec.maxFontPx = static_cast<int>(intSetting("TextRenderer", "maxfontsize", 32));
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
    style.maxFontPx = static_cast<int>(intSetting("TextRenderer", "maxfontsize", 32));
    return style;
}

std::string scrollOrder() {
    return strSetting("Running", "scrollOrder", "1,T");
}

uint32_t attractAfterMs() {
    long seconds = intSetting("Running", "attract_mode", 0);
    return seconds > 0 ? static_cast<uint32_t>(seconds) * 1000u : 0;
}

}  // namespace settings

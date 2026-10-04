#include "ConfigSchema.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace dmd {

const std::vector<SettingDef>& configSchema() {
    // GetConfig() order and SetDefaultConfig() values. Differences, all deliberate:
    // led_chain defaults to 1 (one 64x32 panel) and picturewidth to 64; maxfontsize keeps 30.
    static const std::vector<SettingDef> kSchema = {
        {"DMDRenderer", "cols", "64", true},
        {"DMDRenderer", "rows", "32", true},
        {"DMDRenderer", "led_chain", "1", true},
        {"DMDRenderer", "vertical_parallel_chain", "1", false},
        {"DMDRenderer", "gpio_slowdown", "4", false},
        {"DMDRenderer", "pwm_lsb_nanoseconds", "130", false},
        {"DMDRenderer", "limit_refresh_rate_hz", "180", false},
        {"DMDRenderer", "picturewidth", "64", false},
        {"DMDRenderer", "pictureheight", "32", false},
        {"DMDRenderer", "hardware_mapping", "regular", false},
        {"DMDRenderer", "pwm_bits", "11", false},
        {"DMDRenderer", "scan_mode", "0", false},
        {"DMDRenderer", "rgb_mode", "RGB", false},
        {"DMDRenderer", "brightness", "90", true},
        {"DMDRenderer", "brightnesshours",
         "90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90", true},
        {"DMDRenderer", "led_row_addr_type", "0", false},
        {"DMDRenderer", "center_images", "1", true},
        {"DMDRenderer", "multi_hub", "0", false},
        {"DMDRenderer", "interrupteur", "0", false},

        {"Directory", "fontsttf", "/Medias/Fonts/", false},
        {"Directory", "gifs", "/Medias/Gifs/", false},
        {"Directory", "videos", "/Medias/Videos/", false},
        {"Directory", "images", "/Medias/Images/", false},
        {"Directory", "scores", "/Medias/Scores/", false},
        {"Directory", "textes", "/Medias/Textes/", false},
        {"Directory", "specialsmoves", "/Medias/SpecialsMoves/", false},
        {"Directory", "patterns", "/Medias/Patterns/", false},
        {"Directory", "meteo", "/Medias/Meteo/", false},
        {"Directory", "sounds", "/Medias/Sounds/", false},
        {"Directory", "perfvisualizer", "/Medias/PerfVisualizer", false},
        {"Directory", "edfjourstempo", "/Medias/EDFJoursTempo", false},

        {"TextRenderer", "defaultfont", "Impact.ttf", false},
        {"TextRenderer", "defaultfontcolor", "0,0,255", true},
        {"TextRenderer", "pictureverticalmargin", "0", false},
        {"TextRenderer", "picturebackgroundcolor", "0,0,0", true},
        {"TextRenderer", "maxfontsize", "30", true},
        {"TextRenderer", "maxcharacter", "22", true},

        {"ClockRenderer", "defaultfont_clock", "Impact.ttf", false},
        {"ClockRenderer", "defaultfontcolor_clock", "0,0,255", true},
        {"ClockRenderer", "defaultfontcolor_clockshadow", "255,0,0", true},
        {"ClockRenderer", "showing_datehours", "2", true},
        {"ClockRenderer", "clockBackgroundImage", "OldGame.png", true},
        {"ClockRenderer", "posX_Date", "4.0", false},
        {"ClockRenderer", "posY_Date", "0.0", false},
        {"ClockRenderer", "sizeFont_Date", "24", false},
        {"ClockRenderer", "posX_Hours", "21.0", false},
        {"ClockRenderer", "posY_Hours", "0.0", false},
        {"ClockRenderer", "sizeFont_Hours", "26", false},
        {"ClockRenderer", "timeShow_Date", "2", true},
        {"ClockRenderer", "timeShow_Hours", "4", true},
        {"ClockRenderer", "decal_horaire", "0", false},
        {"ClockRenderer", "format_affichage", "fr_FR", true},
        {"ClockRenderer", "format_date", "%d %b %Y", true},
        {"ClockRenderer", "format_hours", "%H:%M:%S", true},
        {"ClockRenderer", "timezone", "Europe/Paris", true},
        {"ClockRenderer", "rpitime", "", false},
        {"ClockRenderer", "websynch", "0", false},

        {"Running", "standalone", "0", true},
        {"Running", "default", "1", true},
        {"Running", "attract_mode", "0", true},
        {"Running", "raspydarts", "raspydarts.local", false},
        {"Running", "raspydartscanal", "raspydarts/dmd", true},
        {"Running", "resptoraspydarts", "0", true},
        {"Running", "scrollOrder", "1,T", true},
        {"Running", "checkforupdate", "1", false},
        {"Running", "activehdmi", "0", false},

        {"OpenWeatherMap", "callevery", "15", true},
        {"OpenWeatherMap", "seeduring", "4", true},
        {"OpenWeatherMap", "lat", "0.0", true},
        {"OpenWeatherMap", "lon", "0.0", true},
        {"OpenWeatherMap", "cityname", "Inconnu", true},
        {"OpenWeatherMap", "zipcode", "0", true},
        {"OpenWeatherMap", "countrycode", "0", true},
        {"OpenWeatherMap", "statecode", "0", false},
        {"OpenWeatherMap", "appid", "0", true},
        {"OpenWeatherMap", "units", "metric", true},
        {"OpenWeatherMap", "lang", "fr", true},
        {"OpenWeatherMap", "prevision", "1", true},
        {"OpenWeatherMap", "lastcall", "0001-01-01", false},
        {"OpenWeatherMap", "lastcallintime", "0001-01-01 00:00", false},

        {"Sound", "volume", "50", false},
        {"Sound", "output", "local", false},
    };
    return kSchema;
}

std::string normaliseKey(const std::string& key) {
    std::string out = key;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

const SettingDef* findSetting(const std::string& section, const std::string& key) {
    const std::string k = normaliseKey(key);
    for (const auto& def : configSchema()) {
        if (section == def.section && k == normaliseKey(def.key)) return &def;
    }
    return nullptr;
}

std::vector<std::string> receipconfLines(const std::function<std::string(const SettingDef&)>& valueOf) {
    std::vector<std::string> lines;
    for (const auto& def : configSchema()) {
        lines.push_back(std::string(def.section) + ":" + def.key + ":" + valueOf(def));
    }
    return lines;
}

bool needsRestart(const std::string& section, const std::string& key) {
    const std::string k = normaliseKey(key);
    if (section == "DMDRenderer") return k == "cols" || k == "rows" || k == "led_chain";
    if (section == "Running") return k == "standalone";
    return false;
}

bool allowedInStandalone(const std::string& action) {
    static const char* const kAllowed[] = {"rebt",  "shutdwn",    "excludeFolder", "excludeFile", "owmzc",
                                           "fllcn", "meteo",      "meteoPrevi",    "edfJoursTempo", "perf",
                                           "receipconf", "rldconf", "conf",        "msg"};
    for (const char* a : kAllowed) {
        if (action == a) return true;
    }
    return false;
}

namespace {

int parseClamped(const std::string& s, int fallback, int lo, int hi) {
    char* end = nullptr;
    const long v = std::strtol(s.c_str(), &end, 10);
    if (s.empty() || end == s.c_str() || *end != '\0') return fallback;
    return v < lo ? lo : (v > hi ? hi : static_cast<int>(v));
}

}  // namespace

PanelGeometry panelGeometry(const std::string& cols, const std::string& rows, const std::string& chain,
                            const PanelGeometry& defaults) {
    PanelGeometry g;
    g.cols = parseClamped(cols, defaults.cols, 16, 128);
    g.rows = parseClamped(rows, defaults.rows, 16, 64);
    g.chain = parseClamped(chain, defaults.chain, 1, 4);
    return g;
}

}  // namespace dmd

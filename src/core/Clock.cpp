#include "Clock.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace dmd {

namespace {

// glibc fr_FR names, as Python's strftime printed them on the Pi (accents are stripped later).
const char* const kFrMonthsAbbr[] = {"janv.", "f\xC3\xA9vr.", "mars", "avr.", "mai", "juin",
                                     "juil.", "ao\xC3\xBBt", "sept.", "oct.", "nov.", "d\xC3\xA9" "c."};
const char* const kFrMonths[] = {"janvier", "f\xC3\xA9vrier", "mars", "avril", "mai", "juin",
                                 "juillet", "ao\xC3\xBBt", "septembre", "octobre", "novembre",
                                 "d\xC3\xA9" "cembre"};
const char* const kFrDaysAbbr[] = {"dim.", "lun.", "mar.", "mer.", "jeu.", "ven.", "sam."};
const char* const kFrDays[] = {"dimanche", "lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi"};

const char* const kEnMonthsAbbr[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char* const kEnMonths[] = {"January", "February", "March", "April", "May", "June", "July",
                                 "August", "September", "October", "November", "December"};
const char* const kEnDaysAbbr[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* const kEnDays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

std::string number(int value, int width, bool pad) {
    char buf[16];
    if (pad) {
        std::snprintf(buf, sizeof(buf), "%0*d", width, value);
    } else {
        std::snprintf(buf, sizeof(buf), "%d", value);
    }
    return buf;
}

int clampIndex(int v, int n) {
    return v < 0 ? 0 : (v >= n ? n - 1 : v);
}

}  // namespace

std::string formatDateTime(const std::string& format, const DateTime& dt, const std::string& lang) {
    const bool fr = lang != "en";
    const int m = clampIndex(dt.month - 1, 12);
    const int wd = clampIndex(dt.weekday, 7);
    const int hour12 = dt.hour % 12 == 0 ? 12 : dt.hour % 12;

    std::string out;
    for (size_t i = 0; i < format.size(); ++i) {
        const char c = format[i];
        if (c != '%' || i + 1 >= format.size()) {
            out += c;
            continue;
        }
        char spec = format[++i];
        bool pad = true;
        if (spec == '-' && i + 1 < format.size()) {  // glibc "no padding" flag, e.g. %-H
            pad = false;
            spec = format[++i];
        }
        switch (spec) {
            case 'd': out += number(dt.day, 2, pad); break;
            case 'e': out += pad && dt.day < 10 ? " " + number(dt.day, 1, false) : number(dt.day, 1, false); break;
            case 'm': out += number(dt.month, 2, pad); break;
            case 'y': out += number(dt.year % 100, 2, pad); break;
            case 'Y': out += number(dt.year, 4, false); break;
            case 'H': out += number(dt.hour, 2, pad); break;
            case 'I': out += number(hour12, 2, pad); break;
            case 'M': out += number(dt.minute, 2, pad); break;
            case 'S': out += number(dt.second, 2, pad); break;
            case 'p': out += dt.hour < 12 ? "AM" : "PM"; break;  // the original switched to en_GB for %p
            case 'a': out += fr ? kFrDaysAbbr[wd] : kEnDaysAbbr[wd]; break;
            case 'A': out += fr ? kFrDays[wd] : kEnDays[wd]; break;
            case 'b':
            case 'h': out += fr ? kFrMonthsAbbr[m] : kEnMonthsAbbr[m]; break;
            case 'B': out += fr ? kFrMonths[m] : kEnMonths[m]; break;
            case '%': out += '%'; break;
            default:
                out += '%';
                out += spec;
                break;
        }
    }
    return out;
}

std::string languageFromLocale(const std::string& locale) {
    if (locale.compare(0, 2, "en") == 0) return "en";
    return "fr";
}

std::string posixTimezone(const std::string& zone) {
    struct Zone {
        const char* iana;
        const char* posix;
    };
    static const Zone kZones[] = {
        {"Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3"},
        {"Europe/Brussels", "CET-1CEST,M3.5.0,M10.5.0/3"},
        {"Europe/Luxembourg", "CET-1CEST,M3.5.0,M10.5.0/3"},
        {"Europe/Monaco", "CET-1CEST,M3.5.0,M10.5.0/3"},
        {"Europe/Zurich", "CET-1CEST,M3.5.0,M10.5.0/3"},
        {"Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
        {"Europe/Madrid", "CET-1CEST,M3.5.0,M10.5.0/3"},
        {"Europe/Rome", "CET-1CEST,M3.5.0,M10.5.0/3"},
        {"Europe/Amsterdam", "CET-1CEST,M3.5.0,M10.5.0/3"},
        {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
        {"Europe/Dublin", "IST-1GMT0,M10.5.0,M3.5.0/1"},
        {"Europe/Lisbon", "WET0WEST,M3.5.0/1,M10.5.0"},
        {"America/Montreal", "EST5EDT,M3.2.0,M11.1.0"},
        {"America/Toronto", "EST5EDT,M3.2.0,M11.1.0"},
        {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
        {"Indian/Reunion", "<+04>-4"},
        {"America/Guadeloupe", "AST4"},
        {"America/Martinique", "AST4"},
        {"Pacific/Noumea", "<+11>-11"},
        {"Pacific/Tahiti", "<-10>10"},
        {"UTC", "UTC0"},
    };
    for (const auto& z : kZones) {
        if (zone == z.iana) return z.posix;
    }
    // POSIX strings contain a digit (the UTC offset); IANA names do not (apart from Etc/GMT+N).
    for (char c : zone) {
        if (c >= '0' && c <= '9') return zone;
    }
    return kZones[0].posix;
}

int brightnessForHour(const std::string& csv, int hour, int fallback) {
    if (hour < 0 || hour > 23 || csv.empty()) return fallback;
    std::vector<std::string> fields;
    size_t start = 0;
    while (true) {
        size_t comma = csv.find(',', start);
        fields.push_back(csv.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    if (fields.size() != 24) return fallback;
    const std::string& f = fields[static_cast<size_t>(hour)];
    char* end = nullptr;
    long v = std::strtol(f.c_str(), &end, 10);
    if (end == f.c_str() || *end != '\0' || v < 0 || v > 100) return fallback;
    return static_cast<int>(v);
}

}  // namespace dmd

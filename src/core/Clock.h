#ifndef DMD_CORE_CLOCK_H
#define DMD_CORE_CLOCK_H

// Clock helpers for the ClockRenderer port: strftime-style formatting with French names
// (newlib on the ESP32 only knows the C locale), timezone mapping and per-hour brightness.

#include <string>

namespace dmd {

struct DateTime {
    int year = 2000;
    int month = 1;    // 1..12
    int day = 1;      // 1..31
    int hour = 0;     // 0..23
    int minute = 0;
    int second = 0;
    int weekday = 6;  // 0 = Sunday (2000-01-01 was a Saturday)
};

// Subset of strftime used by Raspy2DMD configs: %d %-d %m %-m %y %Y %H %-H %I %-I %M %S %p
// %a %A %b %B %h %e %j-less, %% . lang "fr" (default, Raspy2DMD's format_affichage fr_FR) or "en".
std::string formatDateTime(const std::string& format, const DateTime& dt, const std::string& lang = "fr");

// "fr_FR" -> "fr", "en_GB" -> "en"; anything unknown -> "fr".
std::string languageFromLocale(const std::string& locale);

// IANA zone name ("Europe/Paris") -> POSIX TZ string for configTzTime(). Strings that already
// look like POSIX TZ ("CET-1CEST,M3.5.0,M10.5.0/3") are returned unchanged; unknown names -> Paris.
std::string posixTimezone(const std::string& zone);

// DMDRenderer.brightnesshours: 24 comma-separated percents, one per hour.
// Returns the value for hour (0..23), or fallback if the list is missing or malformed.
int brightnessForHour(const std::string& csv, int hour, int fallback);

}  // namespace dmd

#endif

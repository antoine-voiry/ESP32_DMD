#ifndef DMD_CORE_TEXT_UTIL_H
#define DMD_CORE_TEXT_UTIL_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace dmd {

struct Rgb {
    // A constructor (rather than default member initialisers) keeps Rgb{r, g, b} valid in C++11,
    // which is what the Arduino-ESP32 toolchain falls back to without build_unflags.
    constexpr Rgb(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0) : r(red), g(green), b(blue) {}
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

// Parses "r,g,b" (config files) or "r;g;b" (msgcolor payloads). Values are clamped to 0..255.
bool parseRgb(const std::string& s, Rgb& out);

// Adafruit GFX fonts only cover ASCII 0x20..0x7E. Maps UTF-8 French/Latin-1 text to the closest
// ASCII ("é" -> "e", "œ" -> "oe", "€" -> "EUR"); anything else becomes '?'.
std::string toDisplayAscii(const std::string& utf8);

// Splits on whitespace, collapsing runs (like Python's textwrap does before wrapping).
std::vector<std::string> splitWords(const std::string& text);

// Greedy word wrap. A line is kept while measure(line) <= maxWidth and line.size() <= maxChars.
// Words wider than a line are broken across lines, as textwrap.wrap(break_long_words=True) does.
std::vector<std::string> wrapText(const std::string& text, int maxWidth, size_t maxChars,
                                  const std::function<int(const std::string&)>& measure);

}  // namespace dmd

#endif

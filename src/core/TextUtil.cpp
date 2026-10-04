#include "TextUtil.h"

#include <cctype>
#include <cstdlib>

namespace dmd {

bool parseRgb(const std::string& s, Rgb& out) {
    char sep = s.find(';') != std::string::npos ? ';' : ',';
    long parts[3];
    size_t start = 0;
    for (int i = 0; i < 3; ++i) {
        size_t end = s.find(sep, start);
        if (i < 2 && end == std::string::npos) {
            return false;
        }
        std::string field = s.substr(start, i < 2 ? end - start : std::string::npos);
        if (field.empty()) {
            return false;
        }
        char* stop = nullptr;
        parts[i] = std::strtol(field.c_str(), &stop, 10);
        while (stop && *stop == ' ') ++stop;
        if (stop == field.c_str() || *stop != '\0') {
            return false;
        }
        start = end + 1;
    }
    auto clamp = [](long v) { return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); };
    out.r = clamp(parts[0]);
    out.g = clamp(parts[1]);
    out.b = clamp(parts[2]);
    return true;
}

namespace {

const char* mapCodepoint(uint32_t cp) {
    switch (cp) {
        case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: return "A";
        case 0xC6: return "AE";
        case 0xC7: return "C";
        case 0xC8: case 0xC9: case 0xCA: case 0xCB: return "E";
        case 0xCC: case 0xCD: case 0xCE: case 0xCF: return "I";
        case 0xD1: return "N";
        case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: case 0xD8: return "O";
        case 0xD9: case 0xDA: case 0xDB: case 0xDC: return "U";
        case 0xDD: return "Y";
        case 0xDF: return "ss";
        case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: return "a";
        case 0xE6: return "ae";
        case 0xE7: return "c";
        case 0xE8: case 0xE9: case 0xEA: case 0xEB: return "e";
        case 0xEC: case 0xED: case 0xEE: case 0xEF: return "i";
        case 0xF1: return "n";
        case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF8: return "o";
        case 0xF9: case 0xFA: case 0xFB: case 0xFC: return "u";
        case 0xFD: case 0xFF: return "y";
        case 0x152: return "OE";
        case 0x153: return "oe";
        case 0x178: return "Y";
        case 0xA0: return " ";
        case 0xAB: case 0xBB: case 0x201C: case 0x201D: return "\"";
        case 0x2018: case 0x2019: return "'";
        case 0x2013: case 0x2014: return "-";
        case 0x2026: return "...";
        case 0xB0: return "o";
        case 0x20AC: return "EUR";
        default: return "?";
    }
}

}  // namespace

std::string toDisplayAscii(const std::string& utf8) {
    std::string out;
    out.reserve(utf8.size());
    size_t i = 0;
    while (i < utf8.size()) {
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        if (c < 0x80) {
            // Control characters (newlines from carousel files, tabs) become spaces.
            out += (c < 0x20 || c == 0x7F) ? ' ' : static_cast<char>(c);
            ++i;
            continue;
        }
        uint32_t cp = 0;
        size_t len = 0;
        if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
        else { out += '?'; ++i; continue; }  // stray continuation byte
        if (i + len > utf8.size()) { out += '?'; break; }
        bool ok = true;
        for (size_t k = 1; k < len; ++k) {
            unsigned char cc = static_cast<unsigned char>(utf8[i + k]);
            if ((cc & 0xC0) != 0x80) { ok = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) { out += '?'; ++i; continue; }
        out += mapCodepoint(cp);
        i += len;
    }
    return out;
}

std::string htmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::vector<std::string> splitWords(const std::string& text) {
    std::vector<std::string> words;
    std::string current;
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                words.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        words.push_back(current);
    }
    return words;
}

std::vector<std::string> wrapText(const std::string& text, int maxWidth, size_t maxChars,
                                  const std::function<int(const std::string&)>& measure) {
    std::vector<std::string> lines;
    if (maxChars == 0) {
        maxChars = 1;
    }
    auto fits = [&](const std::string& s) {
        return s.size() <= maxChars && measure(s) <= maxWidth;
    };
    std::string line;
    for (std::string word : splitWords(text)) {
        std::string candidate = line.empty() ? word : line + " " + word;
        if (fits(candidate)) {
            line = candidate;
            continue;
        }
        if (!line.empty()) {
            lines.push_back(line);
            line.clear();
        }
        // Break words that cannot fit on a line of their own.
        while (!fits(word)) {
            size_t n = 1;
            while (n < word.size() && fits(word.substr(0, n + 1))) {
                ++n;
            }
            lines.push_back(word.substr(0, n));
            word = word.substr(n);
        }
        line = word;
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    return lines;
}

}  // namespace dmd

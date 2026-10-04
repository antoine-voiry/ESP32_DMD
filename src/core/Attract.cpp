#include "Attract.h"

#include <cstdlib>

namespace dmd {

AttractPlaylist::AttractPlaylist(const std::string& scrollOrder, const std::string& supported) {
    size_t start = 0;
    while (start <= scrollOrder.size()) {
        size_t comma = scrollOrder.find(',', start);
        std::string code = scrollOrder.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        // Trim spaces.
        while (!code.empty() && code.front() == ' ') code.erase(code.begin());
        while (!code.empty() && code.back() == ' ') code.pop_back();
        if (code.size() == 1 && supported.find(code[0]) != std::string::npos) {
            _codes.push_back(code[0]);
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
}

char AttractPlaylist::next() {
    if (_codes.empty()) return '\0';
    char c = _codes[_pos];
    _pos = (_pos + 1) % _codes.size();
    return c;
}

CarouselEntry parseCarouselFile(const std::string& content) {
    CarouselEntry entry;
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < content.size()) {
        size_t nl = content.find('\n', start);
        std::string line = content.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    if (lines.size() < 2) {
        return entry;  // options line only (or empty file): the original shows nothing
    }

    // Options line (strip() in the original).
    std::string opts = lines[0];
    while (!opts.empty() && (opts.front() == ' ' || opts.front() == '\t')) opts.erase(opts.begin());
    while (!opts.empty() && (opts.back() == ' ' || opts.back() == '\t')) opts.pop_back();
    entry.hasOptions = !opts.empty();

    size_t pos = 0;
    while (entry.hasOptions && pos <= opts.size()) {
        size_t semi = opts.find(';', pos);
        const std::string opt = opts.substr(pos, semi == std::string::npos ? std::string::npos : semi - pos);
        const std::string spade = "\xE2\x99\xA0";  // ♠
        if (opt == "A") {
            entry.randomMotion = true;
        } else if (opt.find("IT") != std::string::npos) {
            std::string n = opt;
            n.erase(n.find("IT"), 2);
            const long it = std::strtol(n.c_str(), nullptr, 10);
            entry.iterations = it > 0 ? static_cast<int>(it) : 1;
        } else if (opt.find(spade) != std::string::npos) {
            std::string gif = opt;
            gif.erase(gif.find(spade), spade.size());
            entry.gifBackground = gif;
        } else {
            const Motion m = motionFromCarouselCode(opt);
            if (m != Motion::None) {
                entry.motion = m;
                entry.randomMotion = false;
            }
        }
        if (semi == std::string::npos) break;
        pos = semi + 1;
    }

    // msg = msg + " " + ligne for every remaining line.
    for (size_t i = 1; i < lines.size(); ++i) {
        entry.message += " " + lines[i];
    }
    entry.valid = true;
    return entry;
}

Motion carouselRandomMotion(uint32_t random) {
    static const Motion kAll[] = {Motion::Left, Motion::Right, Motion::Up, Motion::Down,
                                  Motion::Rotate, Motion::AntiRotate, Motion::Flip, Motion::Twirl};
    return kAll[random % 8];
}

bool IdleTimer::expired(uint32_t nowMs) {
    if (_afterMs == 0 || !_armed) return false;
    if (static_cast<int32_t>(nowMs - (_last + _afterMs)) >= 0) {
        _armed = false;
        return true;
    }
    return false;
}

}  // namespace dmd

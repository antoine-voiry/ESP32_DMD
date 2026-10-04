#include "Media.h"

#include <algorithm>
#include <cctype>

namespace dmd {

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string normalise(std::string p) {
    std::replace(p.begin(), p.end(), '\\', '/');
    std::string out;
    for (char c : p) {
        if (c == '/' && !out.empty() && out.back() == '/') continue;
        out += c;
    }
    if (out.empty() || out[0] != '/') out = "/" + out;
    if (out.size() > 1 && out.back() == '/') out.pop_back();
    return out;
}

// "/Gifs/x.gif" -> "/gifs/x.gif"
std::string lowerFirstFolder(const std::string& p) {
    const size_t slash = p.find('/', 1);
    if (slash == std::string::npos) return p;
    return lower(p.substr(0, slash)) + p.substr(slash);
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        size_t pos = s.find(sep, start);
        out.push_back(s.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
        if (pos == std::string::npos) break;
        start = pos + 1;
    }
    return out;
}

std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

}  // namespace

std::string mapPiPath(const std::string& path) {
    std::string p = normalise(path);
    const std::string prefix = "/Medias/";
    if (p.compare(0, prefix.size(), prefix) == 0) {
        return lowerFirstFolder(p.substr(prefix.size() - 1));
    }
    return p;
}

std::string resolveMedia(const std::string& wanted, const std::string& baseDir,
                         const std::function<bool(const std::string&)>& exists) {
    if (wanted.empty()) return "";
    std::vector<std::string> candidates;
    candidates.push_back(normalise(baseDir + "/" + wanted));
    const std::string mapped = mapPiPath(wanted);
    candidates.push_back(mapped);
    candidates.push_back(lowerFirstFolder(mapped));
    for (const auto& c : candidates) {
        if (exists(c)) return c;
    }
    return "";
}

std::string lowerExtension(const std::string& path) {
    const std::string name = baseName(path);
    const size_t dot = name.rfind('.');
    return dot == std::string::npos ? "" : lower(name.substr(dot));
}

bool isGifFile(const std::string& path) {
    return lowerExtension(path) == ".gif";
}

bool isPngFile(const std::string& path) {
    return lowerExtension(path) == ".png";
}

std::string baseName(const std::string& path) {
    const size_t slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string parentDir(const std::string& path) {
    const size_t slash = path.rfind('/');
    if (slash == std::string::npos) return "";
    if (slash == 0) return "/";
    return path.substr(0, slash);
}

////////////////////////////////////////////////////////////////////////////////
// Exclusions

bool ExclusionList::toggle(bool folder, const std::string& name, const std::string& piPath) {
    const std::string path = mapPiPath(piPath);
    for (auto it = _entries.begin(); it != _entries.end(); ++it) {
        if (it->folder == folder && it->name == name && it->path == path) {
            _entries.erase(it);
            return false;
        }
    }
    _entries.push_back({folder, name, path});
    return true;
}

bool ExclusionList::isExcluded(const std::string& filePath) const {
    const std::string p = normalise(filePath);
    for (const auto& e : _entries) {
        if (e.folder) {
            // The original matched folders by name and path; the path identifies the folder.
            if (p.size() > e.path.size() && p.compare(0, e.path.size(), e.path) == 0 && p[e.path.size()] == '/') {
                return true;
            }
        } else if (p == e.path) {
            return true;
        }
    }
    return false;
}

std::string ExclusionList::serialize() const {
    std::string out;
    for (const auto& e : _entries) {
        out += e.folder ? "D|" : "F|";
        out += e.name + "|" + e.path + "\n";
    }
    return out;
}

void ExclusionList::parse(const std::string& text) {
    _entries.clear();
    for (const std::string& rawLine : split(text, '\n')) {
        const std::string line = trim(rawLine);
        if (line.empty() || line[0] == '#') continue;
        const std::vector<std::string> f = split(line, '|');
        if (f.size() != 3 || (f[0] != "D" && f[0] != "F")) continue;
        _entries.push_back({f[0] == "D", f[1], normalise(f[2])});
    }
}

std::vector<std::string> filterExcluded(const std::vector<std::string>& files, const ExclusionList& exclusions) {
    std::vector<std::string> out;
    for (const auto& f : files) {
        if (!exclusions.isExcluded(f)) out.push_back(f);
    }
    return out;
}

////////////////////////////////////////////////////////////////////////////////
// Effects

bool findEffect(const std::string& table, const std::string& id, Effect& out) {
    for (const std::string& rawLine : split(table, '\n')) {
        const std::string line = trim(rawLine);
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> f = split(line, '|');
        if (trim(f[0]) != trim(id)) continue;
        f.resize(5);
        out.id = trim(f[0]);
        out.name = f[1];
        out.text = f[2];
        out.gif = trim(f[3]);
        out.sound = trim(f[4]);
        return true;
    }
    return false;
}

EffectKind effectKind(const std::string& text, const std::string& gif, const std::string& sound) {
    if (!gif.empty() && !text.empty()) return EffectKind::GifWithText;
    if (!text.empty()) return EffectKind::Text;
    if (!gif.empty()) return EffectKind::Gif;
    if (!sound.empty()) return EffectKind::SoundOnly;
    return EffectKind::Nothing;
}

////////////////////////////////////////////////////////////////////////////////
// Geometry

FitRect fitImage(int srcW, int srcH, int dstW, int dstH, bool center) {
    FitRect r{0, 0, srcW, srcH};
    if (srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) return {0, 0, 0, 0};
    if (srcW > dstW || srcH > dstH) {
        // Same rounding as PIL: scale by the limiting side, keep at least 1 px.
        if (static_cast<long>(srcW) * dstH > static_cast<long>(srcH) * dstW) {
            r.w = dstW;
            r.h = std::max(1, static_cast<int>((static_cast<long>(srcH) * dstW + srcW / 2) / srcW));
        } else {
            r.h = dstH;
            r.w = std::max(1, static_cast<int>((static_cast<long>(srcW) * dstH + srcH / 2) / srcH));
        }
    }
    if (center) {
        r.x = (dstW - r.w) / 2;
        r.y = (dstH - r.h) / 2;
    }
    return r;
}

void destRows(int srcRow, int srcSize, int dstSize, int& first, int& count) {
    first = 0;
    count = 0;
    if (srcSize <= 0 || dstSize <= 0 || srcRow < 0 || srcRow >= srcSize) return;
    // Smallest d with d * srcSize / dstSize >= srcRow, then every d mapping back to srcRow.
    first = (srcRow * dstSize + srcSize - 1) / srcSize;
    int d = first;
    while (d < dstSize && srcIndex(d, srcSize, dstSize) == srcRow) ++d;
    count = d - first;
}

std::string scoreMediaKey(const std::vector<std::string>& darts) {
    std::string key;
    for (const auto& d : darts) {
        if (d != "X") key = d;
    }
    return key;
}

std::string uploadPath(const std::string& dir, const std::string& fileName) {
    std::string name = baseName(normalise(fileName));
    std::string clean;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-') {
            clean += c;
        } else if (c == ' ') {
            clean += '_';
        }
    }
    if (clean.empty() || clean == "." || clean == "..") return "";
    std::string d = normalise(dir);
    // Directories may only use the same safe characters and no "..".
    for (const std::string& part : split(d.substr(1), '/')) {
        if (part == ".." || part == ".") return "";
        for (char c : part) {
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')) return "";
        }
    }
    return d == "/" ? "/" + clean : d + "/" + clean;
}

}  // namespace dmd

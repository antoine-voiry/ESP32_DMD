#ifndef DMD_CORE_MEDIA_H
#define DMD_CORE_MEDIA_H

// Media library logic from DMDRenderer.py (RenderGif, RenderImage, RandomFile, Exclude, RenderEffet),
// independent of the filesystem so it can be unit tested on the host.
//
// On the Pi media lived under /Medias/<Dir>/ (Gifs, Images, Scores, SpecialsMoves, Patterns, Textes,
// Meteo, EDFJoursTempo). On the ESP32 they live on LittleFS under /<dir>/ in lower case: /gifs,
// /images, /scores, /specialsmoves, /patterns, /textes, /meteo, /edfjourstempo.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace dmd {

namespace media {
constexpr const char* kGifs = "/gifs";
constexpr const char* kImages = "/images";
constexpr const char* kScores = "/scores";
constexpr const char* kSpecialMoves = "/specialsmoves";
constexpr const char* kPatterns = "/patterns";
constexpr const char* kTextes = "/textes";
}  // namespace media

// "/Medias/Gifs/fun/a.gif" -> "/gifs/fun/a.gif". Other absolute paths are returned unchanged;
// relative paths get a leading '/'. Backslashes become '/', duplicate slashes are collapsed.
std::string mapPiPath(const std::string& path);

// Port of the lookup in RenderGif()/RenderImage(): returns the first existing candidate among
// baseDir/wanted, mapPiPath(wanted) and the same with its first folder lower-cased ("Gifs/x.gif"),
// or "" when none exists. exists() is the filesystem check.
std::string resolveMedia(const std::string& wanted, const std::string& baseDir,
                         const std::function<bool(const std::string&)>& exists);

std::string lowerExtension(const std::string& path);  // ".gif", ".png", ... ("" if none)
bool isGifFile(const std::string& path);
bool isPngFile(const std::string& path);
std::string baseName(const std::string& path);   // "/gifs/a/b.gif" -> "b.gif"
std::string parentDir(const std::string& path);  // "/gifs/a/b.gif" -> "/gifs/a"

// Exclusions (excludeFolder|name|path, excludeFile|name|path): each call toggles the entry, like
// DMDRenderer.Exclude(). Excluded folders are skipped with everything below them.
class ExclusionList {
public:
    struct Entry {
        bool folder;
        std::string name;
        std::string path;  // ESP32 path (mapped from the Pi path)
    };

    // Returns true if the entry is now excluded, false if it was removed.
    bool toggle(bool folder, const std::string& name, const std::string& piPath);
    bool isExcluded(const std::string& filePath) const;
    const std::vector<Entry>& entries() const { return _entries; }

    // One entry per line: "D|name|path" or "F|name|path".
    std::string serialize() const;
    void parse(const std::string& text);

private:
    std::vector<Entry> _entries;
};

// Port of RandomFile(): the files below `root` that are not excluded, as found by the walker.
std::vector<std::string> filterExcluded(const std::vector<std::string>& files, const ExclusionList& exclusions);

// effet|id: the Pi read them from MariaDB (effects.effect); here from /effets.txt,
// one "id|name|text|gif|sound" per line, '#' starts a comment.
struct Effect {
    std::string id;
    std::string name;
    std::string text;
    std::string gif;
    std::string sound;
};
bool findEffect(const std::string& table, const std::string& id, Effect& out);

// What RenderSoundEffet() plays for a (text, gif, sound) triple.
enum class EffectKind { GifWithText, Text, Gif, SoundOnly, Nothing };
EffectKind effectKind(const std::string& text, const std::string& gif, const std::string& sound);

// PIL Image.thumbnail() + center_images: shrink (never enlarge) to fit, keeping the aspect ratio,
// then centre when `center` is set.
struct FitRect {
    int x, y, w, h;
};
FitRect fitImage(int srcW, int srcH, int dstW, int dstH, bool center);

// Nearest-neighbour scaling when a decoder hands us one source row at a time: the destination
// rows [first, first + count) take their pixels from source row srcRow (count may be 0 when shrinking).
void destRows(int srcRow, int srcSize, int dstSize, int& first, int& count);
// Source column/row for destination index d when scaling srcSize -> dstSize.
inline int srcIndex(int d, int srcSize, int dstSize) {
    return dstSize > 0 ? d * srcSize / dstSize : 0;
}

// Score animations live in Scores/<last dart that is not X>/ (RenderText(val=True)).
std::string scoreMediaKey(const std::vector<std::string>& darts);

// Sanitises an uploaded file name into dir: keeps [A-Za-z0-9._-] (spaces become '_'), drops any
// path part, rejects empty names and "." / "..". Returns "" when rejected.
std::string uploadPath(const std::string& dir, const std::string& fileName);

}  // namespace dmd

#endif

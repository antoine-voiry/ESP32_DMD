#ifndef DMD_MEDIA_LIBRARY_H
#define DMD_MEDIA_LIBRARY_H

// Media files on LittleFS (see core/Media.h for the folder layout), the exclusion list
// (/exclusions.txt) and the effects table (/effets.txt).

#include <string>
#include <vector>

#include "core/Media.h"

class MediaLibrary {
public:
    static constexpr const char* kExclusionsFile = "/exclusions.txt";
    static constexpr const char* kEffectsFile = "/effets.txt";

    void begin();

    // Finds `wanted` the way RenderGif()/RenderImage() did (baseDir first, then the mapped Pi path).
    std::string resolve(const std::string& wanted, const char* baseDir) const;

    // Random non-excluded file below dir (recursive, like RandomFile()) with the given extension
    // (".gif", ".png", or "" for any). "" when there is none.
    std::string randomFile(const std::string& dir, const char* extension) const;
    // Random file directly in dir (Scores/<dart>/, SpecialsMoves/<move>/): listdir() in the original.
    std::string randomFileIn(const std::string& dir, const char* extension) const;

    // Every non-excluded GIF below /gifs, sorted (RenderDemoGif).
    std::vector<std::string> allGifs() const;

    // excludeFolder / excludeFile: toggles and saves. Returns true if now excluded.
    bool toggleExclusion(bool folder, const std::string& name, const std::string& piPath);

    bool findEffect(const std::string& id, dmd::Effect& out) const;

private:
    std::vector<std::string> candidates(const std::string& dir, const char* extension, bool recursive) const;

    dmd::ExclusionList _exclusions;
};

#endif

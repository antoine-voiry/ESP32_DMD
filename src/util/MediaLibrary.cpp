#include "MediaLibrary.h"

#include <esp_log.h>
#include <esp_random.h>

#include <algorithm>

#include "Storage.h"

static const char* TAG = "Media";

void MediaLibrary::begin() {
    _exclusions.parse(storageReadText(kExclusionsFile));
    ESP_LOGI(TAG, "%u exclusion(s) loaded", static_cast<unsigned>(_exclusions.entries().size()));
}

std::string MediaLibrary::resolve(const std::string& wanted, const char* baseDir) const {
    return dmd::resolveMedia(wanted, baseDir, [](const std::string& p) { return storageExists(p); });
}

std::vector<std::string> MediaLibrary::candidates(const std::string& dir, const char* extension,
                                                  bool recursive) const {
    std::vector<std::string> files = dmd::filterExcluded(storageListFiles(dir, recursive), _exclusions);
    if (extension && *extension) {
        const std::string ext = extension;
        files.erase(std::remove_if(files.begin(), files.end(),
                                   [&](const std::string& f) { return dmd::lowerExtension(f) != ext; }),
                    files.end());
    }
    return files;
}

std::string MediaLibrary::randomFile(const std::string& dir, const char* extension) const {
    const std::vector<std::string> files = candidates(dir, extension, true);
    return files.empty() ? "" : files[esp_random() % files.size()];
}

std::string MediaLibrary::randomFileIn(const std::string& dir, const char* extension) const {
    const std::vector<std::string> files = candidates(dir, extension, false);
    return files.empty() ? "" : files[esp_random() % files.size()];
}

std::vector<std::string> MediaLibrary::allGifs() const {
    std::vector<std::string> files = candidates(dmd::media::kGifs, ".gif", true);
    std::sort(files.begin(), files.end());
    return files;
}

bool MediaLibrary::toggleExclusion(bool folder, const std::string& name, const std::string& piPath) {
    const bool excluded = _exclusions.toggle(folder, name, piPath);
    storageWriteText(kExclusionsFile, _exclusions.serialize());
    ESP_LOGI(TAG, "%s %s %s", excluded ? "Excluded" : "Re-included", folder ? "folder" : "file",
             dmd::mapPiPath(piPath).c_str());
    return excluded;
}

bool MediaLibrary::findEffect(const std::string& id, dmd::Effect& out) const {
    return dmd::findEffect(storageReadText(kEffectsFile), id, out);
}

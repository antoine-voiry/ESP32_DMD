#ifndef DMD_STORAGE_H
#define DMD_STORAGE_H

// The board's filesystem: LittleFS (real folders, unlike SPIFFS) on the "spiffs" data partition.
// Holds config.json, exclusions.txt, effets.txt and the media folders (see core/Media.h).

#include <FS.h>
#include <LittleFS.h>

#include <functional>
#include <string>
#include <vector>

inline fs::FS& storage() {
    return LittleFS;
}

// Mounts the filesystem, formatting it on first boot. Safe to call more than once.
bool storageBegin();

bool storageExists(const std::string& path);

// Every file below dir (recursively), as absolute paths. Empty if dir does not exist.
std::vector<std::string> storageListFiles(const std::string& dir, bool recursive = true);

// Direct sub-folders of dir.
std::vector<std::string> storageListDirs(const std::string& dir);

// Reads a small text file (up to maxBytes); "" if missing.
std::string storageReadText(const std::string& path, size_t maxBytes = 16384);
bool storageWriteText(const std::string& path, const std::string& content);

// Creates every folder of path's parent ("/a/b/c.gif" -> "/a", "/a/b").
void storageMakeParents(const std::string& path);

#endif

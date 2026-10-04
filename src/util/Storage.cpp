#include "Storage.h"

#include <esp_log.h>

static const char* TAG = "Storage";

bool storageBegin() {
    static bool mounted = false;
    if (mounted) {
        return true;
    }
    mounted = LittleFS.begin(false) || LittleFS.begin(true);
    if (mounted) {
        ESP_LOGI(TAG, "LittleFS mounted: %u / %u bytes used", static_cast<unsigned>(LittleFS.usedBytes()),
                 static_cast<unsigned>(LittleFS.totalBytes()));
    } else {
        ESP_LOGE(TAG, "LittleFS mount failed");
    }
    return mounted;
}

bool storageExists(const std::string& path) {
    return !path.empty() && LittleFS.exists(path.c_str());
}

static void listInto(const std::string& dir, bool recursive, bool wantDirs, std::vector<std::string>& out) {
    File root = LittleFS.open(dir.c_str());
    if (!root || !root.isDirectory()) {
        return;
    }
    for (File f = root.openNextFile(); f; f = root.openNextFile()) {
        const std::string path = f.path();
        if (f.isDirectory()) {
            if (wantDirs) out.push_back(path);
            if (recursive) listInto(path, true, wantDirs, out);
        } else if (!wantDirs) {
            out.push_back(path);
        }
    }
}

std::vector<std::string> storageListFiles(const std::string& dir, bool recursive) {
    std::vector<std::string> out;
    listInto(dir, recursive, false, out);
    return out;
}

std::vector<std::string> storageListDirs(const std::string& dir) {
    std::vector<std::string> out;
    listInto(dir, false, true, out);
    return out;
}

std::string storageReadText(const std::string& path, size_t maxBytes) {
    File f = LittleFS.open(path.c_str(), "r");
    if (!f || f.isDirectory()) {
        return "";
    }
    std::string out;
    out.reserve(f.size() < maxBytes ? f.size() : maxBytes);
    uint8_t buf[256];
    while (out.size() < maxBytes) {
        const size_t n = f.read(buf, sizeof(buf));
        if (n == 0) break;
        out.append(reinterpret_cast<const char*>(buf), n);
    }
    if (out.size() > maxBytes) out.resize(maxBytes);
    return out;
}

void storageMakeParents(const std::string& path) {
    size_t pos = 1;
    while ((pos = path.find('/', pos)) != std::string::npos) {
        const std::string dir = path.substr(0, pos);
        if (!LittleFS.exists(dir.c_str())) {
            LittleFS.mkdir(dir.c_str());
        }
        ++pos;
    }
}

bool storageWriteText(const std::string& path, const std::string& content) {
    storageMakeParents(path);
    File f = LittleFS.open(path.c_str(), "w");
    if (!f) {
        ESP_LOGE(TAG, "Cannot write %s", path.c_str());
        return false;
    }
    const size_t written = f.write(reinterpret_cast<const uint8_t*>(content.data()), content.size());
    f.close();
    return written == content.size();
}

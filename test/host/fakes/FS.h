#ifndef FAKE_FS_H
#define FAKE_FS_H

// LittleFS on top of a host directory (fake::fsRoot()), so storage code runs for real.

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "Arduino.h"

namespace fs {

class File : public Stream {
public:
    File() = default;
    explicit operator bool() const { return _state != nullptr; }

    size_t write(uint8_t c) override { return write(&c, 1); }
    size_t write(const uint8_t* buffer, size_t size) override;
    int available() override;
    int read() override;
    int peek() override;
    size_t read(uint8_t* buffer, size_t size);
    bool seek(uint32_t position);
    size_t position() const;
    size_t size() const;
    void close();
    bool isDirectory() const;
    File openNextFile();
    const char* path() const;
    const char* name() const;

private:
    struct State;
    friend class FS;
    std::shared_ptr<State> _state;
};

class FS {
public:
    File open(const char* path, const char* mode = "r");
    File open(const String& path, const char* mode = "r") { return open(path.c_str(), mode); }
    bool exists(const char* path);
    bool exists(const String& path) { return exists(path.c_str()); }
    bool remove(const char* path);
    bool remove(const String& path) { return remove(path.c_str()); }
    bool mkdir(const char* path);
    bool rmdir(const char* path);
};

class LittleFSFS : public FS {
public:
    bool begin(bool formatOnFail = false);
    size_t totalBytes();
    size_t usedBytes();
};

}  // namespace fs

using fs::File;

#endif

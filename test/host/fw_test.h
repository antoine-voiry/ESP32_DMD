#ifndef DMD_FW_TEST_H
#define DMD_FW_TEST_H

// Shared helpers for the firmware tests (src/util, src/render, src/matrix, main.cpp on the fakes).

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include <string>

#include "check.h"
#include "util/ConfigHelper.h"
#include "util/DMDRenderer.h"

// Fresh fakes, an empty filesystem and default settings.
inline void freshBoard() {
    fake::resetAll();
    ConfigHelper::getInstance().loadConfigFile();
}

inline void writeBytes(const std::string& path, const unsigned char* data, size_t size) {
    fake::writeFile(path, std::string(reinterpret_cast<const char*>(data), size));
}

// Advances the clock in steps, updating the renderer, until it is idle (or `maxMs` elapsed).
inline void runUntilIdle(DMDRenderer& r, unsigned long maxMs = 20000, unsigned long stepMs = 10) {
    for (unsigned long t = 0; t < maxMs; t += stepMs) {
        r.update();
        if (r.idle()) return;
        fake::advance(stepMs);
    }
}

inline void runFor(DMDRenderer& r, unsigned long ms, unsigned long stepMs = 10) {
    for (unsigned long t = 0; t < ms; t += stepMs) {
        r.update();
        fake::advance(stepMs);
    }
}

inline void setSetting(const char* section, const char* key, const std::string& value) {
    ConfigHelper::getInstance().setSetting(section, key, value);
}

void testServices();
void testRendering();
void testOnline();
void testMessages();
void testWeb();
void testMain();

#endif

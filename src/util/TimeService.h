#ifndef DMD_TIME_SERVICE_H
#define DMD_TIME_SERVICE_H

// NTP time (replaces the Pi's system clock) and DMDRenderer.brightnesshours, which the original
// applies by re-creating the matrix each time the hour changes.

#include <cstdint>
#include <string>

class DMDRenderer;

class TimeService {
public:
    // posixTz: e.g. "CET-1CEST,M3.5.0,M10.5.0/3" (see dmd::posixTimezone()).
    void begin(const std::string& posixTz);
    bool synced() const;
    // Applies brightnesshours[current hour] when the hour (or the setting) changes.
    void loop(uint32_t nowMs, DMDRenderer& renderer);
    // Forces the brightness to be re-evaluated on the next loop() (after a conf change).
    void invalidate() { _appliedHour = -1; }

private:
    uint32_t _lastCheckMs = 0;
    int _appliedHour = -1;
    bool _wasSynced = false;
};

#endif

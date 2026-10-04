#include "TimeService.h"

#include <Arduino.h>
#include <esp_log.h>
#include <time.h>

#include "ConfigHelper.h"
#include "DMDRenderer.h"
#include "core/Clock.h"

static const char* TAG = "TimeService";

namespace {
constexpr int kFirstValidYear = 2024;
constexpr uint32_t kCheckEveryMs = 10000;
}  // namespace

void TimeService::begin(const std::string& posixTz) {
    ESP_LOGI(TAG, "Starting NTP, timezone %s", posixTz.c_str());
    configTzTime(posixTz.c_str(), "pool.ntp.org", "time.google.com", "time.cloudflare.com");
}

bool TimeService::synced() const {
    const time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    return t.tm_year + 1900 >= kFirstValidYear;
}

void TimeService::loop(uint32_t nowMs, DMDRenderer& renderer) {
    if (_appliedHour >= 0 && nowMs - _lastCheckMs < kCheckEveryMs) {
        return;
    }
    _lastCheckMs = nowMs;
    if (!synced()) {
        return;
    }
    if (!_wasSynced) {
        _wasSynced = true;
        ESP_LOGI(TAG, "Clock synchronised");
    }
    const time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    if (t.tm_hour == _appliedHour) {
        return;
    }
    _appliedHour = t.tm_hour;
    const ConfigHelper& config = ConfigHelper::getInstance();
    const std::string& hours = config.getBrightnessHours();
    if (hours.empty()) {
        return;  // only the fixed "brightness" is configured
    }
    const int pct = dmd::brightnessForHour(hours, t.tm_hour, config.getBrightness());
    ESP_LOGI(TAG, "Brightness for %02d:00 -> %d %%", t.tm_hour, pct);
    renderer.setBrightnessPercent(pct);
}

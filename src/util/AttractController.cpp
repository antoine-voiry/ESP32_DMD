#include "AttractController.h"

#include <Arduino.h>
#include <SPIFFS.h>
#include <esp_log.h>
#include <esp_random.h>

#include <vector>

#include "DMDRenderer.h"
#include "Settings.h"

static const char* TAG = "Attract";

namespace {

// Directory.textes on the Pi (/Medias/Textes/); upload carousel files to this SPIFFS folder.
constexpr const char* kCarouselDir = "/textes";
constexpr size_t kMaxCarouselFileBytes = 4096;
// The original waits 4 s after a carousel text without options. We also hold static texts that
// have options, otherwise they would flash by (rendering is much faster than on the Pi).
constexpr uint32_t kCarouselHoldMs = 4000;
constexpr uint32_t kFxShowMs = 6000;

}  // namespace

AttractController::AttractController(DMDRenderer* renderer) : _renderer(renderer), _rng(esp_random()) {}

void AttractController::start(const std::string& codes) {
    _playlist.reset(new dmd::AttractPlaylist(codes, kSupportedCodes));
    if (_playlist->empty()) {
        ESP_LOGW(TAG, "Nothing playable in '%s' (supported: %s)", codes.c_str(), kSupportedCodes);
        _playlist.reset();
        _running = false;
        return;
    }
    ESP_LOGI(TAG, "Attract mode started: %s", codes.c_str());
    _running = true;
}

void AttractController::stop() {
    if (_running) {
        ESP_LOGI(TAG, "Attract mode stopped");
    }
    _running = false;
    _playlist.reset();
}

void AttractController::onMessage(uint32_t nowMs) {
    stop();
    _idle.configure(settings::attractAfterMs());
    _idle.touch(nowMs);
}

void AttractController::loop(uint32_t nowMs) {
    if (!_running) {
        if (_idle.expired(nowMs) && _renderer->idle()) {
            start(settings::scrollOrder());
        }
        return;
    }
    if (!_renderer->idle()) {
        return;
    }
    // Try each show once; skip those with nothing to play (e.g. no carousel files yet).
    for (size_t i = 0; i < _playlist->codes().size(); ++i) {
        if (play(_playlist->next())) {
            return;
        }
    }
    ESP_LOGW(TAG, "No show could be played, stopping attract mode");
    stop();
}

bool AttractController::play(char code) {
    switch (code) {
        case 'T':
            _renderer->renderClock(settings::clockSpec());
            return true;
        case '4':
            return playCarousel();
        case 'F': {
            static const dmd::FxBackground kBackgrounds[] = {
                dmd::FxBackground::Plasma, dmd::FxBackground::Fireworks, dmd::FxBackground::Starfield,
                dmd::FxBackground::MatrixRain};
            FxSpec fx;
            fx.background = kBackgrounds[_rng.range(0, 3)];
            fx.durationMs = kFxShowMs;
            _renderer->renderFx(fx);
            return true;
        }
        default:
            return false;
    }
}

bool AttractController::playCarousel() {
    File dir = SPIFFS.open(kCarouselDir);
    if (!dir || !dir.isDirectory()) {
        ESP_LOGD(TAG, "No carousel folder %s", kCarouselDir);
        return false;
    }
    std::vector<std::string> files;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        if (!f.isDirectory()) {
            files.push_back(f.path());
        }
    }
    if (files.empty()) {
        return false;
    }
    const std::string& path = files[static_cast<size_t>(_rng.range(0, static_cast<int>(files.size()) - 1))];
    File file = SPIFFS.open(path.c_str(), "r");
    if (!file) {
        ESP_LOGW(TAG, "Cannot open %s", path.c_str());
        return false;
    }
    std::string content;
    while (file.available() && content.size() < kMaxCarouselFileBytes) {
        content += static_cast<char>(file.read());
    }
    file.close();

    const dmd::CarouselEntry entry = dmd::parseCarouselFile(content);
    if (!entry.valid) {
        ESP_LOGW(TAG, "%s has no message (first line = options, then the text)", path.c_str());
        return false;
    }
    if (!entry.gifBackground.empty()) {
        ESP_LOGW(TAG, "%s: GIF background '%s' needs media support (phase 3), showing text only",
                 path.c_str(), entry.gifBackground.c_str());
    }
    TextRequest request;
    request.text = entry.message;
    request.motion = entry.randomMotion ? dmd::carouselRandomMotion(_rng.next()) : entry.motion;
    request.iterations = entry.iterations;
    const bool needsHold = !entry.hasOptions || request.motion == dmd::Motion::None;
    _renderer->renderText(request, needsHold ? kCarouselHoldMs : 0);
    return true;
}

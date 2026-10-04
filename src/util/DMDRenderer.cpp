#include "DMDRenderer.h"

#include <memory>
#include <utility>

#include "MediaLibrary.h"
#include "core/Media.h"
#include "core/Protocol.h"
#include "core/SpecialMoves.h"
#include "esp_log.h"

static const char* TAG = "DMDRenderer";

// RenderText(val=True) sleeps 2 s after showing a special move before showing the score.
static constexpr uint32_t kSpecialMoveHoldMs = 2000;
// Fireworks for the rarest moves.
static constexpr uint32_t kBigCelebrationMs = 3500;

static bool isBigMove(const std::string& move) {
    return move == "MAXIMUM_TON_80" || move == "BLACK_HAT_THREE_IN_THE_BLACK" || move == "RED_HAT" ||
           move == "HAT_TRICK" || move == "CHAMPAGNE_BREAKFAST";
}

DMDRenderer::DMDRenderer(Hub75_Matrix* matrix) : _dmd(matrix) {
    ESP_LOGI(TAG, "Initializing DMDRenderer");
}

void DMDRenderer::update() {
    _runner.update(millis());
}

void DMDRenderer::interrupt() {
    _runner.interrupt();
}

void DMDRenderer::renderText(const TextRequest& request, uint32_t holdMs) {
    TextStyle style = _defaults;
    style.motion = request.motion;
    style.iterations = request.iterations < 1 ? 1 : request.iterations;
    if (request.hasFg) style.fg = request.fg;
    if (request.hasBg) style.bg = request.bg;
    if (dmd::isHorizontalScroll(style.motion)) {
        style.maxCharsPerLine = 9999;  // scrolling text is a single line
    }
    ESP_LOGD(TAG, "Queue text '%s' (hold %u ms)", request.text.c_str(), static_cast<unsigned>(holdMs));
    _runner.enqueue(std::unique_ptr<dmd::Scene>(new TextScene(*_dmd, request.text, style)), holdMs);
}

void DMDRenderer::renderText(const std::string& text, uint32_t holdMs) {
    TextRequest request;
    request.text = text;
    renderText(request, holdMs);
}

void DMDRenderer::renderScore(const std::string& score, uint32_t holdMs) {
    const std::string upper = dmd::toUpper(score);
    const std::vector<std::string> darts = dmd::splitScore(upper);
    bool hasMiss = false;
    for (const auto& d : darts) {
        if (d == "X") hasMiss = true;
    }
    // A random GIF from Scores/<last dart that is not X>/, when there is one.
    const std::string key = dmd::scoreMediaKey(darts);
    if (_media && !key.empty()) {
        const std::string gif = _media->randomFileIn(std::string(dmd::media::kScores) + "/" + key, ".gif");
        if (!gif.empty()) {
            renderGif(gif);
        }
    }
    if (!hasMiss) {
        const std::string move = dmd::findSpecialMove(darts);
        // SpecialsMoves/<MOVE>/: a GIF when there is one, otherwise the move's name.
        const std::string moveGif =
            (_media && !move.empty())
                ? _media->randomFileIn(std::string(dmd::media::kSpecialMoves) + "/" + move, ".gif")
                : std::string();
        if (!moveGif.empty()) {
            ESP_LOGI(TAG, "Special move: %s (%s)", move.c_str(), moveGif.c_str());
            renderGif(moveGif);
        } else if (!move.empty()) {
            std::string label = move;
            for (auto& c : label) {
                if (c == '_') c = ' ';
            }
            ESP_LOGI(TAG, "Special move: %s", move.c_str());
            if (!_celebrations) {
                renderText(label, kSpecialMoveHoldMs);
            } else {
                FxSpec fx;
                fx.text = label;
                fx.fg = _defaults.fg;
                fx.maxCharsPerLine = _defaults.maxCharsPerLine;
                fx.maxFontPx = _defaults.maxFontPx;
                if (isBigMove(move)) {
                    fx.background = dmd::FxBackground::Fireworks;
                    fx.textFx = dmd::FxText::Rainbow;
                    fx.durationMs = kBigCelebrationMs;
                } else {
                    fx.textFx = dmd::FxText::Sparkle;
                    fx.durationMs = kSpecialMoveHoldMs;
                }
                renderFx(fx);
            }
        }
    }
    renderText(upper, holdMs);
}

void DMDRenderer::renderFx(FxSpec spec, uint32_t holdMs) {
    ESP_LOGD(TAG, "Queue effect (%u ms, hold %u ms)", static_cast<unsigned>(spec.durationMs),
             static_cast<unsigned>(holdMs));
    _runner.enqueue(std::unique_ptr<dmd::Scene>(new FxScene(*_dmd, std::move(spec))), holdMs);
}

void DMDRenderer::renderGif(const std::string& path, uint32_t holdMs, const std::string& text) {
    MediaOverlay overlay;
    overlay.text = text;
    overlay.color = _defaults.fg;
    overlay.maxCharsPerLine = _defaults.maxCharsPerLine;
    overlay.maxFontPx = _defaults.maxFontPx;
    _runner.enqueue(std::unique_ptr<dmd::Scene>(new GifScene(*_dmd, path, _centerImages, overlay)), holdMs);
}

void DMDRenderer::renderImage(const std::string& path, uint32_t holdMs, const std::string& text) {
    MediaOverlay overlay;
    overlay.text = text;
    overlay.color = _defaults.fg;
    overlay.maxCharsPerLine = _defaults.maxCharsPerLine;
    overlay.maxFontPx = _defaults.maxFontPx;
    _runner.enqueue(std::unique_ptr<dmd::Scene>(new ImageScene(*_dmd, path, _centerImages, overlay)), holdMs);
}

void DMDRenderer::renderClock(ClockSpec spec, uint32_t holdMs) {
    _runner.enqueue(std::unique_ptr<dmd::Scene>(new ClockScene(*_dmd, std::move(spec))), holdMs);
}

void DMDRenderer::renderStatus(const std::string& text) {
    _runner.reset();
    renderText(text);
}

void DMDRenderer::clear() {
    _runner.reset();
    _dmd->clearScreen();
}

void DMDRenderer::setBrightnessPercent(int percent) {
    _dmd->setBrightnessPercent(percent);
}

bool DMDRenderer::idle() const {
    return _runner.idle(millis());
}

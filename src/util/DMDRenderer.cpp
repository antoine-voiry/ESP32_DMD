#include "DMDRenderer.h"

#include <memory>
#include <utility>

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
    // TODO(phase 3): play a random GIF from Scores/<last dart> and SpecialsMoves/<move> when present.
    if (!hasMiss) {
        const std::string move = dmd::findSpecialMove(darts);
        if (!move.empty()) {
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

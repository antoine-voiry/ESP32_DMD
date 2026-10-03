#include "DMDRenderer.h"

#include <memory>

#include "core/Protocol.h"
#include "core/SpecialMoves.h"
#include "esp_log.h"

static const char* TAG = "DMDRenderer";

// RenderText(val=True) sleeps 2 s after showing a special move before showing the score.
static constexpr uint32_t kSpecialMoveHoldMs = 2000;

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
        std::string move = dmd::findSpecialMove(darts);
        if (!move.empty()) {
            for (auto& c : move) {
                if (c == '_') c = ' ';
            }
            ESP_LOGI(TAG, "Special move: %s", move.c_str());
            renderText(move, kSpecialMoveHoldMs);
        }
    }
    renderText(upper, holdMs);
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

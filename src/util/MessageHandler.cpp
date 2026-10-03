#include "MessageHandler.h"

#include <esp_sleep.h>

#include "ConfigHelper.h"
#include "core/Fx.h"
#include "core/Protocol.h"
#include "esp_log.h"

static const char* TAG = "MessageHandler";

MessageHandler::MessageHandler(DMDRenderer* renderer) : dmdRenderer(renderer) {
    ESP_LOGI(TAG, "Initializing MessageHandler");
    setupHandlers();
}

uint32_t MessageHandler::effectDuration(const std::vector<std::string>& params, size_t i) {
    constexpr uint32_t kDefaultMs = 5000;
    constexpr uint32_t kMaxMs = 10u * 60u * 1000u;  // a stuck "fx|plasma|99999" would block the queue
    uint32_t ms = holdArg(params, i);
    if (ms == 0) return kDefaultMs;
    return ms > kMaxMs ? kMaxMs : ms;
}

uint32_t MessageHandler::holdArg(const std::vector<std::string>& params, size_t i) {
    if (i >= params.size()) {
        return 0;
    }
    long seconds = dmd::parseIntArg(params[i], 0);
    return seconds > 0 ? static_cast<uint32_t>(seconds) * 1000u : 0;
}

void MessageHandler::notPortedYet(const char* action, const char* phase) {
    ESP_LOGW(TAG, "'%s' is not ported to the ESP32 yet (%s)", action, phase);
}

void MessageHandler::setupHandlers() {
    ////////////////////////////////////////////////////////////////////////////
    // System
    handlers["rebt"] = [this](const std::vector<std::string>&) {
        ESP_LOGW(TAG, "Reboot requested");
        dmdRenderer->clear();
        delay(100);
        ESP.restart();
    };

    handlers["shutdwn"] = [this](const std::vector<std::string>&) {
        // No power switch on the ESP32: blank the panel and sleep until the board is power cycled.
        ESP_LOGW(TAG, "Shutdown requested: entering deep sleep (power cycle to restart)");
        dmdRenderer->clear();
        delay(100);
        esp_deep_sleep_start();
    };

    handlers["conf"] = [this](const std::vector<std::string>& params) { applyConf(params); };

    ////////////////////////////////////////////////////////////////////////////
    // Text
    handlers["msg"] = [this](const std::vector<std::string>& params) {
        const std::string text = params[0].empty() ? "-Vide-" : params[0];
        dmdRenderer->renderText(text, params.size() == 2 ? holdArg(params, 1) : 0);
    };

    handlers["score"] = [this](const std::vector<std::string>& params) {
        dmdRenderer->renderScore(params[0], params.size() == 2 ? holdArg(params, 1) : 0);
    };

    // msgmove|text|left|2
    handlers["msgmove"] = [this](const std::vector<std::string>& params) {
        TextRequest request;
        request.text = params[0];
        request.motion = dmd::parseMotion(params[1]);
        dmdRenderer->renderText(request, params.size() == 3 ? holdArg(params, 2) : 0);
    };

    // msgmovebcl|text|left|iterations|2
    // (the original sleeps for "iterations" seconds here, an evident typo for the 4th argument)
    handlers["msgmovebcl"] = [this](const std::vector<std::string>& params) {
        TextRequest request;
        request.text = params[0];
        request.motion = dmd::parseMotion(params[1]);
        request.iterations = static_cast<int>(dmd::parseIntArg(params[2], 1));
        dmdRenderer->renderText(request, params.size() == 4 ? holdArg(params, 3) : 0);
    };

    // msgcolor|text|255;255;255|0;0;0
    handlers["msgcolor"] = [this](const std::vector<std::string>& params) {
        TextRequest request;
        request.text = params[0];
        request.hasFg = dmd::parseRgb(params[1], request.fg);
        request.hasBg = dmd::parseRgb(params[2], request.bg);
        if (!request.hasFg || !request.hasBg) {
            ESP_LOGW(TAG, "msgcolor: bad colour '%s' / '%s', using defaults", params[1].c_str(),
                     params[2].c_str());
        }
        dmdRenderer->renderText(request);
    };

    // msgimg|text|/Medias/Patterns/2.png
    handlers["msgimg"] = [this](const std::vector<std::string>& params) {
        notPortedYet("msgimg background image", "phase 3, media");
        dmdRenderer->renderText(params[0]);
    };

    // testFont|Font.ttf: the ESP32 has no TrueType fonts, show the sample with the built-in fonts.
    handlers["testFont"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "testFont '%s': TrueType fonts are not available, using built-in fonts",
                 params[0].c_str());
        dmdRenderer->renderText("Test de la font");
    };

    // soundeffet|text|gif|sound: no audio on the ESP32; show the text part for now.
    handlers["soundeffet"] = [this](const std::vector<std::string>& params) {
        if (!params[0].empty()) {
            dmdRenderer->renderText(params[0]);
        } else {
            notPortedYet("soundeffet without text", "phase 3, media");
        }
    };

    ////////////////////////////////////////////////////////////////////////////
    // ESP32 extensions (Raspydarts never sends these; handy from mosquitto_pub or Home Assistant)

    // fx|plasma|5   backgrounds: plasma, fireworks, stars, matrix
    handlers["fx"] = [this](const std::vector<std::string>& params) {
        FxSpec fx;
        if (!dmd::parseFxBackground(params[0], fx.background)) {
            ESP_LOGW(TAG, "fx: unknown effect '%s' (plasma, fireworks, stars, matrix)", params[0].c_str());
            return;
        }
        fx.durationMs = effectDuration(params, 1);
        dmdRenderer->renderFx(fx);
    };

    // msgfx|Hello|rainbow|5   text effects: solid, rainbow, wave, typewriter, sparkle;
    // a background name (plasma, fireworks, stars, matrix) shows rainbow text over it.
    handlers["msgfx"] = [this](const std::vector<std::string>& params) {
        FxSpec fx;
        fx.text = params[0].empty() ? "-Vide-" : params[0];
        fx.fg = dmdRenderer->defaultStyle().fg;
        fx.maxCharsPerLine = dmdRenderer->defaultStyle().maxCharsPerLine;
        fx.maxFontPx = dmdRenderer->defaultStyle().maxFontPx;
        if (dmd::parseFxBackground(params[1], fx.background)) {
            fx.textFx = dmd::FxText::Rainbow;
        } else if (!dmd::parseFxText(params[1], fx.textFx)) {
            ESP_LOGW(TAG, "msgfx: unknown effect '%s'", params[1].c_str());
            return;
        }
        fx.durationMs = effectDuration(params, 2);
        dmdRenderer->renderFx(fx);
    };

    handlers["sound"] = [](const std::vector<std::string>&) {
        ESP_LOGW(TAG, "'sound' ignored: the ESP32 build has no audio output");
    };

    ////////////////////////////////////////////////////////////////////////////
    // Not ported yet
    const struct {
        const char* action;
        const char* phase;
    } pending[] = {
        {"waiter", "phase 2, attract mode"},
        {"msgcarrou", "phase 2, carousel"},
        {"time", "phase 2, clock"},
        {"testPattern", "phase 2, clock"},
        {"rldconf", "phase 5, settings"},
        {"receipconf", "phase 5, settings"},
        {"excludeFolder", "phase 3, media"},
        {"excludeFile", "phase 3, media"},
        {"rand", "phase 3, media"},
        {"demo", "phase 3, media"},
        {"gif", "phase 3, media"},
        {"gifText", "phase 3, media"},
        {"gifPath", "phase 3, media"},
        {"img", "phase 3, media"},
        {"effet", "phase 3, media"},
        {"owmzc", "phase 4, weather"},
        {"fllcn", "phase 4, weather"},
        {"meteo", "phase 4, weather"},
        {"meteoPrevi", "phase 4, weather"},
        {"edfJoursTempo", "phase 4, EDF Tempo"},
        {"perf", "phase 4, performance view"},
    };
    for (const auto& p : pending) {
        const char* action = p.action;
        const char* phase = p.phase;
        handlers[action] = [this, action, phase](const std::vector<std::string>&) {
            notPortedYet(action, phase);
        };
    }
}

// conf|Section|key:value|key:value  (DMDRenderer_Config.ApplyConfig)
void MessageHandler::applyConf(const std::vector<std::string>& params) {
    const std::string& section = params[0];
    ConfigHelper& config = ConfigHelper::getInstance();
    bool changed = false;
    for (size_t i = 1; i < params.size(); ++i) {
        const std::string& kv = params[i];
        size_t colon = kv.find(':');
        if (colon == std::string::npos) {
            ESP_LOGW(TAG, "conf: ignoring '%s' (expected key:value)", kv.c_str());
            continue;
        }
        const std::string key = kv.substr(0, colon);
        const std::string value = kv.substr(colon + 1);
        if (section == "DMDRenderer" && key == "brightness") {
            long pct = dmd::parseIntArg(value, -1);
            if (pct < 0 || pct > 100) {
                ESP_LOGW(TAG, "conf: invalid brightness '%s'", value.c_str());
                continue;
            }
            config.setBrightness(static_cast<int>(pct));
            dmdRenderer->setBrightnessPercent(static_cast<int>(pct));
            changed = true;
            ESP_LOGI(TAG, "Brightness set to %ld %%", pct);
        } else if (section == "DMDRenderer" && key == "brightnesshours") {
            // Applied hour by hour once the clock is ported (phase 2); stored now.
            config.setBrightnessHours(value);
            changed = true;
        } else {
            ESP_LOGW(TAG, "conf: [%s] %s is not supported on the ESP32 yet (phase 5, settings)",
                     section.c_str(), key.c_str());
        }
    }
    if (changed) {
        config.saveConfigFile();
    }
}

void MessageHandler::handleMessage(const std::string& message) {
    ESP_LOGD(TAG, "Handling message: %s", message.c_str());

    dmd::Command command;
    if (!dmd::parseCommand(message, command)) {
        ESP_LOGW(TAG, "Malformed message: %s", message.c_str());
        return;
    }

    // Like the original: every accepted message stops the current animation first,
    // even if its own arguments turn out to be missing.
    dmdRenderer->interrupt();

    if (command.args.size() < dmd::minArgs(command.action)) {
        ESP_LOGE(TAG, "Missing arguments for '%s': got %u, expected %u", command.action.c_str(),
                 static_cast<unsigned>(command.args.size()),
                 static_cast<unsigned>(dmd::minArgs(command.action)));
        return;
    }

    auto handler = handlers.find(command.action);
    if (handler == handlers.end()) {
        ESP_LOGW(TAG, "Unknown action received: %s", command.action.c_str());
        return;
    }
    handler->second(command.args);
}

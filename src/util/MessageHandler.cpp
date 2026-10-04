#include "MessageHandler.h"

#include <esp_sleep.h>

#include "ConfigHelper.h"
#include "Settings.h"
#include "Storage.h"
#include "render/OnlineScenes.h"
#include "core/Fx.h"
#include "core/Media.h"
#include "core/Protocol.h"
#include "esp_log.h"

static const char* TAG = "MessageHandler";

MessageHandler::MessageHandler(DMDRenderer* renderer, AttractController* attract, TimeService* timeService,
                               MediaLibrary* media, OnlineService* online)
    : dmdRenderer(renderer), attract(attract), timeService(timeService), media(media), online(online) {
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

    // msgimg|text|/Medias/Patterns/2.png: text centred over a PNG.
    handlers["msgimg"] = [this](const std::vector<std::string>& params) {
        const std::string png = media->resolve(params[1], dmd::media::kPatterns);
        if (png.empty()) {
            ESP_LOGW(TAG, "msgimg: '%s' not found, showing the text only", params[1].c_str());
            dmdRenderer->renderText(params[0]);
            return;
        }
        dmdRenderer->renderImage(png, 0, params[0]);
    };

    // testFont|Font.ttf: the ESP32 has no TrueType fonts, show the sample with the built-in fonts.
    handlers["testFont"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "testFont '%s': TrueType fonts are not available, using built-in fonts",
                 params[0].c_str());
        dmdRenderer->renderText("Test de la font");
    };

    // soundeffet|text|gif|sound
    handlers["soundeffet"] = [this](const std::vector<std::string>& params) {
        playEffect(params[0], params[1], params[2]);
    };

    // effet|12: effect from /effets.txt ("id|name|text|gif|sound" per line).
    handlers["effet"] = [this](const std::vector<std::string>& params) {
        dmd::Effect effect;
        if (!media->findEffect(params[0], effect)) {
            ESP_LOGW(TAG, "effet: no effect '%s' in %s", params[0].c_str(), MediaLibrary::kEffectsFile);
            return;
        }
        playEffect(effect.text, effect.gif, effect.sound);
    };

    ////////////////////////////////////////////////////////////////////////////
    // GIFs and images (phase 3)

    // gif|Gifs/Gif.gif|2
    handlers["gif"] = [this](const std::vector<std::string>& params) {
        const std::string path = media->resolve(params[0], dmd::media::kGifs);
        if (path.empty()) {
            ESP_LOGW(TAG, "gif: '%s' not found", params[0].c_str());
            return;
        }
        dmdRenderer->renderGif(path, params.size() == 2 ? holdArg(params, 1) : 0);
    };

    // gifPath|/Medias/Gifs/x.gif|2
    handlers["gifPath"] = [this](const std::vector<std::string>& params) {
        const std::string path = dmd::mapPiPath(params[0]);
        if (!storageExists(path)) {
            ESP_LOGW(TAG, "gifPath: '%s' (%s) not found", params[0].c_str(), path.c_str());
            return;
        }
        dmdRenderer->renderGif(path, params.size() == 2 ? holdArg(params, 1) : 0);
    };

    // gifText|Gifs/Gif.gif|Bla bla bla|2 (the original slept action[3] with 3 arguments, an
    // IndexError; the 3rd argument is the hold here)
    handlers["gifText"] = [this](const std::vector<std::string>& params) {
        if (params[0].empty()) {
            dmdRenderer->renderText("Veuillez faire un choix pour votre gif");
            return;
        }
        if (params[1].empty()) {
            dmdRenderer->renderText("Veuillez indiquer un texte a afficher");
            return;
        }
        const std::string path = media->resolve(params[0], dmd::media::kGifs);
        if (path.empty()) {
            ESP_LOGW(TAG, "gifText: '%s' not found", params[0].c_str());
            return;
        }
        dmdRenderer->renderGif(path, params.size() == 3 ? holdArg(params, 2) : 0, params[1]);
    };

    // img|Images/Image.png|2 ("WELK.OME" is the welcome image)
    handlers["img"] = [this](const std::vector<std::string>& params) {
        if (params[0].empty()) {
            dmdRenderer->renderText("Veuillez faire un choix pour votre image");
            return;
        }
        const std::string wanted = params[0] == "WELK.OME" ? "Raspy2DMD.png" : params[0];
        const std::string path = media->resolve(wanted, dmd::media::kImages);
        if (path.empty()) {
            ESP_LOGW(TAG, "img: '%s' not found", params[0].c_str());
            return;
        }
        dmdRenderer->renderImage(path, params.size() == 2 ? holdArg(params, 1) : 0);
    };

    // rand|gif|2 or rand|img|2
    handlers["rand"] = [this](const std::vector<std::string>& params) {
        const uint32_t hold = params.size() == 2 ? holdArg(params, 1) : 0;
        if (params[0] == "gif") {
            const std::string path = media->randomFile(dmd::media::kGifs, ".gif");
            if (path.empty()) {
                ESP_LOGW(TAG, "rand: no GIF in %s", dmd::media::kGifs);
                return;
            }
            dmdRenderer->renderGif(path, hold);
        } else if (params[0] == "img") {
            const std::string path = media->randomFile(dmd::media::kImages, ".png");
            if (path.empty()) {
                ESP_LOGW(TAG, "rand: no PNG in %s", dmd::media::kImages);
                return;
            }
            dmdRenderer->renderImage(path, hold);
        }
    };

    // demo|gif: every GIF, each preceded by its name.
    handlers["demo"] = [this](const std::vector<std::string>& params) {
        if (params[0] != "gif") {
            return;
        }
        constexpr size_t kMaxDemoGifs = 200;
        const std::vector<std::string> gifs = media->allGifs();
        for (size_t i = 0; i < gifs.size() && i < kMaxDemoGifs; ++i) {
            dmdRenderer->renderText(dmd::baseName(gifs[i]), 500);
            dmdRenderer->renderGif(gifs[i], 500);
        }
    };

    // excludeFolder|DirName|DirPath and excludeFile|FileName|FilePath (toggle)
    handlers["excludeFolder"] = [this](const std::vector<std::string>& params) {
        media->toggleExclusion(true, params[0], params[1]);
    };
    handlers["excludeFile"] = [this](const std::vector<std::string>& params) {
        media->toggleExclusion(false, params[0], params[1]);
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

    ////////////////////////////////////////////////////////////////////////////
    // Clock, attract mode and carousel (phase 2)

    // time|start or time|stop (an optional 2nd argument is ignored, as it was by RunTime())
    handlers["time"] = [this](const std::vector<std::string>& params) {
        if (params[0] == "start") {
            if (ConfigHelper::getInstance().getSettingInt("ClockRenderer", "showing_datehours", 2) == 0) {
                ESP_LOGI(TAG, "time|start ignored: ClockRenderer.showing_datehours is 0");
                return;
            }
            dmdRenderer->renderClock(settings::clockSpec());
        } else {
            dmdRenderer->clear();
        }
    };

    // testPattern|Modele01.png: clock preview over that pattern.
    handlers["testPattern"] = [this](const std::vector<std::string>& params) {
        ClockSpec spec = settings::clockSpec();
        spec.mode = 2;
        spec.pattern = media->resolve(params[0], dmd::media::kPatterns);
        if (spec.pattern.empty()) {
            ESP_LOGW(TAG, "testPattern: '%s' not found", params[0].c_str());
        }
        dmdRenderer->renderClock(spec);
    };

    // waiter|start|stop|pause|resume
    handlers["waiter"] = [this](const std::vector<std::string>& params) {
        const std::string& cmd = params[0];
        if (cmd == "start" || cmd == "resume") {
            attract->start(settings::scrollOrder());
        } else {
            attract->stop();  // stop, pause
        }
    };

    // msgcarrou|start or msgcarrou|stop
    handlers["msgcarrou"] = [this](const std::vector<std::string>& params) {
        if (params[0] == "start") {
            attract->start("4");
        } else {
            attract->stop();
            dmdRenderer->clear();
        }
    };

    // rldconf: settings are read live; re-apply the ones cached at start-up.
    handlers["rldconf"] = [this](const std::vector<std::string>&) {
        dmdRenderer->defaultStyle() = settings::textStyle();
        dmdRenderer->setBrightnessPercent(ConfigHelper::getInstance().getBrightness());
        timeService->begin(settings::timezone());
        timeService->invalidate();
    };

    ////////////////////////////////////////////////////////////////////////////
    // Online data (phase 4)

    handlers["meteo"] = [this](const std::vector<std::string>&) {
        dmdRenderer->renderScene(std::unique_ptr<dmd::Scene>(new CurrentWeatherScene(
            dmdRenderer->matrix(), *online, settings::owmConfig(), dmdRenderer->defaultStyle().fg)));
    };
    handlers["meteoPrevi"] = [this](const std::vector<std::string>&) {
        dmdRenderer->renderScene(std::unique_ptr<dmd::Scene>(new ForecastScene(
            dmdRenderer->matrix(), *online, settings::owmConfig(), dmdRenderer->defaultStyle().fg)));
    };
    handlers["edfJoursTempo"] = [this](const std::vector<std::string>&) {
        const uint32_t pageMs = static_cast<uint32_t>(settings::owmConfig().seeDuringSec) * 1000u;
        dmdRenderer->renderScene(std::unique_ptr<dmd::Scene>(
            new TempoScene(dmdRenderer->matrix(), *online, pageMs, dmdRenderer->defaultStyle().fg)));
    };
    handlers["perf"] = [this](const std::vector<std::string>&) {
        dmdRenderer->renderScene(
            std::unique_ptr<dmd::Scene>(new PerfScene(dmdRenderer->matrix(), 2000, dmdRenderer->defaultStyle().fg)));
    };
    // owmzc (ZipPostCodeGeocoding) and fllcn (FindLatLonCityname) both resolve
    // OpenWeatherMap.zipcode/countrycode into cityname, lat and lon.
    handlers["owmzc"] = [this](const std::vector<std::string>&) { lookUpZipCode(); };
    handlers["fllcn"] = [this](const std::vector<std::string>&) { lookUpZipCode(); };

    handlers["sound"] = [](const std::vector<std::string>&) {
        ESP_LOGW(TAG, "'sound' ignored: the ESP32 build has no audio output");
    };

    ////////////////////////////////////////////////////////////////////////////
    // Not ported yet
    const struct {
        const char* action;
        const char* phase;
    } pending[] = {
        {"receipconf", "phase 5, settings"},
    };
    for (const auto& p : pending) {
        const char* action = p.action;
        const char* phase = p.phase;
        handlers[action] = [this, action, phase](const std::vector<std::string>&) {
            notPortedYet(action, phase);
        };
    }
}

void MessageHandler::lookUpZipCode() {
    OnlineService* service = online;
    dmdRenderer->renderScene(std::unique_ptr<dmd::Scene>(new GeoScene(
        dmdRenderer->matrix(), *online, dmdRenderer->defaultStyle().fg, [service](const dmd::GeoResult& g) {
            ConfigHelper& config = ConfigHelper::getInstance();
            config.setSetting("OpenWeatherMap", "cityname", g.name);
            config.setSetting("OpenWeatherMap", "lat", g.lat);
            config.setSetting("OpenWeatherMap", "lon", g.lon);
            config.saveConfigFile();
            service->invalidate();
            ESP_LOGI(TAG, "OpenWeatherMap location: %s (%s, %s)", g.name.c_str(), g.lat.c_str(), g.lon.c_str());
        })));
}

void MessageHandler::playEffect(const std::string& text, const std::string& gif, const std::string& sound) {
    const std::string path = gif.empty() ? "" : media->resolve(gif, dmd::media::kGifs);
    if (!gif.empty() && path.empty()) {
        ESP_LOGW(TAG, "Effect GIF '%s' not found", gif.c_str());
    }
    switch (dmd::effectKind(text, path, sound)) {
        case dmd::EffectKind::GifWithText:
            dmdRenderer->renderGif(path, 0, text);
            break;
        case dmd::EffectKind::Text:
            dmdRenderer->renderText(text);
            break;
        case dmd::EffectKind::Gif:
            dmdRenderer->renderGif(path);
            break;
        case dmd::EffectKind::SoundOnly:
            ESP_LOGW(TAG, "Effect sound '%s' ignored: no audio output", sound.c_str());
            break;
        case dmd::EffectKind::Nothing:
            break;
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
            config.setBrightnessHours(value);
            timeService->invalidate();
            changed = true;
        } else {
            // Everything else is stored and read live (see util/Settings).
            config.setSetting(section, key, value);
            changed = true;
            ESP_LOGI(TAG, "conf: [%s] %s = %s", section.c_str(), key.c_str(), value.c_str());
            if (section == "OpenWeatherMap") {
                online->invalidate();
            }
            if (section == "DMDRenderer" && key == "center_images") {
                dmdRenderer->setCenterImages(value != "0");
            }
            if (section == "ClockRenderer" && key == "timezone") {
                timeService->begin(settings::timezone());
                timeService->invalidate();
            }
        }
    }
    if (changed) {
        config.saveConfigFile();
        if (section == "TextRenderer") {
            dmdRenderer->defaultStyle() = settings::textStyle();
        }
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

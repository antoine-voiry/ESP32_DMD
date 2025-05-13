#include "MessageHandler.h"
#include "esp_log.h"

static const char* TAG = "MessageHandler";

MessageHandler::MessageHandler(DMDRenderer* renderer) : dmdRenderer(renderer) {
    ESP_LOGI(TAG, "Initializing MessageHandler");
    setupHandlers();
}


void MessageHandler::setupHandlers() {
    // Initialize message handlers map
    handlers["rebt"] = [this](const std::vector<std::string>& params) {
        ESP_LOGW(TAG, "Reboot command received - not implemented for ESP32");
        dmdRenderer->renderText("Reboot command received - not implemented for ESP32");
        // TODO: Implement proper ESP32 restart
    };

    handlers["shutdwn"] = [this](const std::vector<std::string>& params) {
        ESP_LOGW(TAG, "Shutdown command received - not implemented for ESP32");
        // TODO: Implement proper ESP32 shutdown
        dmdRenderer->renderText("Shutdown command received - not implemented for ESP32");
    };

    handlers["excludeFolder"] = [this](const std::vector<std::string>& params) {
        if(params.size() < 2) {
            ESP_LOGE(TAG, "Missing arguments for excludeFolder");
            throw std::runtime_error("Missing arguments for excludeFolder");
        }
        ESP_LOGD(TAG, "Excluding folder: %s, %s", params[0].c_str(), params[1].c_str());
        dmdRenderer->exclude(true, params[0], params[1]);
    };

    handlers["excludeFile"] = [this](const std::vector<std::string>& params) {
        if(params.size() < 2) {
            ESP_LOGE(TAG, "Missing arguments for excludeFile");
            throw std::runtime_error("Missing arguments for excludeFile");
        }
        if(dmdRenderer == nullptr) {
            ESP_LOGE(TAG, "DMDRenderer not initialized");
            throw std::runtime_error("DMDRenderer not initialized");
        }
        ESP_LOGD(TAG, "Excluding file: %s, %s", params[0].c_str(), params[1].c_str());
        dmdRenderer->exclude(false, params[0], params[1]);
    };

    handlers["msg"] = [this](const std::vector<std::string>& params) {
        if(params.empty()) {
            ESP_LOGE(TAG, "Missing arguments for msg");
            throw std::runtime_error("Missing arguments for msg");
        }
        std::string text = params[0].empty() ? "-Vide-" : params[0];
        ESP_LOGI(TAG, "Rendering message: %s", text.c_str());
        dmdRenderer->renderText(text);
        if(params.size() > 1) {
            ESP_LOGE(TAG, "Delaying for %s seconds", params[1].c_str());
            delay(std::stoi(params[1]) * 1000);
        }
    };

    handlers["score"] = [this](const std::vector<std::string>& params) {
        if(params.empty()) {
            ESP_LOGE(TAG, "Missing arguments for score");
            throw std::runtime_error("Missing arguments for score");
        }
        ESP_LOGI(TAG, "Rendering score: %s", params[0].c_str());
        dmdRenderer->renderText(params[0], true);
        if(params.size() > 1) {
            ESP_LOGE(TAG, "Delaying for %s seconds", params[1].c_str());
            delay(std::stoi(params[1]) * 1000);
        }
    };
    handlers["owmzc"] = [this](const std::vector<std::string>& params) { 
        ESP_LOGI(TAG, "OWMZC command received"); 
        // TODO: Implement Open Weather Map Zone Coverage
    };

    handlers["fllcn"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "FLLCN command received");
        // TODO: Implement Fall/Winter seasonal change
    };

    handlers["meteo"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Meteo command received");
        // TODO: Implement weather display
    };
    handlers["meteoPrevi"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Meteo forecast command received");
        // TODO: Implement weather forecast display
    };
    handlers["receipconf"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Receipt configuration command received");
        // TODO: Implement configuration receipt handling
    };
    handlers["rldconf"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Reload configuration command received");
        // TODO: Implement configuration reload
    };
    handlers["conf"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Configuration command received");
        // TODO: Implement configuration handling
    };
    handlers["waiter"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Waiter command received");
        // TODO: Implement waiter mode
    };
    handlers["msgmove"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Message move command received");
        // TODO: Implement moving message display
    };
    handlers["msgmovebcl"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Message move loop command received");
        // TODO: Implement looping message display
    };
    handlers["msgcarrou"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Message carousel command received");
        // TODO: Implement carousel message display
    };
    handlers["msgcolor"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Message color command received");
        // TODO: Implement colored message display
    };
    handlers["msgimg"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Message with image command received");
        // TODO: Implement message with image display
    };
    handlers["testFont"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Test font command received");
        // TODO: Implement font testing
    };
    handlers["testPattern"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Test pattern command received");
        // TODO: Implement pattern testing
    };
    handlers["rand"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Random command received");
        // TODO: Implement random content display
    };
    handlers["demo"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Demo command received");
        // TODO: Implement demo mode
    };
    handlers["gif"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "GIF command received");
        // TODO: Implement GIF display
    };
    handlers["gifText"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "GIF with text command received");
        // TODO: Implement GIF with text display
    };
    handlers["gifPath"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "GIF path command received");
        // TODO: Implement GIF path handling
    };
    handlers["img"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Image command received");
        // TODO: Implement image display
    };
    handlers["time"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Time command received");
        // TODO: Implement time display
    };
    handlers["sound"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Sound command received");
        // TODO: Implement sound playback
    };
    handlers["effet"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Effect command received");
        // TODO: Implement visual effects
    };
    handlers["soundeffet"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Sound effect command received");
        // TODO: Implement sound effects
    };
    handlers["perf"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "Performance test command received");
        // TODO: Implement performance testing
    };
    handlers["edfJoursTempo"] = [this](const std::vector<std::string>& params) {
        ESP_LOGI(TAG, "EDF Tempo Days command received");
        // TODO: Implement EDF Tempo Days display
    };

}

void MessageHandler::handleMessage(const std::string& message) {
    ESP_LOGD(TAG, "Handling message: %s", message.c_str());

    auto parts = parseMessage(message);
    if(parts.empty()) {
        ESP_LOGW(TAG, "Received empty message");
        return;
    }

    std::string action = parts[0];
    parts.erase(parts.begin());
    
    try {
        auto handler = handlers.find(action);
        if(handler != handlers.end()) {
            ESP_LOGD(TAG, "Executing handler for action: %s", action.c_str());
            handler->second(parts);
        } else {
            ESP_LOGW(TAG, "Unknown action received: %s", action.c_str());
        }
    } catch(const std::exception& e) {
        ESP_LOGE(TAG, "Error handling message: %s", e.what());
    }
}

std::vector<std::string> MessageHandler::parseMessage(const std::string& message) {
    ESP_LOGD(TAG, "Parsing message: %s", message.c_str());
    std::vector<std::string> parts;
    std::stringstream ss(message);
    std::string item;

    while(std::getline(ss, item, '|')) {
        parts.push_back(item);
    }

    return parts;
}
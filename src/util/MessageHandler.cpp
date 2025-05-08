#include "MessageHandler.h"
#include "esp_log.h"

static const char* TAG = "MessageHandler";

MessageHandler::MessageHandler(DMDRenderer* renderer) : dmdRenderer(renderer) {
    ESP_LOGI(TAG, "Initializing MessageHandler");
    
    // Initialize message handlers map
    handlers["rebt"] = [](const std::vector<std::string>& params) {
        ESP_LOGW(TAG, "Reboot command received - not implemented for ESP32");
        // TODO: Implement proper ESP32 restart
    };

    handlers["shutdwn"] = [](const std::vector<std::string>& params) {
        ESP_LOGW(TAG, "Shutdown command received - not implemented for ESP32");
        // TODO: Implement proper ESP32 shutdown
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
            ESP_LOGD(TAG, "Delaying for %s seconds", params[1].c_str());
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
            ESP_LOGD(TAG, "Delaying for %s seconds", params[1].c_str());
            delay(std::stoi(params[1]) * 1000);
        }
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
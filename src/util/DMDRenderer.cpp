#include "DMDRenderer.h"
#include "esp_log.h"
#include "Darthasciiart.h"

// Define log tag for this class
static const char* TAG = "DMDRenderer";

DMDRenderer::DMDRenderer(Hub75_Matrix* matrix) {
    ESP_LOGI(TAG, "Initializing DMDRenderer");
    standalone = false;
    brightness = 100;
    _dmd = matrix;
    renderFirstStart();
    ESP_LOGI(TAG, "Initializing DMDRenderer done");
}

DMDRenderer::~DMDRenderer() {
    ESP_LOGI(TAG, "Destroying DMDRenderer");
}

void DMDRenderer::renderText(const std::string& text) {
    if (text.empty()) {
        ESP_LOGW(TAG, "Attempted to render empty text");
        return;
    }
    _dmd->clearScreen();
    ESP_LOGD(TAG, "Rendering text: %s", text.c_str());
    _dmd->drawCenteredTextRandomColor(1, text.c_str(),1);
}

//TODO: remove what isDartScore  is used for
void DMDRenderer::renderText(const std::string& text, bool isDartScore) {
    ESP_LOGD(TAG, "Rendering text with val=%d: %s", isDartScore, text.c_str());
    _dmd->clearScreen();
    _dmd->drawCenteredTextRandomColor(1, text.c_str(),1);
}

void DMDRenderer::stop(const std::string& message) {
    ESP_LOGI(TAG, "Stopping renderer with message: %s", message.c_str());
    _dmd->fillScreen(0);
}

void DMDRenderer::applyConfig(const std::vector<std::string>& config) {
    ESP_LOGI(TAG, "Applying configuration");
    for(const auto& cfg : config) {
        size_t pos = cfg.find(':');
        if(pos != std::string::npos) {
            std::string key = cfg.substr(0, pos);
            std::string value = cfg.substr(pos + 1);
            
            ESP_LOGD(TAG, "Config: %s = %s", key.c_str(), value.c_str());
            
            if(key == "brightness") {
                try {
                    brightness = std::stoi(value);
                    _dmd->setBrightness(brightness);
                    ESP_LOGI(TAG, "Brightness set to %d", brightness);
                } catch (const std::exception& e) {
                    ESP_LOGE(TAG, "Failed to parse brightness value: %s", e.what());
                }
            }
        } else {
            ESP_LOGE(TAG, "Invalid config format: %s", cfg.c_str());
        }
    }
}

void DMDRenderer::update() {
    ESP_LOGV(TAG, "Update called"); // Very verbose logging
    _dmd->update();
}

bool DMDRenderer::scoreReceived(const std::string& score) {
    if(score.empty()) {
        ESP_LOGW(TAG, "Received empty score");
        return false;
    }
    ESP_LOGI(TAG, "Score received: %s", score.c_str());
    
    return true;
}

void DMDRenderer::exclude(bool isFolder, const std::string& name, const std::string& path) {
    ESP_LOGI(TAG, "Excluding %s: %s from path: %s", isFolder ? "folder" : "file", name.c_str(), path.c_str());
    renderText("Excluding " + name);
    delay(DELAY_CRITICAL_MSG);
}

void DMDRenderer::renderFirstStart() {
    renderText(DART_ASCII_ART_LINE_0);
    renderText(DART_ASCII_ART_LINE_1);
    renderText(DART_ASCII_ART_LINE_2);
    renderText(DART_ASCII_ART_LINE_3);
    _dmd->swapBuffers(true);

    delay(DELAY_CRITICAL_MSG);

}

void DMDRenderer::swapBuffers(bool copy) {
    ESP_LOGI(TAG, "Swapping buffers");
    _dmd->swapBuffers(copy);
}

void DMDRenderer::renderScore(const std::string& score) {
    ESP_LOGI(TAG, "Rendering score: %s", score.c_str());
    _dmd->clearScreen();
    _dmd->drawCenteredTextRandomColor(1, score.c_str(),1);
    delay(DELAY_CRITICAL_MSG);
}
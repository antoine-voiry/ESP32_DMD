#include "Hub75_Matrix.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <esp_log.h>

static const char* TAG = "Hub75_Matrix";

Hub75_Matrix::Hub75_Matrix(int panel_width, int panel_height, int chain_length) :
    _panel_width(panel_width),
    _panel_height(panel_height),
    _chain_length(chain_length) {
    ESP_LOGI(TAG, "Creating Hub75_Matrix with width: %d, height: %d, chain: %d", panel_width, panel_height, chain_length);
}

Hub75_Matrix::~Hub75_Matrix() {
    if (dma_display) {
        delete dma_display;
        dma_display = nullptr;
        ESP_LOGI(TAG, "Hub75_Matrix destroyed");
    }
}

bool Hub75_Matrix::begin(int pin_a, int pin_b, int pin_c, int pin_d, int pin_e, int pin_lat, int pin_oe, int pin_clk) {
    ESP_LOGI(TAG, "Beginning Hub75_Matrix");
    HUB75_I2S_CFG mxconfig(
        _panel_width,  // Panel width
        _panel_height, // Panel height
        _chain_length  // chain length
    );

    // Display matrix setup
    dma_display = new MatrixPanel_I2S_DMA(mxconfig);

    dma_display->begin(pin_a, pin_b, pin_c, pin_d, pin_e, pin_lat, pin_oe, pin_clk);
    dma_display->clearScreen();
    dma_display->fillScreen(dma_display->color565(0, 0, 0));
    return true;
}

void Hub75_Matrix::fillScreen(uint16_t color) {
    if (dma_display) {
        dma_display->fillScreen(color);
    }
}

void Hub75_Matrix::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (dma_display) {
        dma_display->drawPixel(x, y, color);
    }
}

void Hub75_Matrix::swapBuffers(bool copy) {
    if (dma_display) {
        dma_display->flipDMABuffer();
    }
}

MatrixPanel_I2S_DMA* Hub75_Matrix::getMatrixPanel() {
    return dma_display;
}
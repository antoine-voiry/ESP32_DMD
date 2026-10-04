#include "Hub75_Matrix.h"
#include <esp_log.h>

static const char* TAG = "Hub75_Matrix";

Hub75_Matrix::Hub75_Matrix(int cols, int rows, int chain) : _cols(cols), _rows(rows), _chain(chain) {
    ESP_LOGI(TAG, "Creating Hub75_Matrix %dx%d, chain: %d", _cols, _rows, _chain);
    if (_rows > 32 && E_PIN < 0) {
        ESP_LOGE(TAG, "%d-row panels need the E address line: build with -DE_PIN=<gpio>", _rows);
    }

    HUB75_I2S_CFG::i2s_pins pins = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN,
                                    A_PIN,  B_PIN,  C_PIN,  D_PIN,  E_PIN,  LAT_PIN, OE_PIN, CLK_PIN};
    HUB75_I2S_CFG mxconfig(static_cast<uint16_t>(_cols), static_cast<uint16_t>(_rows), static_cast<uint16_t>(_chain), pins);
    mxconfig.driver = HUB75_I2S_CFG::FM6126A;
    // Animations redraw full frames; double buffering avoids tearing and flicker while scrolling.
    mxconfig.double_buff = true;

    _matrix = new MatrixPanel_I2S_DMA(mxconfig);
    if (!_matrix->begin()) {
        ESP_LOGE(TAG, "HUB75 DMA init failed (not enough DMA memory?)");
    }
    _matrix->setBrightness8(230);  // ~90 %, the Raspy2DMD default
    clearScreen();
}

Hub75_Matrix::~Hub75_Matrix() {
    delete _matrix;
    _matrix = nullptr;
}

uint16_t Hub75_Matrix::color(uint8_t r, uint8_t g, uint8_t b) const {
    return _matrix ? _matrix->color565(r, g, b) : 0;
}

void Hub75_Matrix::fillScreen(uint16_t color) {
    if (_matrix) {
        _matrix->fillScreen(color);
    }
}

void Hub75_Matrix::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (_matrix) {
        _matrix->drawPixel(x, y, color);
    }
}

void Hub75_Matrix::present() {
    if (_matrix) {
        _matrix->flipDMABuffer();
    }
}

void Hub75_Matrix::clearScreen() {
    if (_matrix) {
        _matrix->fillScreen(0);
        _matrix->flipDMABuffer();
    }
}

void Hub75_Matrix::setBrightness(uint8_t brightness) {
    if (_matrix) {
        _matrix->setBrightness8(brightness);
    }
}

void Hub75_Matrix::setBrightnessPercent(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    setBrightness(static_cast<uint8_t>(percent * 255 / 100));
}

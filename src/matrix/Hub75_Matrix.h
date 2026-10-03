#ifndef HUB75_MATRIX_H
#define HUB75_MATRIX_H

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Arduino.h>

// +---------+   Panel - ESP32 pins
//|  R1 G1  |    R1   - IO25      G1   - IO26
//|  B1 GND |    B1   - IO27      GND  - GND
//|  R2 G2  |    R2   - IO21      G2   - IO22
//|  B2 GND |    B2   - IO23      GND  - GND
//|   A B   |    A    - IO12      B    - IO16
//|   C D   |    C    - IO17      D    - IO18
//| CLK LAT |    CLK  - IO15      LAT  - IO32
//|  OE GND |    OE   - IO33      GND  - GND
#define R1_PIN  GPIO_NUM_25
#define G1_PIN  GPIO_NUM_26
#define B1_PIN  GPIO_NUM_27

#define R2_PIN  GPIO_NUM_21
#define G2_PIN  GPIO_NUM_22
#define B2_PIN  GPIO_NUM_23

#define A_PIN   GPIO_NUM_12
#define B_PIN   GPIO_NUM_16
#define C_PIN   GPIO_NUM_17
#define D_PIN   GPIO_NUM_18
#define E_PIN   -1  // not assigned (needed for 64-row panels)

#define LAT_PIN GPIO_NUM_32
#define OE_PIN  GPIO_NUM_33
#define CLK_PIN GPIO_NUM_15

#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32   // Panel height of 64 will required PIN_E to be defined.
#define PANELS_NUMBER 1   // Number of chained panels

// Thin wrapper around the HUB75 DMA driver.
// The panel is double buffered: draw a full frame, then call present() to show it.
class Hub75_Matrix {
public:
    Hub75_Matrix();
    ~Hub75_Matrix();

    int width() const { return PANEL_WIDTH * PANELS_NUMBER; }
    int height() const { return PANEL_HEIGHT; }

    uint16_t color(uint8_t r, uint8_t g, uint8_t b) const;
    void fillScreen(uint16_t color);
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    // Shows the frame drawn since the last present().
    void present();
    // Fills the screen with black and shows it.
    void clearScreen();

    // 0..255, as the driver expects.
    void setBrightness(uint8_t brightness);
    // 0..100 %, as Raspy2DMD's "brightness" setting.
    void setBrightnessPercent(int percent);

    MatrixPanel_I2S_DMA* getMatrixPanel() { return _matrix; }

private:
    MatrixPanel_I2S_DMA* _matrix = nullptr;
};

#endif

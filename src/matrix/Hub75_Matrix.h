#ifndef HUB75_MATRIX_H
#define HUB75_MATRIX_H

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <FastLED.h>
#include <Arduino.h>
#include "xtensa/core-macros.h"

// +---------+   Panel - ESP32 pins
//|  R1 G1  |    R1   - IO25      G1   - IO26
//|  B1 GND |    B1   - IO27      GND  - GND
//|  R2 G2  |    R2   - IO21      G2   - IO22
//|  B2 GND |    B2   - IO23      GND  - GND
//|   A B   |    A    - IO12      B    - IO16
//|   C D   |    C    - IO17      D    - IO18
//| CLK LAT |    CLK  - IO15      LAT  - IO32
//|  OE GND |    OE   - IO33      GND  - GND
// ADDX is output directly using GPIO
#define GPIOPINOUT 3300
#define CLKS_DURING_LATCH   0 
#define MATRIX_I2S_MODE I2S_PARALLEL_BITS_16
#define MATRIX_DATA_STORAGE_TYPE uint16_t
#define R1_PIN  GPIO_NUM_25 // was uint8_t pinR1 = 25;
#define G1_PIN  GPIO_NUM_26 //was  uint8_t pinG1 = 26;
#define B1_PIN  GPIO_NUM_27  //uint8_t pinB1 = 27;

#define R2_PIN  GPIO_NUM_21 //uint8_t pinR2 = 21;
#define G2_PIN  GPIO_NUM_22 //uint8_t pinG2 = 22;
#define B2_PIN  GPIO_NUM_23 //uint8_t pinB2 = 23;

#define A_PIN   GPIO_NUM_12 //uint8_t pinA = 12;
#define B_PIN   GPIO_NUM_16 //uint8_t pinB = 16;
#define C_PIN   GPIO_NUM_17 //uint8_t pinC = 17;
#define D_PIN   GPIO_NUM_18 //uint8_t pinD = 18;
#define E_PIN   -1 //not assigned

#define LAT_PIN GPIO_NUM_32 //uint8_t pinLAT = 32;
#define OE_PIN  GPIO_NUM_33 //uint8_t pinOE = 33;

#define CLK_PIN GPIO_NUM_15 //uint8_t pinCLK = 15;
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32         // Panel height of 64 will required PIN_E to be defined.
#define PANEL_CHAIN 1         // Total number of panels chained one to another
#define PANELS_NUMBER 1         // Number of chained panels, if just a single panel, obviously set to 1

class Hub75_Matrix {
public:
    Hub75_Matrix();
    ~Hub75_Matrix();

    void fillScreen(uint16_t color);
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void swapBuffers(bool copy = false);
    void buffclear(CRGB *buf);
    void drawTextRandomColor(int colorWheelOffset, const char *str, int size);
    void drawTextRandomColorAtPosition(int colorWheelOffset, int xposition, int yposition, const char *str, int size);
    void setBrightness(uint8_t brightness);
    void startCarrousel(int colorWheelOffset, int position, int speed, boolean LeftRight, int fontSize, const char *str);
    void stopCarrousel();
    void update();
    uint16_t colorWheel(uint8_t pos);
    
    // Getters for private members
    int getPanelWidth() const { return _panel_width; }
    int getPanelHeight() const { return _panel_height; }
    int getChainLength() const { return _chain_length; }
    int getPanelWidthChain() const { return _panel_width_chain; }
    int getNumLeds() const { return _num_leds; }
    CRGB* getLedBuffer() const { return _ledbuff; }

    MatrixPanel_I2S_DMA* getMatrixPanel(); // Add this getter
    void clearScreen();

private:
    MatrixPanel_I2S_DMA *_matrix = nullptr;
    // gradient buffer
    CRGB *_ledbuff = nullptr;
    int _panel_width = PANEL_WIDTH;
    int _panel_height = PANEL_HEIGHT;
    int _chain_length = PANEL_CHAIN;    
    int _panel_width_chain = PANEL_WIDTH * PANEL_CHAIN;
    int _num_leds = _panel_width_chain * PANEL_HEIGHT;
};

#endif
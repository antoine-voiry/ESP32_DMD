#ifndef MATRIX_HELPER
#define MATRIX_HELPER

#define USE_ADAFRUIT_GFX_LAYERS
#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <WiFiManager.h>
#include <Fonts/TomThumb.h>
#include <Fonts/Org_01.h>
#include <Fonts/Picopixel.h>
#include <FastLED.h>
//#include <TomThumb.h>
/**
 * This defines what needs to be included
*/
//File main_inclue.h
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
//#define DEBUG_PINS_ENABLED
//#define DEBUG_1_GPIO    GPIO_NUM_18
//#define DEBUG_2_GPIO    GPIO_NUM_23

#include <MatrixHardware_ESP32_V0.h>                // This file contains multiple ESP32 hardware configurations, 

// Assign human-readable names to some common 16-bit color values:
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
//edit the file to define GPIOPINOUT (or add #define GPIOPINOUT with a hardcoded number before this #include)
//#include "MatrixHardware_Custom.h"                  // Copy an existing MatrixHardware file to your Sketch directory, rename, customize, and you can include it like this
#include <SmartMatrix.h>

#define COLOR_DEPTH 24                  // Choose the color depth used for storing pixels in the layers: 24 or 48 (24 is good for most sketches - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24)
// Teensy 3.0 has the LED on pin 13
const int ledPin = 13;

void drawRandomPixelMatrix ();
void setupMatrix (void);
void setupMatrixStartBackground(void);
void drawClock(void) ;
void drawWIFIDetails(void);
void drawRandomTransition(void);
void drawdrawHorizonalBarchart();
void cleanLayersAndSwapBuffers();
void cleanLayersWithoutSwapBuffer();

#endif
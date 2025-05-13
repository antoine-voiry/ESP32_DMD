#include "Hub75_Matrix.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <esp_log.h>

static const char* TAG = "Hub75_Matrix";

Hub75_Matrix::Hub75_Matrix() {
    ESP_LOGI(TAG, "Creating Hub75_Matrix with width: %d, height: %d, chain: %d", _panel_width, _panel_height, _chain_length);

    HUB75_I2S_CFG::i2s_pins _pins={R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
    HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER,_pins);
    mxconfig.gpio.e = E_PIN;  // Set E_PIN to -1 if not used
    mxconfig.driver = HUB75_I2S_CFG::FM6126A;
  
    _matrix = new MatrixPanel_I2S_DMA(mxconfig);
    _matrix->begin();
    _matrix->setBrightness8(255);
  
    _ledbuff = (CRGB *)malloc(_num_leds * sizeof(CRGB));  // allocate buffer for some tests
    buffclear(_ledbuff);  
    // draw a blue circle
    _matrix->drawCircle(10, 10, 10, _matrix->color444(0, 0, 15));
    delay(500);

    // fill a violet circle
    _matrix->fillCircle(40, 21, 10, _matrix->color444(15, 0, 15));
    delay(500);

    // fill the screen with 'black'
    _matrix->fillScreen(_matrix->color444(0, 0, 0));    
}

Hub75_Matrix::~Hub75_Matrix() {
    if (_matrix) {
        delete _matrix;
        _matrix = nullptr;
        ESP_LOGI(TAG, "Hub75_Matrix destroyed");
    }
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

void Hub75_Matrix::swapBuffers(bool copy) {
    if (_matrix) {
        _matrix->flipDMABuffer();
    }
}

MatrixPanel_I2S_DMA* Hub75_Matrix::getMatrixPanel() {
    return _matrix;
}

void Hub75_Matrix::buffclear(CRGB *buf){
    memset(buf, 0x00, _num_leds * sizeof(CRGB)); // flush buffer to black  
}

/**
 * @brief Draws text on the matrix with a random color for each character.
 * @param colorWheelOffset Offset for the color wheel.
 * @param str The string to draw.   
 * @param size Desired text size. 1 is default 6x8, 2 is 12x16, 3 is 18x24, etc.
 * @note The text is drawn at the position (5, 5)
 *  with a between each character defined by the size (1 pixel of 1, 2 for 2, etc...). 
 */
void Hub75_Matrix::drawTextRandomColor(int colorWheelOffset, const char *str, int size){
    // draw some text
    //Desired text size. 1 is default 6x8, 2 is 12x16, 3 is 18x24, etc
    _matrix->setTextSize(size);     // size 1 == 8 pixels high
    _matrix->setTextWrap(false); // Don't wrap at end of line - will do ourselves
  
    _matrix->setCursor(5, 5);    // start at top left, with 5,5 pixel of spacing
    uint8_t w = 0;
    
    for (w=0; w<strlen(str); w++) {
      _matrix->setTextColor(colorWheel((w*32)+colorWheelOffset));
      _matrix->print(str[w]);
    }
}

/**
 * @brief Draws text on the matrix with a random color for each character.
 * @param colorWheelOffset Offset for the color wheel.
 * @param xposition The x position to start drawing the text.
 * @param yposition The y position to start drawing the text.   
 * @param str The string to draw.   
 * @param size Desired text size. 1 is default 6x8, 2 is 12x16, 3 is 18x24, etc.
 * @note The text is drawn at the position provided by the parameters xposition and yposition
 *  with a between each character defined by the size (1 pixel of 1, 2 for 2, etc...). 
 */
void Hub75_Matrix::drawTextRandomColorAtPosition(int colorWheelOffset, int xposition, int yposition, const char *str, int size){
    // draw some text
    _matrix->setTextSize(size);     // size 1 == 8 pixels high
    _matrix->setTextWrap(false); // Don't wrap at end of line - will do ourselves
  
    _matrix->setCursor(xposition, yposition);    // start at top left, with 5,5 pixel of spacing
    uint8_t w = 0;
  
    for (w=0; w<strlen(str); w++) {
      _matrix->setTextColor(colorWheel((w*32)+colorWheelOffset));
      _matrix->print(str[w]);
    }
}

/**
 * @brief Generates a color based on the position in the color wheel.
 * @param pos The position in the color wheel (0-255).
 * @return The color in RGB565 format.
 * @note This function generates a color based on the position in the color wheel.  
 */
uint16_t Hub75_Matrix::colorWheel(uint8_t pos) {
    if(pos < 85) {
      return _matrix->color565(pos * 3, 255 - pos * 3, 0);
    } else if(pos < 170) {
      pos -= 85;
      return _matrix->color565(255 - pos * 3, 0, pos * 3);
    } else {
      pos -= 170;
      return _matrix->color565(0, pos * 3, 255 - pos * 3);
    }
  }

  /**
   * @brief Sets the brightness of the matrix.
   * @param brightness The brightness level (0-255).    
   * @note This function sets the brightness of the matrix.
   *  The brightness level can be adjusted from 0 (off) to 255 (full brightness).
   */
void Hub75_Matrix::setBrightness(uint8_t brightness) {
    if (_matrix) {
        _matrix->setBrightness8(brightness);
    }
}


/**
 *  @brief Stops the carrousel effect by clearing the screen.
 *  @note This function clears the screen and stops the carrousel effect.   
 */
void Hub75_Matrix::stopCarrousel() {
    if (_matrix) {
        _matrix->fillScreen(_matrix->color444(0, 0, 0));
        _matrix->flipDMABuffer();
    }
}   

/**
 *  @brief Starts a carrousel effect with the given parameters.
 *  @param colorWheelOffset Offset for the color wheel.
 *  @param position The y position to start drawing the text.
 *  @param speed The speed of the carrousel effect.
 *  @param leftRight Direction of the carrousel effect (true for left, false for right).
 *  @param fontSize The size of the font to use.
 *  @param str The string to draw.
 * @note This function creates a carrousel effect with the given parameters.    
 * The text will move from one side of the screen to the other,
 */
void Hub75_Matrix::startCarrousel(int colorWheelOffset, int position, int speed, boolean leftRight, int fontSize, const char *str) {
    if (_matrix) {
        int textWidth = strlen(str) * 6 * fontSize;  // Approximate width of text
        int startX = leftRight ? -textWidth : _panel_width;
        int endX = leftRight ? _panel_width : -textWidth;
        
        _matrix->setTextSize(fontSize);
        _matrix->setTextWrap(false);
        
        for (int x = startX; leftRight ? x <= endX : x >= endX; x += (leftRight ? 1 : -1)) {
            _matrix->fillScreen(_matrix->color444(0, 0, 0));
            _matrix->setCursor(x, position);
            _matrix->setTextColor(colorWheel(colorWheelOffset));
            _matrix->print(str);
            _matrix->flipDMABuffer();
            delay(speed);
        }
    }
}
void Hub75_Matrix::update() {
    /* Not implemented */
}

/**
 * @brief Clears the screen.

 */
void Hub75_Matrix::clearScreen() {
    if (_matrix) {
        _matrix->fillScreen(_matrix->color444(0, 0, 0));
        _matrix->flipDMABuffer();
    }
}

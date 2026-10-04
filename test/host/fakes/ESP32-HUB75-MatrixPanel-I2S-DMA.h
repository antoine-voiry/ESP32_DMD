#ifndef FAKE_HUB75_H
#define FAKE_HUB75_H

// The HUB75 driver as a pair of RGB565 buffers: drawing goes to the back buffer, flipDMABuffer()
// shows it. fake::panel() gives tests the last panel created.

#include <cstdint>
#include <vector>

enum gpio_num_t {
    GPIO_NUM_12 = 12, GPIO_NUM_15 = 15, GPIO_NUM_16 = 16, GPIO_NUM_17 = 17, GPIO_NUM_18 = 18,
    GPIO_NUM_21 = 21, GPIO_NUM_22 = 22, GPIO_NUM_23 = 23, GPIO_NUM_25 = 25, GPIO_NUM_26 = 26,
    GPIO_NUM_27 = 27, GPIO_NUM_32 = 32, GPIO_NUM_33 = 33
};

struct HUB75_I2S_CFG {
    enum shift_driver { SHIFTREG, FM6124, FM6126A };
    struct i2s_pins {
        int8_t r1, g1, b1, r2, g2, b2, a, b, c, d, e, lat, oe, clk;
    };
    HUB75_I2S_CFG(uint16_t w, uint16_t h, uint16_t chain, i2s_pins p)
        : mx_width(w), mx_height(h), chain_length(chain), gpio(p) {}
    uint16_t mx_width;
    uint16_t mx_height;
    uint16_t chain_length;
    i2s_pins gpio;
    shift_driver driver = SHIFTREG;
    bool double_buff = false;
};

class MatrixPanel_I2S_DMA {
public:
    explicit MatrixPanel_I2S_DMA(const HUB75_I2S_CFG& cfg);
    ~MatrixPanel_I2S_DMA();

    bool begin();
    void setBrightness8(uint8_t b) { brightness = b; }
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) const {
        return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }
    void fillScreen(uint16_t color);
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void flipDMABuffer();

    // Inspection.
    int width;
    int height;
    HUB75_I2S_CFG config;
    uint8_t brightness = 0;
    int flips = 0;
    std::vector<uint16_t> back;
    std::vector<uint16_t> shown;
    uint16_t shownAt(int x, int y) const { return shown[static_cast<size_t>(y) * width + x]; }
    int litPixels() const;
};

#endif

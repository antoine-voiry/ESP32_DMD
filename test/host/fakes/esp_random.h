#ifndef FAKE_ESP_RANDOM_H
#define FAKE_ESP_RANDOM_H
#include <cstdint>
// Deterministic sequence (fake::seedRandom()).
uint32_t esp_random();
#endif

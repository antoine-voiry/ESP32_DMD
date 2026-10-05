#ifndef FAKE_ESP_SLEEP_H
#define FAKE_ESP_SLEEP_H
// Counted in fake::deepSleeps; returns, unlike the real one.
void esp_deep_sleep_start();
#endif

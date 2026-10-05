#ifndef FAKE_ESP_LOG_H
#define FAKE_ESP_LOG_H

// Logs are dropped (set DMD_TEST_LOG=1 to print them), but their format strings are still checked.
#include <cstdio>

void fake_log(char level, const char* tag, const char* format, ...) __attribute__((format(printf, 3, 4)));

#define ESP_LOGE(tag, format, ...) fake_log('E', tag, format, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) fake_log('W', tag, format, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) fake_log('I', tag, format, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) fake_log('D', tag, format, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) fake_log('V', tag, format, ##__VA_ARGS__)

#endif

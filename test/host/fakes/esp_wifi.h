#ifndef FAKE_ESP_WIFI_H
#define FAKE_ESP_WIFI_H
typedef enum { WIFI_PS_NONE, WIFI_PS_MIN_MODEM } wifi_ps_type_t;
inline int esp_wifi_set_ps(wifi_ps_type_t) { return 0; }
#endif

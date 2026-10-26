#pragma once
#include <cstdint>
using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
constexpr int WIFI_IF_STA = 0;
#define WIFI_NVS_ENABLED 1
struct wifi_config_t {
  struct { uint8_t ssid[32]; uint8_t password[64]; } sta;
};
esp_err_t esp_wifi_get_config(int interface, wifi_config_t* config);

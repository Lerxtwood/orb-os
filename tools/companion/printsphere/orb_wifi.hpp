#pragma once

#include <algorithm>
#include "esp_wifi.h"
#include "printsphere/config_store.hpp"

namespace printsphere::orb_wifi {
// Captured once before PrintSphere switches the driver to RAM storage. The SDK
// reads Orb's ordinary Wi-Fi configuration from default NVS during initialization.
// Keep a snapshot: starting PrintSphere's AP or saving its own network must never
// change what a later ConfigStore load regards as Orb's saved network.
inline WifiCredentials saved_credentials;

inline esp_err_t capture_saved_credentials() {
  static_assert(WIFI_NVS_ENABLED, "Orb Wi-Fi import requires Wi-Fi NVS support");
  saved_credentials = {};
  wifi_config_t config = {};
  const esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &config);
  if (err != ESP_OK) return err;
  const auto* ssid = reinterpret_cast<const char*>(config.sta.ssid);
  const auto* password = reinterpret_cast<const char*>(config.sta.password);
  saved_credentials.ssid.assign(ssid, std::find(ssid, ssid + sizeof(config.sta.ssid), '\0'));
  if (!saved_credentials.ssid.empty()) {
    saved_credentials.password.assign(password, std::find(password, password + sizeof(config.sta.password), '\0'));
  }
  return ESP_OK;
}
}  // namespace printsphere::orb_wifi

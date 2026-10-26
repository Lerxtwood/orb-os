#include "printsphere/orb_wifi.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

namespace {
wifi_config_t saved_station{};
esp_err_t read_error = ESP_OK;
int reads = 0;
}

esp_err_t esp_wifi_get_config(int interface, wifi_config_t* config) {
  assert(interface == WIFI_IF_STA);
  ++reads;
  *config = saved_station;
  return read_error;
}

int main() {
  using namespace printsphere::orb_wifi;
  assert(capture_saved_credentials() == ESP_OK);
  assert(!saved_credentials.is_configured());

  std::memcpy(saved_station.sta.ssid, "Orb network", 11);
  std::memcpy(saved_station.sta.password, "Orb password", 12);
  assert(capture_saved_credentials() == ESP_OK);
  assert(saved_credentials.ssid == "Orb network");
  assert(saved_credentials.password == "Orb password");
  // Later RAM-only changes in PrintSphere must not change the fallback.
  saved_station = {};
  std::memcpy(saved_station.sta.ssid, "PrintSphere network", 19);
  assert(saved_credentials.ssid == "Orb network");
  assert(saved_credentials.password == "Orb password");
  assert(reads == 2);

  saved_station = {};
  std::memcpy(saved_station.sta.ssid, "Open network", 12);
  assert(capture_saved_credentials() == ESP_OK);
  assert(saved_credentials.ssid == "Open network");
  assert(saved_credentials.password.empty());

  // SDK fields need not be NUL terminated at their maximum length.
  std::memset(saved_station.sta.ssid, 's', sizeof(saved_station.sta.ssid));
  std::memset(saved_station.sta.password, 'p', sizeof(saved_station.sta.password));
  assert(capture_saved_credentials() == ESP_OK);
  assert(saved_credentials.ssid == std::string(32, 's'));
  assert(saved_credentials.password == std::string(64, 'p'));

  read_error = 123;
  assert(capture_saved_credentials() == 123);
  assert(!saved_credentials.is_configured());
  assert(saved_credentials.password.empty());

  read_error = ESP_OK;
  saved_station.sta.ssid[0] = 0;
  assert(capture_saved_credentials() == ESP_OK);
  assert(saved_credentials.password.empty());
  std::puts("Orb Wi-Fi import checks passed");
}

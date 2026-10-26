#pragma once
#include <string>
namespace printsphere {
struct WifiCredentials {
  std::string ssid;
  std::string password;
  bool is_configured() const { return !ssid.empty(); }
};
}

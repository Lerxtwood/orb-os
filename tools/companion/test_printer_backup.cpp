#include "printsphere/backup_files.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace printsphere::printer_backup;
namespace fs = std::filesystem;

static void write(const std::string& path, const std::string& bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << bytes;
  assert(file.good());
}

int main(int argc, char** argv) {
  assert(argc == 2);
  const fs::path directory = fs::absolute(argv[1]);
  fs::create_directories(directory);
  const auto path = (directory / "printer-settings.json").string();
  const std::string device = "aabbccddeeff";
  Settings settings, decoded;
  for (const auto& key : keys()) settings[key] = "";
  assert(!has_setup(settings));
  assert(!can_bind(settings));
  assert(encode(settings, device).empty());
  settings["prn_count"] = "0"; // Deliberately deleting all profiles is still setup.
  assert(has_setup(settings));
  settings["prn_count"] = "2";
  settings["prn_active"] = "1";
  settings["prn_0_ser"] = "printer-zero";
  settings["prn_0_host"] = "192.0.2.1";
  settings["prn_0_acc"] = "local-access";
  settings["prn_1_ser"] = "printer-one";
  settings["prn_1_name"] = "Printer \"one\" \n unicode: \xE2\x98\x83";
  settings["prn_1_mdl"] = "X1C";
  settings["prn_1_cld"] = "1";
  settings["cloud_email"] = "test@example.invalid";
  settings["cloud_pass"] = "test password";
  settings["cloud_token"] = "test session token";
  settings["cloud_region"] = "us";
  settings["source_mode"] = "cloud_only";
  assert(can_bind(settings));
  const auto bytes = encode(settings, device);
  assert(!bytes.empty());
  assert(decode(bytes, device, decoded) && decoded == settings);
  auto sparse = settings;
  for (auto it = sparse.begin(); it != sparse.end();) {
    if (it->second.empty()) it = sparse.erase(it);
    else ++it;
  }
  assert(sparse.size() < 20);
  assert(can_bind(sparse));
  assert(decode(encode(sparse, device), device, decoded) && decoded == sparse);
  sparse["wifi_pass"] = "excluded even in a sparse snapshot";
  assert(encode(sparse, device).empty());
  assert(allowed_key("prn_15_acc"));
  assert(!allowed_key("prn_16_acc"));
  assert(!allowed_key("prn_00_ser"));
  assert(!decode(bytes, "different-device", decoded));
  assert(!decode(bytes.substr(0, bytes.size() / 2), device, decoded));
  assert(!decode(bytes + "garbage", device, decoded));
  assert(!decode(std::string(kMaxFileSize + 1, 'x'), device, decoded));
  auto corrupt = bytes;
  corrupt[corrupt.find("printer-zero")] = 'X';
  assert(!decode(corrupt, device, decoded));
  auto invalid = settings;
  invalid["wifi_pass"] = "must not back up Wi-Fi";
  assert(encode(invalid, device).empty());
  invalid = settings;
  invalid["prn_active"] = "2";
  assert(!can_bind(invalid));
  invalid = settings;
  invalid["prn_count"] = "17";
  assert(!can_bind(invalid));
  invalid = settings;
  invalid["cloud_token"] = std::string(8193, 't');
  assert(!can_bind(invalid));
  auto root = json(cJSON_Parse(bytes.c_str()));
  auto* data = cJSON_GetObjectItemCaseSensitive(root.get(), "data");
  cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(data, "schema"), 2);
  cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(root.get(), "crc32"), checksum(print(data)).c_str());
  assert(!decode(print(root.get()), device, decoded));
  cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(data, "schema"), 1);
  cJSON_AddStringToObject(cJSON_GetObjectItemCaseSensitive(data, "settings"), "cloud_email", "duplicate");
  cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(root.get(), "crc32"), checksum(print(data)).c_str());
  assert(!decode(print(root.get()), device, decoded));

  // Use a caller-provided isolated fixture directory; never erase a directory tree.
  for (const char* suffix : {"", ".prev", ".tmp"}) std::remove((path + suffix).c_str());
  assert(!load_file(path, device, decoded));
  assert(save_file(path, device, settings));
  assert(load_file(path, device, decoded) && decoded == settings);
  assert(save_file(path, device, settings));
  assert(!fs::exists(path + ".prev")); // Unchanged snapshots do not rotate/write.
  auto updated = settings;
  updated["cloud_token"] = "refreshed session token";
  assert(save_file(path, device, updated));
  assert(read_file(path, device, decoded) && decoded == updated);
  assert(read_file(path + ".prev", device, decoded) && decoded == settings);
  write(path, "truncated write");
  assert(load_file(path, device, decoded) && decoded == settings);
  write(path + ".tmp", "unfinished temporary file");
  assert(load_file(path, device, decoded) && decoded == settings);
  assert(save_file(path, device, updated));
  assert(read_file(path, device, decoded) && decoded == updated);
  std::remove(path.c_str()); // Power loss after rotating primary into .prev.
  assert(load_file(path, device, decoded) && decoded == settings);
  assert(save_file(path, device, updated));
  assert(!save_file((directory / "absent-parent" / "backup.json").string(), device, settings));
  assert(load_file(path, device, decoded) && decoded == updated);
  assert(!load_file(path, "other-device", decoded));

  auto local = settings;
  local["source_mode"] = "local_only";
  assert(!can_bind(local)); // Active cloud-only printer lacks local credentials.
  local["prn_active"] = "0";
  local["cloud_email"] = local["cloud_pass"] = local["cloud_token"] = "";
  assert(can_bind(local));
  assert(decode(encode(local, device), device, decoded) && decoded == local);
  std::cout << "Printer backup: round trip, isolation, corruption, rotation, and recovery passed\n";
}

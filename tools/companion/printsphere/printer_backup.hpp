#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "cJSON.h"

namespace printsphere::printer_backup {
using Settings = std::map<std::string, std::string>;
constexpr size_t kMaxFileSize = 65536;
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
inline Json json(cJSON* value) { return Json(value, cJSON_Delete); }

template <typename Callback> inline void each_key(Callback callback) {
  for (const char* key : {"cloud_email", "cloud_pass", "cloud_region", "cloud_token",
                          "source_mode", "state_source", "prn_count", "prn_active"}) callback(key);
  for (int i = 0; i < 16; ++i)
    for (const char* field : {"ser", "host", "acc", "name", "mdl", "cld"}) {
      char key[16];
      std::snprintf(key, sizeof(key), "prn_%d_%s", i, field);
      callback(key);
    }
}
inline std::vector<std::string> keys() {
  std::vector<std::string> result;
  each_key([&](const char* key) { result.emplace_back(key); });
  return result;
}
inline bool allowed_key(const std::string& key) {
  bool found = false;
  each_key([&](const char* candidate) { if (key == candidate) found = true; });
  return found;
}
inline std::string get(const Settings& settings, const std::string& key) {
  const auto it = settings.find(key);
  return it == settings.end() ? "" : it->second;
}
inline bool number(const std::string& value, int limit, int& result) {
  if (value.empty()) { result = 0; return true; }
  if (value.size() > 2 || value.find_first_not_of("0123456789") != std::string::npos) return false;
  result = std::atoi(value.c_str());
  return result <= limit && std::to_string(result) == value;
}
inline bool valid(const Settings& settings) {
  // Missing values mean empty/default settings. Avoid allocating 16 empty
  // printer slots on the device; full snapshots from older builds still work.
  if (settings.size() > 104) return false;
  for (const auto& [key, value] : settings) {
    if (!allowed_key(key) || value.size() > 8192 || value.find('\0') != std::string::npos) return false;
  }
  int count = 0, active = 0;
  if (!number(get(settings, "prn_count"), 16, count) ||
      !number(get(settings, "prn_active"), 15, active) || (count && active >= count)) return false;
  for (int i = 0; i < count; ++i)
    if (get(settings, "prn_" + std::to_string(i) + "_ser").empty()) return false;
  const auto region = get(settings, "cloud_region"), mode = get(settings, "source_mode");
  return (region.empty() || region == "eu" || region == "us" || region == "cn") &&
         (mode.empty() || mode == "hybrid" || mode == "cloud_only" || mode == "local_only");
}
inline bool has_setup(const Settings& settings) {
  if (!get(settings, "cloud_email").empty() || !get(settings, "cloud_pass").empty() ||
      !get(settings, "cloud_token").empty() || !get(settings, "prn_count").empty()) return true;
  for (int i = 0; i < 16; ++i)
    for (const char* field : {"ser", "host", "acc"})
      if (!get(settings, "prn_" + std::to_string(i) + "_" + field).empty()) return true;
  return false;
}
inline bool can_bind(const Settings& settings) {
  if (!valid(settings)) return false;
  int count = 0, active = 0;
  number(get(settings, "prn_count"), 16, count);
  number(get(settings, "prn_active"), 15, active);
  if (!count) return false;
  const auto prefix = "prn_" + std::to_string(active) + "_";
  const bool local = !get(settings, prefix + "host").empty() && !get(settings, prefix + "acc").empty();
  const bool cloud = !get(settings, "cloud_email").empty() &&
      (!get(settings, "cloud_pass").empty() || !get(settings, "cloud_token").empty());
  const auto mode = get(settings, "source_mode");
  return mode == "local_only" ? local : mode == "cloud_only" ? cloud : local || cloud;
}
inline std::string print(cJSON* value) {
  char* raw = cJSON_PrintUnformatted(value);
  if (!raw) return {};
  std::string result(raw);
  cJSON_free(raw);
  return result;
}
inline std::string checksum(const std::string& value) {
  uint32_t crc = 0xFFFFFFFFU;
  for (unsigned char byte : value) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
  }
  char out[9];
  std::snprintf(out, sizeof(out), "%08lx", static_cast<unsigned long>(~crc));
  return out;
}
inline std::string encode(const Settings& settings, const std::string& device) {
  if (!can_bind(settings)) return {};
  auto data = json(cJSON_CreateObject()), values = json(cJSON_CreateObject()), outer = json(cJSON_CreateObject());
  if (!data || !values || !outer) return {};
  if (!cJSON_AddNumberToObject(data.get(), "schema", 1) ||
      !cJSON_AddStringToObject(data.get(), "device", device.c_str())) return {};
  for (const auto& [key, value] : settings)
    if (!cJSON_AddStringToObject(values.get(), key.c_str(), value.c_str())) return {};
  if (!cJSON_AddItemToObject(data.get(), "settings", values.get())) return {};
  values.release();
  const auto payload = print(data.get());
  if (payload.empty() || !cJSON_AddStringToObject(outer.get(), "crc32", checksum(payload).c_str())) return {};
  if (!cJSON_AddItemToObject(outer.get(), "data", data.get())) return {};
  data.release();
  auto result = print(outer.get());
  return result.size() <= kMaxFileSize ? result : std::string{};
}
inline bool decode(const std::string& bytes, const std::string& device, Settings& output) {
  output.clear();
  if (bytes.empty() || bytes.size() > kMaxFileSize || bytes.find('\0') != std::string::npos) return false;
  auto root = json(cJSON_ParseWithLengthOpts(bytes.c_str(), bytes.size() + 1, nullptr, true));
  if (!root) return false;
  const auto* data = cJSON_GetObjectItemCaseSensitive(root.get(), "data");
  const auto* crc = cJSON_GetObjectItemCaseSensitive(root.get(), "crc32");
  if (!cJSON_IsObject(data) || !cJSON_IsString(crc)) return false;
  const auto payload = print(const_cast<cJSON*>(data));
  if (payload.empty() || checksum(payload) != crc->valuestring) return false;
  const auto* schema = cJSON_GetObjectItemCaseSensitive(data, "schema");
  const auto* id = cJSON_GetObjectItemCaseSensitive(data, "device");
  const auto* values = cJSON_GetObjectItemCaseSensitive(data, "settings");
  if (!cJSON_IsNumber(schema) || schema->valuedouble != 1 || !cJSON_IsString(id) ||
      device != id->valuestring || !cJSON_IsObject(values)) return false;
  Settings parsed;
  for (const cJSON* item = values->child; item; item = item->next) {
    if (!cJSON_IsString(item) || !item->string || !parsed.emplace(item->string, item->valuestring).second) return false;
  }
  if (!can_bind(parsed)) return false;
  output = std::move(parsed);
  return true;
}
}  // namespace printsphere::printer_backup

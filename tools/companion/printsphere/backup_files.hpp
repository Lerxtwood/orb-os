#pragma once
#include "printer_backup.hpp"
#include <cerrno>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace printsphere::printer_backup {
inline bool read_file(const std::string& path, const std::string& device, Settings& settings) {
  FILE* file = std::fopen(path.c_str(), "rb");
  if (!file) return false;
  bool ok = std::fseek(file, 0, SEEK_END) == 0;
  const long size = ok ? std::ftell(file) : -1;
  ok = ok && size > 0 && static_cast<size_t>(size) <= kMaxFileSize && std::fseek(file, 0, SEEK_SET) == 0;
  std::string bytes(ok ? static_cast<size_t>(size) : 0, '\0');
  if (ok) ok = std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size();
  if (std::fclose(file) != 0) ok = false;
  return ok && decode(bytes, device, settings);
}
inline bool load_file(const std::string& path, const std::string& device, Settings& settings) {
  return read_file(path, device, settings) || read_file(path + ".prev", device, settings);
}
inline bool save_file(const std::string& path, const std::string& device, const Settings& settings) {
  Settings previous;
  const bool primary_valid = read_file(path, device, previous);
  if (primary_valid && previous == settings) return true;
  const auto bytes = encode(settings, device);
  if (bytes.empty()) return false;
  const auto temporary = path + ".tmp", retained = path + ".prev";
  FILE* file = std::fopen(temporary.c_str(), "wb");
  if (!file) return false;
  bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size() && std::fflush(file) == 0;
#ifdef _WIN32
  if (ok) ok = _commit(_fileno(file)) == 0;
#else
  if (ok) ok = fsync(fileno(file)) == 0;
#endif
  if (std::fclose(file) != 0) ok = false;
  Settings verified;
  if (!ok || !read_file(temporary, device, verified) || verified != settings) {
    std::remove(temporary.c_str());
    return false;
  }
  // FAT rename cannot replace an existing file. Keep a verified previous copy
  // through promotion, including recovery when power failed between renames.
  if (primary_valid) {
    if (std::remove(retained.c_str()) != 0 && errno != ENOENT) return false;
    if (std::rename(path.c_str(), retained.c_str()) != 0) return false;
  } else if (std::remove(path.c_str()) != 0 && errno != ENOENT) return false;
  return std::rename(temporary.c_str(), path.c_str()) == 0;
}
}  // namespace printsphere::printer_backup

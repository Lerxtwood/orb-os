#include "printsphere/sd_backup.hpp"
#include "printsphere/backup_files.hpp"

#include <atomic>
#include <cstring>
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "sdmmc_cmd.h"

namespace printsphere::sd_backup {
namespace {
constexpr char kTag[] = "printsphere.backup";
constexpr char kPath[] = "/sdcard/printsphere/printer-settings.json";
constexpr char kPartition[] = "ps_nvs";
constexpr char kNamespace[] = "printsphere";
std::recursive_mutex config_mutex;
std::atomic<uint32_t> revision{1};
std::atomic<uint32_t> last_change_ms{0};
uint32_t saved_revision = 0;
int64_t next_check = 0;
bool enabled = false;
bool restore_pending = false;
std::string device;
sdmmc_card_t* mounted_card = nullptr;
std::atomic<const char*> backup_status{"not_started"};
int64_t connected_since = 0;

bool mount_card() {
  // Orb's SD socket is SPI: MOSI=1, CLK=2, MISO=3, CS=41.
  // The BSP's SDMMC helper does not match this wiring. LCD owns SPI2.
  spi_bus_config_t bus = {};
  bus.mosi_io_num = 1;
  bus.miso_io_num = 3;
  bus.sclk_io_num = 2;
  bus.quadwp_io_num = bus.quadhd_io_num = -1;
  bus.data4_io_num = bus.data5_io_num = bus.data6_io_num = bus.data7_io_num = -1;
  bus.max_transfer_sz = 4096;
  if (spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return false;
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = SPI3_HOST;
  sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot.host_id = SPI3_HOST;
  slot.gpio_cs = GPIO_NUM_41;
  esp_vfs_fat_mount_config_t config = {};
  config.format_if_mount_failed = false;
  config.max_files = 1; // Backup operations never keep two files open together.
  config.allocation_unit_size = 16 * 1024;
  for (int khz : {20000, 10000, 4000}) {
    host.max_freq_khz = khz;
    if (esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot, &config, &mounted_card) == ESP_OK) return true;
  }
  spi_bus_free(SPI3_HOST);
  return false;
}

struct MountedCard {
  bool ready = mount_card();
  ~MountedCard() {
    if (!ready) return;
    if (esp_vfs_fat_sdcard_unmount("/sdcard", mounted_card) == ESP_OK) {
      mounted_card = nullptr;
      spi_bus_free(SPI3_HOST);
    } else {
      enabled = false;
      backup_status = "unmount_failed";
      ESP_LOGW(kTag, "Could not release SD resources; further backup attempts disabled");
    }
  }
};

bool read_settings(printer_backup::Settings& settings, std::string* pending = nullptr) {
  nvs_handle_t handle = 0;
  const esp_err_t opened = nvs_open_from_partition(kPartition, kNamespace, NVS_READONLY, &handle);
  if (opened != ESP_OK && opened != ESP_ERR_NVS_NOT_FOUND) return false;
  auto read = [&](const char* key, std::string& value) {
    value.clear();
    if (opened != ESP_OK) return true;
    size_t size = 0;
    const esp_err_t err = nvs_get_str(handle, key, nullptr, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) return true;
    if (err != ESP_OK || !size || size > 8193) return false;
    std::vector<char> buffer(size);
    if (nvs_get_str(handle, key, buffer.data(), &size) != ESP_OK) return false;
    value.assign(buffer.data());
    return true;
  };
  settings.clear();
  bool ok = true;
  printer_backup::each_key([&](const char* key) {
    if (!ok) return;
    std::string value;
    if (!read(key, value)) { ok = false; return; }
    if (!value.empty()) settings.emplace(key, std::move(value));
  });
  if (ok && pending) ok = read("sd_restore", *pending);
  if (opened == ESP_OK) nvs_close(handle);
  return ok;
}

bool restore(const printer_backup::Settings& settings) {
  nvs_handle_t handle = 0;
  if (nvs_open_from_partition(kPartition, kNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
  esp_err_t err = nvs_set_str(handle, "sd_restore", "1");
  if (err == ESP_OK) err = nvs_commit(handle);
  // A durable marker lets the next boot retry if power is lost mid-restore.
  printer_backup::each_key([&](const char* key) {
    if (err == ESP_OK) err = nvs_set_str(handle, key, printer_backup::get(settings, key).c_str());
  });
  if (err == ESP_OK) err = nvs_commit(handle);
  if (err == ESP_OK) err = nvs_set_str(handle, "sd_restore", "");
  if (err == ESP_OK) err = nvs_commit(handle);
  nvs_close(handle);
  return err == ESP_OK;
}
}  // namespace

std::recursive_mutex& configuration_mutex() { return config_mutex; }
const char* status() { return backup_status.load(); }

void changed(const char* key) {
  if (std::strncmp(key, "cloud_", 6) != 0 && std::strncmp(key, "prn_", 4) != 0 &&
      std::strcmp(key, "source_mode") != 0 && std::strcmp(key, "state_source") != 0) return;
  if (printer_backup::allowed_key(key)) {
    last_change_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    ++revision;
  }
}

void initialize_and_restore() {
  uint8_t mac[6] = {};
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) { backup_status = "device_id_failed"; return; }
  char id[13];
  std::snprintf(id, sizeof(id), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  device = id;
  std::lock_guard<std::recursive_mutex> lock(config_mutex);
  printer_backup::Settings current, backup;
  std::string pending;
  if (!read_settings(current, &pending)) { backup_status = "settings_read_failed"; return; }
  enabled = true;
  restore_pending = pending == "1";
  if (!restore_pending && printer_backup::has_setup(current)) {
    // Do not allocate SD/DMA resources before MQTT starts on a configured device.
    backup_status = "waiting_for_printer";
    ESP_LOGI(kTag, "Existing printer settings retained; SD backup waits for live printer connection");
    return;
  }
  MountedCard card;
  if (!card.ready) {
    backup_status = "sd_unavailable";
    ESP_LOGW(kTag, "SD unavailable; printer settings remain in NVS (card was not formatted)");
    return;
  }
  if (!printer_backup::load_file(kPath, device, backup)) {
    backup_status = "no_valid_backup";
    ESP_LOGI(kTag, "No valid SD printer backup for this device");
    return;
  }
  if (!restore(backup)) {
    restore_pending = true;
    backup_status = "restore_failed";
    ESP_LOGW(kTag, "SD printer restore incomplete; backup retained for next boot");
    return;
  }
  restore_pending = false;
  backup_status = "restored";
  ESP_LOGI(kTag, "Restored printer profiles and cloud settings from SD");
}

void poll(bool printer_connected) {
  const int64_t now = esp_timer_get_time();
  if (!printer_connected) { connected_since = 0; return; }
  if (!connected_since) connected_since = now;
  if (!enabled || restore_pending || now < next_check || now - connected_since < 5000000) return;
  next_check = now + 1000000;
  const auto observed = revision.load();
  if (observed == saved_revision || static_cast<uint32_t>(now / 1000) - last_change_ms.load() < 2000) return;
  // Sparse backups and one open file no longer need a 16 KiB MQTT-sized
  // reserve. Keep 8 KiB free and a 4 KiB contiguous block for SPI/DMA setup.
  if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < 8192 ||
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) < 4096) {
    backup_status = "waiting_for_memory";
    next_check = now + 30000000;
    return;
  }
  printer_backup::Settings settings;
  {
    std::lock_guard<std::recursive_mutex> lock(config_mutex);
    if (!read_settings(settings) || !printer_backup::can_bind(settings)) return;
    if (observed != revision.load()) return;
  }
  MountedCard card;
  if (!card.ready) { backup_status = "sd_unavailable"; next_check = now + 30000000; return; }
  if (mkdir("/sdcard/printsphere", 0700) != 0 && errno != EEXIST) {
    backup_status = "directory_failed";
    next_check = now + 30000000;
    return;
  }
  if (printer_backup::save_file(kPath, device, settings)) {
    saved_revision = observed;
    backup_status = "saved";
    ESP_LOGI(kTag, "Printer profiles and cloud settings backed up to SD");
  } else {
    backup_status = "write_failed";
    next_check = now + 30000000;
    ESP_LOGW(kTag, "SD backup write failed; previous backup retained");
  }
}
}  // namespace printsphere::sd_backup

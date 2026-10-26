"""Add printer-binding SD backup only to the isolated PrintSphere companion."""
import shutil
from pathlib import Path

ASSETS = Path(__file__).resolve().parent / 'printsphere'


def adapt(destination, replace):
    main = destination / 'main'
    # The 16 KiB MQTT input buffer otherwise lands in scarce internal RAM at
    # upstream's 16 KiB cutoff. Prefer PSRAM for larger ordinary allocations;
    # explicit DMA and RTOS stack allocations retain their required capabilities.
    old_limit = 'CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384'
    new_limit = 'CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=1024'
    replace(destination / 'sdkconfig.defaults', old_limit, new_limit)
    cached_config = destination / 'sdkconfig'
    if cached_config.exists() and new_limit not in cached_config.read_text(encoding='utf-8'):
        replace(cached_config, old_limit, new_limit)
    for name in ('printer_backup.hpp', 'backup_files.hpp', 'sd_backup.hpp'):
        shutil.copy2(ASSETS / name, main / 'include/printsphere' / name)
    shutil.copy2(ASSETS / 'sd_backup.cpp', main / 'src/sd_backup.cpp')
    tests = destination / 'tests'
    tests.mkdir(exist_ok=True)
    shutil.copy2(ASSETS.parent / 'test_printer_backup.cpp', tests / 'test_printer_backup.cpp')
    config = main / 'src/config_store.cpp'
    replace(config, '#include "printsphere/config_store.hpp"',
            '#include "printsphere/config_store.hpp"\n#include "printsphere/sd_backup.hpp"')
    # Snapshot reads and complete binding updates share a lock, so an HTTP save
    # cannot put half of a cloud account or printer profile into the SD backup.
    for signature in (
        'esp_err_t ConfigStore::save_string(const char* key, const std::string& value) const',
        'esp_err_t ConfigStore::save_cloud_credentials(const BambuCloudCredentials& credentials) const',
        'esp_err_t ConfigStore::save_printer_profile(const PrinterProfile& profile) const',
        'esp_err_t ConfigStore::delete_printer_profile(uint8_t index) const',
        'esp_err_t ConfigStore::save_source_mode(SourceMode mode) const',
    ):
        replace(config, signature + ' {', signature + ' {\n  std::lock_guard<std::recursive_mutex> backup_lock(sd_backup::configuration_mutex());')
    replace(config, '''  const esp_err_t commit_err = nvs_commit(handle);
  nvs_close(handle);
  return commit_err;''', '''  const esp_err_t commit_err = nvs_commit(handle);
  nvs_close(handle);
  if (commit_err == ESP_OK) sd_backup::changed(key);
  return commit_err;''')
    app = main / 'src/application.cpp'
    replace(app, '#include "printsphere/application.hpp"',
            '#include "printsphere/application.hpp"\n#include "printsphere/sd_backup.hpp"')
    replace(app, '  ESP_ERROR_CHECK(pmu_manager_.initialize());',
            '  ESP_ERROR_CHECK(pmu_manager_.initialize());\n  sd_backup::initialize_and_restore();')
    replace(app, '    BambuCloudSnapshot cloud_snapshot = cloud_client_.snapshot();',
            '''    BambuCloudSnapshot cloud_snapshot = cloud_client_.snapshot();
    sd_backup::poll(wifi_connected && (local_snapshot.local_connected || cloud_snapshot.connected));''')
    portal = main / 'src/setup_portal.cpp'
    replace(portal, '#include "printsphere/setup_portal.hpp"',
            '#include "printsphere/setup_portal.hpp"\n#include "printsphere/sd_backup.hpp"\n#include "esp_heap_caps.h"')
    replace(portal, '''    append_mqtt_telemetry_fields(&body, portal->printer_client_.mqtt_telemetry(),
                                 portal->cloud_client_.mqtt_telemetry());''', '''    append_mqtt_telemetry_fields(&body, portal->printer_client_.mqtt_telemetry(),
                                 portal->cloud_client_.mqtt_telemetry());
    body += ",\\"sd_backup_status\\":\\"";
    body += sd_backup::status();
    body += "\\",\\"heap_internal_free\\":" + std::to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    body += ",\\"heap_internal_largest\\":" + std::to_string(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));''')
    cmake = main / 'CMakeLists.txt'
    replace(cmake, '    esp_driver_gpio\n', '    esp_driver_gpio\n    esp_driver_sdspi\n    fatfs\n    json\n')
    replace(cmake, '        "src/config_store.cpp"', '        "src/config_store.cpp"\n        "src/sd_backup.cpp"')

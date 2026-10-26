#pragma once
#include <mutex>
namespace printsphere::sd_backup {
std::recursive_mutex& configuration_mutex();
void changed(const char* key);
void initialize_and_restore();
void poll(bool printer_connected);
const char* status();
}

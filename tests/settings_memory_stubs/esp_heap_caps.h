#pragma once
#include <cstdlib>
#include <cstddef>
constexpr unsigned MALLOC_CAP_INTERNAL = 1, MALLOC_CAP_SPIRAM = 2, MALLOC_CAP_8BIT = 4;
inline size_t test_free_internal = 128 * 1024;
inline unsigned test_last_caps = 0;
inline bool test_psram_available = true;
inline size_t heap_caps_get_free_size(unsigned) { return test_free_internal; }
inline void *heap_caps_malloc(size_t size, unsigned caps) {
    test_last_caps = caps;
    return (caps & MALLOC_CAP_SPIRAM) && !test_psram_available ? nullptr : std::malloc(size);
}
inline void *heap_caps_realloc(void *p, size_t size, unsigned caps) {
    test_last_caps = caps;
    return (caps & MALLOC_CAP_SPIRAM) && !test_psram_available ? nullptr : std::realloc(p, size);
}
inline void heap_caps_free(void *p) { std::free(p); }

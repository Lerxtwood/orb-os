#ifdef ARDUINO
#include "tls_memory.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <mbedtls/platform.h>

namespace {
void *tls_calloc(size_t count, size_t size) {
    // Match calloc's overflow/zeroing contract. Never let a failed large TLS
    // allocation consume the internal RAM needed by WiFi, DMA and task stacks.
    return heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
}

bool tls_memory_begin() {
    if (!psramFound()) {
        Serial.println("[tls] PSRAM unavailable; retaining the default allocator");
        return false;
    }
    // The stock Arduino/IDF libraries expose this hook. Changing sdkconfig macros
    // in the application alone would NOT change their compiled-in allocator.
    // Install once before networking, never swap callbacks around live sessions.
    const int result = mbedtls_platform_set_calloc_free(tls_calloc, heap_caps_free);
    Serial.printf("[tls] PSRAM allocator %s; free=%u largest=%u internal_largest=%u\n",
                  result == 0 ? "enabled" : "failed",
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    return result == 0;
}
#endif

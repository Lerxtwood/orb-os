#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace flight_icon {

// Compare transmitted callsigns with registry data without spaces/hyphens/case.
// Invalid or truncated identifiers must not accidentally match.
inline bool normalize(const char *input, char (&out)[16]) {
    size_t n = 0;
    for (const char *p = input; p && *p; ++p) {
        char c = *p;
        if (c == ' ' || c == '-') continue;
        if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) || n + 1 >= sizeof(out))
            return false;
        out[n++] = c;
    }
    out[n] = 0;
    return n != 0;
}

// US callsigns can still be recognized when the feed omits registry data.
// FAA format: N + 1..5 digits, optionally ending in at most two letters (no I/O).
// https://www.faa.gov/licenses_certificates/aircraft_certification/aircraft_registry/forming_nnumber
inline bool nNumber(const char *call) {
    const size_t n = strlen(call);
    if (n < 2 || n > 6 || call[0] != 'N' || call[1] < '1' || call[1] > '9') return false;
    size_t i = 1;
    while (call[i] >= '0' && call[i] <= '9') ++i;
    if (n - i > 2) return false;
    for (; i < n; ++i)
        if (call[i] < 'A' || call[i] > 'Z' || call[i] == 'I' || call[i] == 'O') return false;
    return true;
}

// Registration-style callsigns are a presentation heuristic for GA, not a
// definitive operation type. Unknown and military contacts retain full size.
// LVGL uses 256 for 100%, 154 is the nearest zoom to 60% in both dimensions.
inline uint16_t imageZoom(const char *callsign, const char *registration, bool military) {
    char call[16], reg[16];
    if (military || !normalize(callsign, call)) return 256;
    const bool matchesRegistration = normalize(registration, reg) && strcmp(call, reg) == 0;
    return matchesRegistration || nNumber(call) ? 154 : 256;
}

} // namespace flight_icon

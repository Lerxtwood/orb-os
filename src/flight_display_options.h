#pragma once
#include "flight_close_pass.h"
#include <cstdint>

namespace flight_display {
constexpr uint32_t DefaultSeconds = 5;
constexpr uint32_t MinSeconds = 1;
constexpr uint32_t MaxSeconds = 3600;

struct Options {
    uint32_t seconds = DefaultSeconds;
    bool stayVisible = false;
    bool arrivals = false;
    bool closePasses = false;
    double miles = flight_close_pass::DefaultMiles;
    bool smallGa = true;
};

inline bool validSeconds(uint32_t seconds) { return seconds >= MinSeconds && seconds <= MaxSeconds; }
inline bool parseSeconds(const char *text, uint32_t &seconds) {
    if (!text || !*text) return false;
    uint32_t value = 0;
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        value = value * 10 + uint32_t(*p - '0');
        if (value > MaxSeconds) return false;
    }
    if (!validSeconds(value)) return false;
    seconds = value;
    return true;
}
inline bool valid(const Options &o) { return validSeconds(o.seconds) && flight_close_pass::validMiles(o.miles); }

// One NVS value saves the entire form atomically. Keep automatic preferences even
// while stayVisible overrides them, so switching back restores the user's choices.
inline uint32_t pack(const Options &o) {
    return o.seconds | (o.stayVisible ? 1u << 12 : 0) |
           (o.arrivals ? 1u << 13 : 0) | (o.closePasses ? 1u << 14 : 0) |
           (uint32_t(std::lround(o.miles * 10)) << 15) |
           (!o.smallGa ? 1u << 25 : 0); // absent in older saves: smaller GA is on
}
inline Options unpack(uint32_t packed) {
    Options o;
    o.seconds = packed & 0xfffu;
    o.stayVisible = (packed & (1u << 12)) != 0;
    o.arrivals = (packed & (1u << 13)) != 0;
    o.closePasses = (packed & (1u << 14)) != 0;
    o.miles = ((packed >> 15) & 0x3ffu) / 10.0;
    o.smallGa = (packed & (1u << 25)) == 0;
    if (packed & 0xfc000000u) return Options{}; // reserved bits / corrupt value
    return valid(o) ? o : Options{};
}
} // namespace flight_display

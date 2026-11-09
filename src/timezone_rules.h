#pragma once
#include "timezone_rules_data.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace timezone_rules {
constexpr long UnknownOffset = 0x7fffffffL;

inline const char *lookup(const char *name) {
    if (!name || !*name) return nullptr;
    size_t lo = 0, hi = sizeof(Entries) / sizeof(Entries[0]);
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        const int order = std::strcmp(name, Entries[mid].name);
        if (!order) return Entries[mid].rule;
        if (order < 0) hi = mid;
        else lo = mid + 1;
    }
    return nullptr;
}

inline bool validOffset(long seconds) {
    return seconds >= -50400 && seconds <= 50400 && seconds % 60 == 0;
}

inline bool fixedOffset(long seconds, char *out, size_t size) {
    if (!out || !size || !validOffset(seconds)) return false;
    const long minutes = (seconds < 0 ? -seconds : seconds) / 60;
    const long hours = minutes / 60, remainder = minutes % 60;
    const char east = seconds < 0 ? '-' : '+';
    const char *west = seconds > 0 ? "-" : ""; // POSIX signs are west of UTC
    int length;
    if (remainder)
        length = std::snprintf(out, size, "<%c%02ld%02ld>%s%ld:%02ld", east, hours, remainder, west, hours, remainder);
    else
        length = std::snprintf(out, size, "<%c%02ld>%s%ld", east, hours, west, hours);
    return length >= 0 && size_t(length) < size;
}

inline bool resolve(const char *name, long currentOffset, char *out, size_t size) {
    if (!out || !size) return false;
    const char *rule = lookup(name);
    // The recurring rules handle future seasonal changes offline. Some regions
    // have irregular, announced transitions that cannot fit a POSIX rule; keep
    // their observed offset current with the existing location-weather refresh.
    if ((!rule || !std::strchr(rule, ',')) && validOffset(currentOffset))
        return fixedOffset(currentOffset, out, size);
    if (!rule) return false;
    const int length = std::snprintf(out, size, "%s", rule);
    return length >= 0 && size_t(length) < size;
}

inline bool legacyAutomatic(const char *tz, bool stored, bool explicitPreset) {
    if (!stored) return true;
    return !explicitPreset && tz && tz[0] == '<' && (tz[1] == '+' || tz[1] == '-');
}

inline bool sameLocation(double lat, double lon, double otherLat, double otherLon) {
    return std::isfinite(lat) && std::isfinite(lon) &&
           std::isfinite(otherLat) && std::isfinite(otherLon) &&
           std::abs(lat - otherLat) < 0.000001 && std::abs(lon - otherLon) < 0.000001;
}
} // namespace timezone_rules

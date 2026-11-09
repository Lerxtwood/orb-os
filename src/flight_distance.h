#pragma once
#include <string>

namespace flight_distance {

inline float value(float km, bool imperial) { return imperial ? km * 0.621371f : km; }
inline const char *unit(bool imperial) { return imperial ? "mi" : "km"; }

// Existing SD themes embed the unit after {dist}. Keep those themes working
// while allowing new themes to use an explicit {distUnit} token.
inline std::string format(const char *fmt) {
    std::string result = fmt;
    size_t pos = 0;
    while ((pos = result.find("{dist}", pos)) != std::string::npos) {
        pos += 6;
        size_t suffix = pos;
        while (suffix < result.size() && (result[suffix] == ' ' || result[suffix] == '\t')) ++suffix;
        if (result.compare(suffix, 2, "km") != 0 && result.compare(suffix, 2, "KM") != 0) continue;
        const size_t end = suffix + 2;
        if (end < result.size() && ((result[end] >= 'a' && result[end] <= 'z') ||
                                   (result[end] >= 'A' && result[end] <= 'Z'))) continue;
        result.replace(suffix, 2, "{distUnit}");
    }
    return result;
}

} // namespace flight_distance

#pragma once
#include <cmath>
#include <cstdint>

namespace flight_close_pass {
constexpr double KmPerMile = 1.609344;
constexpr double DefaultMiles = 5.0;
constexpr double MinMiles = 0.1;
constexpr double MaxMiles = 100.0;

inline bool validMiles(double miles) {
    return std::isfinite(miles) && miles >= MinMiles && miles <= MaxMiles &&
           std::abs(miles * 10 - std::round(miles * 10)) < 0.000001;
}
// Distance in tenths of a mile and the enable flag share one atomic NVS write.
inline uint32_t packOptions(double miles, bool enabled) {
    return uint32_t(std::lround(miles * 10)) | (enabled ? 0x10000u : 0);
}
inline double unpackMiles(uint32_t options) {
    const double miles = (options & 0xffffu) / 10.0;
    return validMiles(miles) ? miles : DefaultMiles;
}
inline bool unpackEnabled(uint32_t options) { return (options & 0x10000u) != 0; }

// Trigger on actual reported distance, without forecasting a future pass.
inline bool withinDistance(double distanceKm, double miles) {
    return std::isfinite(distanceKm) && distanceKm >= 0 && validMiles(miles) &&
           distanceKm <= miles * KmPerMile;
}
} // namespace flight_close_pass

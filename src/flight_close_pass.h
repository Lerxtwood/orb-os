#pragma once
#include "geo.h"
#include <cmath>
#include <cstdint>

namespace flight_close_pass {
constexpr double KmPerMile = 1.609344;
constexpr double DefaultMiles = 5.0;
constexpr double MinMiles = 0.1;
constexpr double MaxMiles = 100.0;
constexpr double LookaheadSeconds = 15 * 60;

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

struct Prediction {
    double distanceKm = NAN;
    double seconds = NAN;
    bool within(double miles) const {
        return std::isfinite(miles) && miles >= MinMiles && miles <= MaxMiles &&
               seconds > 0 && seconds <= LookaheadSeconds && distanceKm <= miles * KmPerMile;
    }
};

// Closest point along the aircraft's current great-circle ground track. Positive
// along-track distance means the aircraft is approaching that point; flights
// already travelling away, stationary contacts and unknown motion do not qualify.
inline Prediction predict(double distanceKm, double bearingToHomeDeg,
                          double trackDeg, double groundSpeedKt) {
    if (!std::isfinite(distanceKm) || distanceKm < 0 ||
        !std::isfinite(bearingToHomeDeg) || !std::isfinite(trackDeg) ||
        !std::isfinite(groundSpeedKt) || trackDeg < 0 || trackDeg > 360 || groundSpeedKt <= 1)
        return {};
    constexpr double EarthKm = 6371.0088;
    const double delta = distanceKm / EarthKm;
    const double angle = geo::deg2rad(trackDeg - bearingToHomeDeg);
    const double along = std::atan2(std::sin(delta) * std::cos(angle), std::cos(delta));
    if (along <= 0) return {};
    Prediction result;
    result.distanceKm = std::abs(std::asin(std::sin(delta) * std::sin(angle))) * EarthKm;
    result.seconds = along * EarthKm / (groundSpeedKt * 1.852) * 3600;
    return result;
}
} // namespace flight_close_pass

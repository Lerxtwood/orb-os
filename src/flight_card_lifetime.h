#pragma once
#include <cstdint>
#include "flight_display_options.h"

// The countdown begins after route completion (including a completed miss).
// Cached routes and flights without a callsign begin their countdown immediately.
class FlightCardLifetime {
public:
    void configure(uint32_t seconds, bool stayVisible, uint32_t now) {
        if (!flight_display::validSeconds(seconds)) return;
        durationSeconds = seconds;
        pinned = stayVisible;
        resume(now);
    }
    uint32_t seconds() const { return durationSeconds; }
    bool stayVisible() const { return pinned; }
    bool automaticDisplaysAllowed() const { return !pinned; }
    void begin(uint32_t now, bool needsRoute) {
        completedAt = now;
        complete = !needsRoute;
    }
    void routeFinished(uint32_t now) {
        if (complete) return;
        complete = true;
        completedAt = now;
    }
    void routeStarted() { complete = false; }
    void resume(uint32_t now) { if (complete) completedAt = now; }
    bool expired(uint32_t now) const {
        return !pinned && complete && uint32_t(now - completedAt) >= durationSeconds * 1000u;
    }
private:
    uint32_t completedAt = 0;
    uint32_t durationSeconds = flight_display::DefaultSeconds;
    bool pinned = false;
    bool complete = true;
};

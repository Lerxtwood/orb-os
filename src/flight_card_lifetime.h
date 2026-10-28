#pragma once
#include <cstdint>

// The card must satisfy both its minimum reading time and the reading time
// after route completion. Cached routes and completed misses count as finished.
class FlightCardLifetime {
public:
    static constexpr uint32_t MinimumMs = 10000;
    static constexpr uint32_t RouteReadingMs = 3000;
    void begin(uint32_t now, bool needsRoute) {
        visibleSince = completedAt = now;
        complete = !needsRoute;
    }
    void routeFinished(uint32_t now) {
        if (complete) return;
        complete = true;
        completedAt = now;
    }
    void routeStarted() { complete = false; }
    void resume(uint32_t now) { visibleSince = now; }
    bool expired(uint32_t now) const {
        return complete && uint32_t(now - visibleSince) >= MinimumMs &&
               uint32_t(now - completedAt) >= RouteReadingMs;
    }
private:
    uint32_t visibleSince = 0, completedAt = 0;
    bool complete = true;
};

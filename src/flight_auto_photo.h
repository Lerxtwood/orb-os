#pragma once
#include <algorithm>
#include <cstdint>
#include <deque>
#include <set>
#include <string>
#include <vector>

// Transition detection and preview queue for drawable flights. Ordinary arrivals
// establish a silent initial baseline; predicted close passes opt into initial
// qualification so an already-visible inbound flight can trigger a preview.
class FlightAutoPhoto {
public:
    static constexpr uint32_t HighlightMs = 1000;
    static constexpr uint32_t DisplayMs = 3000;
    static constexpr uint32_t LookupTimeoutMs = 30000;
    enum class Phase { Idle, Highlight, Loading, Showing };

    void reset() { cancel(); previous.clear(); primed = false; }
    void cancel() { pending.clear(); hex.clear(); phase = Phase::Idle; }
    std::vector<std::string> observe(const std::vector<std::string>& visible, bool allowArrivals,
                                     bool includeInitial = false) {
        // Motion checks usually move pixels without changing visibility. Avoid
        // rebuilding tree nodes on every glide frame on the constrained device.
        if (primed && visible.size() == previous.size() &&
            std::all_of(visible.begin(), visible.end(),
                        [&](const std::string& id) { return previous.count(id) != 0; })) return {};
        std::vector<std::string> arrivals;
        std::set<std::string> current(visible.begin(), visible.end());
        pending.erase(std::remove_if(pending.begin(), pending.end(),
            [&](const std::string& id) { return !current.count(id); }), pending.end());
        if (primed || includeInitial) {
            for (const auto& id : visible) {
                if (previous.count(id)) continue;
                arrivals.push_back(id);
                if (allowArrivals && id != hex && pending.size() < 32 &&
                    std::find(pending.begin(), pending.end(), id) == pending.end())
                    pending.push_back(id);
            }
        }
        previous.swap(current);
        primed = true;
        return arrivals;
    }
    bool start(uint32_t now) {
        if (phase != Phase::Idle || pending.empty()) return false;
        hex = pending.front(); pending.pop_front();
        phase = Phase::Highlight; since = now;
        return true;
    }
    bool highlightDone(uint32_t now) const {
        return phase == Phase::Highlight && uint32_t(now - since) >= HighlightMs;
    }
    void loading(uint32_t now) { phase = Phase::Loading; since = now; }
    void shown(uint32_t now) { phase = Phase::Showing; since = now; }
    bool expired(uint32_t now) const {
        return (phase == Phase::Showing && uint32_t(now - since) >= DisplayMs) ||
               (phase == Phase::Loading && uint32_t(now - since) >= LookupTimeoutMs);
    }
    void finish() { hex.clear(); phase = Phase::Idle; }
    void discard(const std::string& id) {
        pending.erase(std::remove(pending.begin(), pending.end(), id), pending.end());
    }
    bool active() const { return phase != Phase::Idle; }
    bool stillVisible() const { return previous.count(hex) != 0; }
    const std::string& aircraft() const { return hex; }
    Phase state() const { return phase; }
private:
    bool primed = false;
    uint32_t since = 0;
    Phase phase = Phase::Idle;
    std::string hex;
    std::set<std::string> previous;
    std::deque<std::string> pending;
};

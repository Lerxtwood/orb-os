#pragma once
#include <atomic>
#include <stdint.h>

// Only an SNTP notification proves network synchronization. A plausible time
// seeded from the RTC must never count as a network fix.
class ClockSyncState {
public:
    void notified(uint32_t nowMs) {
        lastMs_.store(nowMs);
        generation_.fetch_add(1);
    }
    uint32_t generation() const { return generation_.load(); }
    uint32_t lastMs() const { return lastMs_.load(); }
    uint32_t savedGeneration() const { return saved_; }
    bool pending(uint32_t generation) const { return generation != saved_; }
    // Called on the main core only, after a successful RTC write. A notification
    // arriving during that write remains pending for the next pass.
    void saved(uint32_t generation) { saved_ = generation; }
private:
    std::atomic<uint32_t> generation_{0}, lastMs_{0};
    uint32_t saved_ = 0;
};

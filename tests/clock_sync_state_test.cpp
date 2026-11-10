#include "clock_sync_state.h"
#include <cassert>
#include <cstdio>

int main() {
    ClockSyncState state;
    // Neither boot nor seeding a plausible RTC time produces an SNTP callback.
    assert(state.generation() == 0 && !state.pending(0));
    state.notified(1500);
    const auto first = state.generation();
    assert(first == 1 && state.pending(first) && state.lastMs() == 1500);
    // A failed RTC write leaves the sync pending, for retry on the main core.
    assert(state.pending(state.generation()));
    state.saved(first);
    assert(!state.pending(state.generation()));
    // A later correction must also reach the RTC.
    state.notified(3000);
    const auto second = state.generation();
    assert(state.pending(second));
    // A callback during the RTC write must not be lost by acknowledging second.
    state.notified(3010);
    state.saved(second);
    assert(state.pending(state.generation()) && state.generation() == 3);
    state.saved(state.generation());
    assert(!state.pending(state.generation()));
    // millis() rollover does not affect synchronization tracking.
    state.notified(0xfffffff0u);
    state.notified(20);
    assert(state.pending(state.generation()) && state.lastMs() == 20);
    std::puts("Clock synchronization state checks passed");
}

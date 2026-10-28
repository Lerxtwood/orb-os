#include "flight_auto_photo.h"
#include <cassert>
#include <cstdint>

int main() {
    // Predicted passes already on screen qualify immediately; ordinary arrivals
    // retain their silent initial baseline. A later course change can qualify a
    // flight that has been visible for several snapshots.
    FlightAutoPhoto closePass;
    closePass.observe({"predicted"}, true, true);
    assert(closePass.start(0));
    assert(closePass.aircraft() == "predicted");
    closePass.finish();
    assert(closePass.observe({"predicted"}, true, true).empty());
    assert(!closePass.start(10));
    closePass.observe({}, true, true); // prediction no longer qualifies
    closePass.observe({"course-changed"}, true, true);
    assert(closePass.start(20));
    assert(closePass.aircraft() == "course-changed");
    closePass.finish();
    closePass.observe({"course-changed", "queued"}, true, true);
    closePass.observe({"course-changed"}, true, true); // queued prediction becomes a miss
    assert(!closePass.start(30));

    // The renderer supplies drawable contacts only: a flight may already be
    // geographically in range while hidden by a keep-out area or display cap.
    // Sounds consume these same arrivals even with automatic photos disabled.
    FlightAutoPhoto visibility;
    assert(visibility.observe({"visible"}, false).empty());
    assert(visibility.observe({"visible"}, false).empty()); // masked aircraft stays silent
    auto arrivals = visibility.observe({"visible", "unmasked"}, false);
    assert(arrivals.size() == 1 && arrivals[0] == "unmasked");
    assert(!visibility.start(0)); // sound detection does not enable photos
    assert(visibility.observe({"visible", "unmasked"}, false).empty());
    visibility.observe({"visible"}, true); // passes behind another keep-out area
    arrivals = visibility.observe({"visible", "unmasked"}, true);
    assert(arrivals.size() == 1 && arrivals[0] == "unmasked");
    assert(visibility.start(100)); // same event drives sound and photo
    assert(visibility.aircraft() == arrivals[0]);
    visibility.observe({"visible"}, true);
    assert(!visibility.stillVisible()); // becoming masked cancels the preview

    FlightAutoPhoto preview;
    preview.observe({"existing"}, true);
    assert(!preview.start(0)); // entering the app establishes the baseline
    preview.observe({"existing", "arrival", "second"}, true);
    assert(preview.start(100));
    assert(preview.aircraft() == "arrival");
    assert(!preview.highlightDone(1099));
    assert(preview.highlightDone(1100));
    preview.loading(1100);
    assert(!preview.expired(31099));
    assert(preview.expired(31100)); // network stalls cannot pin the screen
    preview.shown(20000);
    assert(!preview.expired(24999));
    assert(preview.expired(25000)); // five seconds measured from image arrival
    preview.finish();
    assert(preview.start(25001));
    assert(preview.aircraft() == "second");
    preview.finish();
    preview.observe({"existing", "arrival", "second"}, true);
    assert(!preview.start(26000)); // repeated polls do not replay contacts

    preview.observe({"existing"}, true);
    preview.observe({"existing", "arrival"}, true);
    assert(preview.start(27000)); // leaving and re-entering is a fresh arrival
    preview.observe({"existing"}, true);
    assert(!preview.stillVisible());
    preview.finish();
    preview.observe({"existing", "gone-before-preview"}, true);
    preview.observe({"existing"}, true);
    assert(!preview.start(28000)); // queued aircraft that leave are discarded

    preview.observe({"existing", "manual"}, false);
    preview.observe({"existing", "manual"}, true);
    assert(!preview.start(29000)); // arrivals during manual use are skipped
    preview.observe({"existing", "manual", "queued"}, true);
    preview.cancel();
    assert(!preview.start(30000)); // knob input discards pending previews

    preview.reset();
    preview.observe({"new-range"}, true);
    assert(!preview.start(31000)); // changing range/home or re-enabling re-baselines
    preview.observe({"new-range", "rollover"}, true);
    assert(preview.start(UINT32_MAX - 500));
    assert(!preview.highlightDone(498));
    assert(preview.highlightDone(499));
    preview.loading(UINT32_MAX - 500);
    assert(preview.expired(29499));
    preview.shown(UINT32_MAX - 500);
    assert(!preview.expired(4498));
    assert(preview.expired(4499));
}

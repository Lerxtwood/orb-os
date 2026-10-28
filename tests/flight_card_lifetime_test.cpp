#include "flight_card_lifetime.h"
#include "flight_auto_photo.h"
#include <cassert>
#include <cstdint>

int main() {
    FlightCardLifetime card;
    card.begin(100, true);
    assert(!card.expired(10100));
    assert(!card.expired(60000)); // pending route cannot dismiss at the minimum
    card.routeFinished(60000);
    assert(!card.expired(62999));
    assert(card.expired(63000)); // completed miss and valid route share this window
    card.routeFinished(64000); // repeated render polls must not restart the countdown
    assert(card.expired(64000));

    card.begin(0, true);
    card.routeFinished(1000);
    assert(!card.expired(9999));
    assert(card.expired(10000)); // early route: keep the ten-second minimum
    card.begin(0, true);
    card.routeFinished(9000);
    assert(!card.expired(10000));
    assert(!card.expired(11999));
    assert(card.expired(12000)); // late route: extend past the minimum
    card.begin(0, true);
    card.routeFinished(0);
    assert(!card.expired(9999));
    assert(card.expired(10000)); // cached result
    card.routeStarted(); // cached result expired and a fresh lookup is now pending
    assert(!card.expired(11000));
    card.routeFinished(12000);
    assert(!card.expired(14999));
    assert(card.expired(15000));
    card.begin(100, false);
    assert(!card.expired(10099));
    assert(card.expired(10100)); // no callsign: no lookup to await

    FlightAutoPhoto photo;
    card.begin(0, true);
    photo.observe({"flight"}, true, true);
    assert(photo.start(0));
    photo.loading(1000);
    card.routeFinished(5000); // route completes behind the photo
    photo.shown(15000);
    assert(!photo.expired(17999));
    assert(photo.expired(18000));
    photo.finish();
    card.resume(18000); // photo dismissal returns to the card, not an empty radar
    assert(!card.expired(27999));
    assert(card.expired(28000));

    card.begin(0, true);
    card.resume(20000);
    assert(!card.expired(30000)); // still pending after photo dismissal
    card.routeFinished(35000);
    assert(!card.expired(37999));
    assert(card.expired(38000));
    card.begin(40000, true); // changing aircraft resets completion and minimum
    assert(!card.expired(90000));

    card.begin(UINT32_MAX - 500, true);
    card.routeFinished(999);
    assert(!card.expired(9498));
    assert(card.expired(9499));
    card.begin(UINT32_MAX - 500, true);
    card.routeFinished(9000);
    assert(!card.expired(11999));
    assert(card.expired(12000));
}

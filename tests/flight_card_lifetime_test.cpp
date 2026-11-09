#include "flight_card_lifetime.h"
#include "flight_auto_photo.h"
#include <cassert>
#include <cstdint>

int main() {
    FlightCardLifetime card;
    assert(card.seconds() == 5);
    card.begin(0, true);
    card.routeFinished(1000);
    assert(!card.expired(5999));
    assert(card.expired(6000));
    card.configure(10, false, 0);
    card.begin(100, true);
    assert(!card.expired(10100));
    assert(!card.expired(60000)); // pending route cannot dismiss at the minimum
    card.routeFinished(60000);
    assert(!card.expired(69999));
    assert(card.expired(70000)); // completed miss and valid route share this window
    card.routeFinished(71000); // repeated render polls must not restart the countdown
    assert(card.expired(71000));

    card.begin(0, true);
    card.routeFinished(1000);
    assert(!card.expired(10999));
    assert(card.expired(11000)); // exactly ten seconds after an early route result
    card.begin(0, true);
    card.routeFinished(9000);
    assert(!card.expired(10000));
    assert(!card.expired(18999));
    assert(card.expired(19000)); // late route gets the same full reading window
    card.begin(0, true);
    card.routeFinished(0);
    assert(!card.expired(9999));
    assert(card.expired(10000)); // cached result
    card.routeStarted(); // cached result expired and a fresh lookup is now pending
    assert(!card.expired(11000));
    card.routeFinished(12000);
    assert(!card.expired(21999));
    assert(card.expired(22000));
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
    assert(!card.expired(44999));
    assert(card.expired(45000));
    card.begin(40000, true); // changing aircraft resets completion and minimum
    assert(!card.expired(90000));

    card.begin(UINT32_MAX - 500, true);
    card.routeFinished(999);
    assert(!card.expired(10998));
    assert(card.expired(10999));
    card.begin(UINT32_MAX - 500, true);
    card.routeFinished(9000);
    assert(!card.expired(18999));
    assert(card.expired(19000));

    card.configure(1, false, 0);
    card.begin(UINT32_MAX - 500, false);
    assert(!card.expired(498));
    assert(card.expired(499)); // rollover during the countdown
    card.configure(3600, false, 0);
    card.begin(0, false);
    assert(!card.expired(3599999));
    assert(card.expired(3600000));
    card.configure(0, true, 0); // invalid settings must not change policy
    assert(!card.stayVisible() && card.seconds() == 3600);
    card.configure(3601, true, 0);
    assert(!card.stayVisible());

    card.configure(7, true, 0);
    card.begin(0, true);
    assert(!card.automaticDisplaysAllowed()); // observation and playback use this guard
    card.routeFinished(1000);
    assert(!card.expired(3600000));
    card.resume(4000000);
    assert(!card.expired(UINT32_MAX));
    card.begin(5000000, false); // manually choosing another flight stays pinned
    assert(!card.expired(6000000));
    card.configure(7, false, 6000000); // timed mode gives a fresh reading window
    assert(card.automaticDisplaysAllowed());
    assert(!card.expired(6006999));
    assert(card.expired(6007000));

    uint32_t seconds = 42;
    for (const char *invalid : {"", "0", "3601", "-1", "1.5", "1s", " 3", "999999999999999999999"}) {
        assert(!flight_display::parseSeconds(invalid, seconds));
        assert(seconds == 42);
    }
    assert(flight_display::parseSeconds("1", seconds) && seconds == 1);
    assert(flight_display::parseSeconds("3600", seconds) && seconds == 3600);
    for (uint32_t duration : {1u, 10u, 3600u}) {
        for (double miles : {0.1, 5.0, 100.0}) {
            for (unsigned flags = 0; flags < 8; ++flags) {
                flight_display::Options o{duration, bool(flags & 1), bool(flags & 2), bool(flags & 4), miles};
                const auto restored = flight_display::unpack(flight_display::pack(o));
                assert(restored.seconds == o.seconds && restored.stayVisible == o.stayVisible);
                assert(restored.arrivals == o.arrivals && restored.closePasses == o.closePasses);
                assert(restored.miles == o.miles);
            }
        }
    }
    assert(flight_display::unpack(0).seconds == flight_display::DefaultSeconds);
    assert(!flight_display::unpack(UINT32_MAX).stayVisible);
}

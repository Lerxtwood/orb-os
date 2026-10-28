#include "flight_close_pass.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <initializer_list>

int main() {
    using namespace flight_close_pass;
    const auto inbound = predict(50, 180, 180, 250);
    assert(inbound.within(5));
    assert(std::abs(inbound.distanceKm) < 0.000001);
    assert(std::abs(inbound.seconds - 50 / (250 * 1.852) * 3600) < 0.001);
    assert(!predict(50, 180, 0, 250).within(5)); // same position, flying away
    assert(!predict(50, 180, 90, 250).within(5)); // crossing too far from home
    assert(!predict(200, 180, 180, 250).within(5)); // closest point beyond 15 minutes
    assert(predict(50, 180, 185, 250).within(5)); // small predicted miss still qualifies
    const auto offset = predict(50, 180, 190, 250);
    assert(!offset.within(5)); // about 8.68 km is outside 5 ordinary miles
    assert(offset.within(6)); // changing n changes eligibility
    assert(predict(1, 180, 180, 100).within(1)); // already close and approaching
    assert(!predict(1, 180, 0, 100).within(1)); // already close but departing
    assert(predict(50, 0, 360, 250).within(5)); // north wraparound
    assert(predict(50, 359, 1, 250).within(5));
    assert(!predict(50, 180, NAN, 250).within(5));
    assert(!predict(50, 180, 180, NAN).within(5));
    assert(!predict(50, 180, 180, 0).within(5));
    assert(!predict(NAN, 180, 180, 250).within(5));
    assert(!predict(-1, 180, 180, 250).within(5));
    assert(!predict(50, 180, -1, 250).within(5));
    assert(!inbound.within(NAN));

    assert(validMiles(0.1) && validMiles(5) && validMiles(100));
    assert(!validMiles(0) && !validMiles(100.1) && !validMiles(0.15));
    assert(!validMiles(NAN) && !validMiles(std::numeric_limits<double>::infinity()));
    for (double miles : {0.1, 5.0, 10.5, 100.0}) {
        for (bool enabled : {false, true}) {
            const auto settings = packOptions(miles, enabled);
            assert(std::abs(unpackMiles(settings) - miles) < 0.000001);
            assert(unpackEnabled(settings) == enabled);
        }
    }
    assert(unpackMiles(0) == DefaultMiles); // invalid stored distance falls back safely
}

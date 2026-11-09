#include "flight_close_pass.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <initializer_list>

int main() {
    using namespace flight_close_pass;
    assert(!withinDistance(50, 5)); // an eventual flyover must not trigger early
    assert(!withinDistance(5 * KmPerMile + 0.001, 5));
    assert(withinDistance(5 * KmPerMile, 5)); // entering the radius triggers
    assert(withinDistance(5 * KmPerMile - 0.001, 5));
    assert(withinDistance(1, 1)); // current distance needs no heading or speed
    assert(!withinDistance(8.68, 5));
    assert(withinDistance(8.68, 6)); // changing the radius changes eligibility
    assert(withinDistance(0, 0.1));
    assert(!withinDistance(NAN, 5));
    assert(!withinDistance(-1, 5));
    assert(!withinDistance(std::numeric_limits<double>::infinity(), 5));
    assert(!withinDistance(1, NAN));
    assert(!withinDistance(1, 0));

    // Feed updates: no notification while approaching outside the radius,
    // then qualification at entry, and no qualification again after leaving.
    for (double km : {30.0, 20.0, 10.0, 8.1}) assert(!withinDistance(km, 5));
    for (double km : {8.0, 5.0, 1.0}) assert(withinDistance(km, 5));
    assert(!withinDistance(8.1, 5));

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

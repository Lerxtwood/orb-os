#include "../src/flight_distance.h"
#include "../src/text_tokens.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

static void check(const char *fmt, bool imperial, const char *expected) {
    char distance[16], output[80];
    snprintf(distance, sizeof(distance), "%.1f", (double)flight_distance::value(10, imperial));
    const text_tokens::Tok tokens[] = {
        {"dist", distance}, {"distUnit", flight_distance::unit(imperial)},
    };
    const auto normalized = flight_distance::format(fmt);
    text_tokens::expand(output, sizeof(output), normalized.c_str(), tokens, 2);
    assert(strcmp(output, expected) == 0);
}

int main() {
    assert(std::abs(flight_distance::value(1.609344f, true) - 1.0f) < 0.00001f);
    check("DIST {dist} km", false, "DIST 10.0 km");
    check("DIST {dist} km", true, "DIST 6.2 mi");
    check("{dist} KM / {dist}km", true, "6.2 mi / 6.2mi");
    check("{dist} {distUnit}", false, "10.0 km");
    check("{dist} {distUnit}", true, "6.2 mi");
    check("{dist}", true, "6.2");
    check("{dist} km range: 30 km", true, "6.2 mi range: 30 km");
    assert(flight_distance::format("{dist} kmh") == "{dist} kmh");
}

#include "flight_icon_scale.h"
#include <cassert>
#include <cstdio>

int main() {
    using flight_icon::imageZoom;
    // Airline and charter/operator callsigns stay full size, regardless of tail.
    assert(imageZoom("UAL123", "N123UA", false) == 256);
    assert(imageZoom("DAL456", "N456DL", false) == 256);
    assert(imageZoom("NJE123", "CS-DLA", false) == 256);
    assert(imageZoom("N123ABC", "", false) == 256); // not an N-number
    assert(imageZoom("N0123", "", false) == 256);
    assert(imageZoom("N123IO", "", false) == 256);
    assert(imageZoom("N12A3", "", false) == 256);

    // Registration callsigns use the same icon at 60% in each dimension.
    assert(imageZoom("N12345", "N12345", false) == 154);
    assert(imageZoom("n123ab  ", "", false) == 154);
    assert(imageZoom("N1234A", nullptr, false) == 154);
    assert(imageZoom("CFABC", "C-FABC", false) == 154);
    assert(imageZoom("g-abcd ", "GABCD", false) == 154);
    assert(imageZoom("VHABC", "VH-ABC", false) == 154);

    // Missing/invalid identifiers and military contacts must not shrink.
    assert(imageZoom("", "N12345", false) == 256);
    assert(imageZoom(nullptr, nullptr, false) == 256);
    assert(imageZoom("???", "???", false) == 256);
    assert(imageZoom("ABCDEFGHIJKLMNOP", "ABCDEFGHIJKLMNOP", false) == 256);
    assert(imageZoom("N12345", "N12345", true) == 256);
    assert(imageZoom("RCH123", "", true) == 256);
    std::puts("Flight icon scale checks passed");
}

#include "timezone_rules.h"
#include <cassert>
#include <cstdio>
#include <cstring>

int main(int argc, char **argv) {
    using namespace timezone_rules;
    if (argc == 2) {
        const char *rule = lookup(argv[1]);
        if (!rule) return 1;
        puts(rule);
        return 0;
    }
    for (size_t i = 0; i < sizeof(Entries) / sizeof(Entries[0]); ++i) {
        assert(lookup(Entries[i].name) == Entries[i].rule);
        assert(strlen(Entries[i].rule) < 128);
        if (i) assert(strcmp(Entries[i - 1].name, Entries[i].name) < 0);
    }
    assert(!lookup(nullptr) && !lookup("") && !lookup("Not/A_Zone"));
    assert(strcmp(lookup("America/Denver"), "MST7MDT,M3.2.0,M11.1.0") == 0);
    assert(strcmp(lookup("America/Phoenix"), "MST7") == 0);
    assert(strcmp(lookup("US/Mountain"), lookup("America/Denver")) == 0);
    char out[128];
    assert(resolve("America/Denver", -6 * 3600, out, sizeof(out)));
    assert(strcmp(out, "MST7MDT,M3.2.0,M11.1.0") == 0); // summer offset must not become permanent
    assert(resolve("America/Denver", UnknownOffset, out, sizeof(out)));
    assert(strchr(out, ',')); // named rules do not depend on a current offset
    assert(resolve("America/Phoenix", -7 * 3600, out, sizeof(out)));
    assert(strcmp(out, "<-07>7") == 0 && !strchr(out, ','));
    assert(resolve("Africa/Casablanca", 3600, out, sizeof(out)));
    assert(strcmp(out, "<+01>-1") == 0); // irregular regional changes use the observed offset
    assert(resolve("Future/Unknown", 20700, out, sizeof(out)));
    assert(strcmp(out, "<+0545>-5:45") == 0);
    assert(!resolve("Future/Unknown", UnknownOffset, out, sizeof(out)));
    assert(!fixedOffset(50460, out, sizeof(out)) && !fixedOffset(-50460, out, sizeof(out)));
    assert(!fixedOffset(1, out, sizeof(out)));
    assert(fixedOffset(1800, out, sizeof(out)) && strcmp(out, "<+0030>-0:30") == 0);
    assert(fixedOffset(-1800, out, sizeof(out)) && strcmp(out, "<-0030>0:30") == 0);
    assert(fixedOffset(0, out, sizeof(out)) && strcmp(out, "<+00>0") == 0);
    assert(!resolve("America/Denver", 0, out, 4)); // never apply a truncated rule
    assert(legacyAutomatic("MST7", false, true)); // no saved TZ: resolve location
    assert(legacyAutomatic("<-06>6", true, false)); // migrate a saved summer offset
    assert(legacyAutomatic("<-07>7", true, false)); // migrate a saved winter offset too
    assert(!legacyAutomatic("MST7", true, true)); // explicitly selected Arizona
    assert(!legacyAutomatic("<+0530>-5:30", true, true)); // explicit fixed-offset preset
    assert(!legacyAutomatic("MST7MDT,M3.2.0,M11.1.0", true, true));
    assert(sameLocation(40.0, -105.0, 40.0, -105.0));
    assert(!sameLocation(40.0, -105.0, 33.4, -112.0));
    assert(!sameLocation(NAN, -105.0, 40.0, -105.0));
}

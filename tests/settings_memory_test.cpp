#include "lv_psram_alloc.h"
#include <cassert>
#include <cstdio>

unsigned orb_lv_external_scope_depth = 0;

int main() {
    void *p = orb_lv_malloc(32);
    assert(p && (test_last_caps & MALLOC_CAP_INTERNAL));
    orb_lv_free(p);
    {
        OrbLvExternalScope settings;
        assert(orb_lv_external_scope_depth == 1);
        p = orb_lv_malloc(32);
        assert(p && (test_last_caps & MALLOC_CAP_SPIRAM));
        {
            OrbLvExternalScope nested;
            assert(orb_lv_external_scope_depth == 2);
            p = orb_lv_realloc(p, 64);
            assert(p && (test_last_caps & MALLOC_CAP_SPIRAM));
        }
        assert(orb_lv_external_scope_depth == 1);
        orb_lv_free(p);
        test_psram_available = false;
        p = orb_lv_malloc(32);
        assert(p && (test_last_caps & MALLOC_CAP_INTERNAL));
        orb_lv_free(p);
        test_psram_available = true;
    }
    assert(orb_lv_external_scope_depth == 0);
    p = orb_lv_malloc(32);
    assert(p && (test_last_caps & MALLOC_CAP_INTERNAL));
    orb_lv_free(p);
    test_free_internal = 8000;
    p = orb_lv_malloc(32);
    assert(p && (test_last_caps & MALLOC_CAP_SPIRAM));
    orb_lv_free(p);
    std::puts("Settings widget allocation checks passed");
}

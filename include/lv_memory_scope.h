#pragma once

#ifdef ESP_PLATFORM
#ifdef __cplusplus
extern "C" {
#endif
// LVGL and Settings run on the main core. Shared with LVGL's C allocator.
extern unsigned orb_lv_external_scope_depth;
#ifdef __cplusplus
}
#endif
#endif

#ifdef __cplusplus
// Long-lived Settings widgets need not occupy the RAM required by Wi-Fi.
// Drawing after construction keeps the usual allocation policy.
class OrbLvExternalScope {
public:
    OrbLvExternalScope() {
#ifdef ESP_PLATFORM
        ++orb_lv_external_scope_depth;
#endif
    }
    ~OrbLvExternalScope() {
#ifdef ESP_PLATFORM
        --orb_lv_external_scope_depth;
#endif
    }
    OrbLvExternalScope(const OrbLvExternalScope &) = delete;
    OrbLvExternalScope &operator=(const OrbLvExternalScope &) = delete;
};
#endif

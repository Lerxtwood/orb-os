#pragma once
#include <cstdint>
#include <cstring>
using lv_opa_t = uint8_t;
struct lv_color_t { uint16_t full; };
struct lv_point_t { int16_t x, y; };
struct lv_font_t { int base_line = 3; };
struct lv_font_glyph_dsc_t {
    int box_w = 6, box_h = 10, adv_w = 8, ofs_x = 0, ofs_y = 0;
};
constexpr int LV_COORD_MAX = 32767;
inline int lv_font_get_line_height(const lv_font_t *) { return 16; }
inline bool lv_font_get_glyph_dsc(const lv_font_t *, lv_font_glyph_dsc_t *g, uint32_t, uint32_t) {
    *g = {}; return true;
}
inline const uint8_t *lv_font_get_glyph_bitmap(const lv_font_t *, uint32_t) {
    static const uint8_t glyph[30] = {
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
    };
    return glyph;
}
inline void lv_txt_get_size(lv_point_t *p, const char *s, const lv_font_t *, int, int, int, int) {
    p->x = (int16_t)(8 * std::strlen(s)); p->y = 16;
}

// Minimal layout/style state for testing fixed_screen_overlay's real implementation.
using lv_coord_t = int16_t;
struct lv_area_t { lv_coord_t x1, y1, x2, y2; };
constexpr int LV_OBJ_FLAG_HIDDEN = 1, LV_PART_MAIN = 0;
struct lv_obj_t {
    lv_area_t coords{0,0,99,29};
    int angle = 0, pivotX = 0, pivotY = 0, styleWrites = 0;
    bool hidden = false;
};
inline bool lv_obj_has_flag(lv_obj_t *o, int) { return o->hidden; }
inline void lv_obj_update_layout(lv_obj_t *) {}
inline void lv_obj_refresh_ext_draw_size(lv_obj_t *) {}
inline void lv_obj_invalidate(lv_obj_t *) {}
inline void lv_obj_get_coords(lv_obj_t *o, lv_area_t *a) { *a = o->coords; }
inline int lv_obj_get_style_transform_angle(lv_obj_t *o, int) { return o->angle; }
inline int lv_obj_get_style_transform_pivot_x(lv_obj_t *o, int) { return o->pivotX; }
inline int lv_obj_get_style_transform_pivot_y(lv_obj_t *o, int) { return o->pivotY; }
inline void lv_obj_set_style_transform_angle(lv_obj_t *o, int v, int) { o->angle = v; ++o->styleWrites; }
inline void lv_obj_set_style_transform_pivot_x(lv_obj_t *o, int v, int) { o->pivotX = v; ++o->styleWrites; }
inline void lv_obj_set_style_transform_pivot_y(lv_obj_t *o, int v, int) { o->pivotY = v; ++o->styleWrites; }

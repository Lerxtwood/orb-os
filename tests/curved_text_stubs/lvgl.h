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

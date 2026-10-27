#pragma once
#include <lvgl.h>
#include "config.h"

// Cancel the panel rotation about the SCREEN centre, not the object's centre.
// This keeps both the original position and orientation, including child objects.
// Reapply after layout/text changes; setting an unchanged style is deliberately avoided.
inline void fixed_screen_overlay(lv_obj_t *obj, uint16_t displayDegrees) {
    if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    const int angle = (360 - displayDegrees % 360) % 360 * 10;
    if (!angle && !lv_obj_get_style_transform_angle(obj, LV_PART_MAIN)) return;
    lv_obj_update_layout(obj);
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    const lv_coord_t px = SCREEN_W / 2 - area.x1;
    const lv_coord_t py = SCREEN_H / 2 - area.y1;
    const bool pivotChanged = lv_obj_get_style_transform_pivot_x(obj, LV_PART_MAIN) != px
                           || lv_obj_get_style_transform_pivot_y(obj, LV_PART_MAIN) != py;
    if (lv_obj_get_style_transform_pivot_x(obj, LV_PART_MAIN) != px)
        lv_obj_set_style_transform_pivot_x(obj, px, LV_PART_MAIN);
    if (lv_obj_get_style_transform_pivot_y(obj, LV_PART_MAIN) != py)
        lv_obj_set_style_transform_pivot_y(obj, py, LV_PART_MAIN);
    if (lv_obj_get_style_transform_angle(obj, LV_PART_MAIN) != angle)
        lv_obj_set_style_transform_angle(obj, angle, LV_PART_MAIN);
    if (pivotChanged) {
        // Pivot properties don't carry LVGL's EXT_DRAW flag. A resized label can
        // otherwise keep its previous transformed bounds and clip at the edges.
        lv_obj_refresh_ext_draw_size(obj);
        lv_obj_invalidate(obj);
    }
}

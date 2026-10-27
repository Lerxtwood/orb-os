// Exercise the real text rasterizer with synthetic glyphs, independent of panel hardware.
#include "curved_text.h"
#include "fixed_screen_overlay.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

int main() {
    constexpr int W = 161, H = 161, guard = 32;
    const lv_font_t font;
    for (const int angle : {0, 15, 45, 90, 135, 180, 270, 315, 359}) {
        for (const bool background : {false, true}) {
            std::vector<uint8_t> memory(W * H * 3 + 2 * guard, 0);
            std::fill(memory.begin(), memory.begin() + guard, 0xA5);
            std::fill(memory.end() - guard, memory.end(), 0xA5);
            uint8_t *pixels = memory.data() + guard;
            curved_text::Target target{pixels, W, H, -(float)angle, 80, 80};
            curved_text::Pill pill{{0x1234}, (uint8_t)(background ? 80 : 0), 4};
            curved_text::draw_straight(target, &font, "ABCDEF", 70, 110,
                {0xFFFF}, 0, {0}, 1, 255, pill);
            // Simulate the panel rotation. A counter-rotated run must read horizontally,
            // including its optional background, at every tested display angle.
            double minX = 1000, maxX = -1000, minY = 1000, maxY = -1000;
            const double a = angle * 3.14159265358979323846 / 180;
            int count = 0;
            for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
                if (!pixels[(y * W + x) * 3 + 2]) continue;
                ++count;
                const double px = std::cos(a) * (x - 80) - std::sin(a) * (y - 80);
                const double py = std::sin(a) * (x - 80) + std::cos(a) * (y - 80);
                minX = std::fmin(minX, px); maxX = std::fmax(maxX, px);
                minY = std::fmin(minY, py); maxY = std::fmax(maxY, py);
            }
            assert(count > 200);
            assert(maxX - minX > 2.5 * (maxY - minY));
            assert(std::fabs((minX + maxX) * 0.5 - (70 - 80)) < 4);
            assert(std::fabs((minY + maxY) * 0.5 - (110 - 80)) < 2);
            for (int i = 0; i < guard; ++i) {
                assert(memory[i] == 0xA5);
                assert(memory[memory.size() - 1 - i] == 0xA5);
            }
        }
    }
    // Whole overlays must retain their ORIGINAL screen coordinates, not just stay
    // upright while orbiting with the map. Include corners/child positions, then
    // resize/re-align to model changing download text and photo dimensions.
    for (const int degrees : {0, 15, 45, 90, 180, 270, 315, 359}) {
        lv_obj_t obj;
        for (const auto area : {lv_area_t{93,47,372,92}, lv_area_t{107,92,362,373}}) {
            obj.coords = area;
            fixed_screen_overlay(&obj, degrees);
            const double a = degrees * 3.14159265358979323846 / 180;
            const double b = obj.angle * 3.14159265358979323846 / 1800;
            for (const auto pt : {lv_point_t{0,0}, lv_point_t{40,20},
                                 lv_point_t{(int16_t)(area.x2-area.x1),(int16_t)(area.y2-area.y1)}}) {
                const double x = area.x1 + obj.pivotX + std::cos(b)*(pt.x-obj.pivotX) - std::sin(b)*(pt.y-obj.pivotY);
                const double y = area.y1 + obj.pivotY + std::sin(b)*(pt.x-obj.pivotX) + std::cos(b)*(pt.y-obj.pivotY);
                const double cx = (SCREEN_W-1)*0.5, cy = (SCREEN_H-1)*0.5;
                const double physicalX = cx + std::cos(a)*(x-cx) - std::sin(a)*(y-cy);
                const double physicalY = cy + std::sin(a)*(x-cx) + std::cos(a)*(y-cy);
                assert(std::fabs(physicalX-(area.x1+pt.x)) <= 1.1);
                assert(std::fabs(physicalY-(area.y1+pt.y)) <= 1.1);
            }
            const int writes = obj.styleWrites;
            fixed_screen_overlay(&obj, degrees);
            assert(obj.styleWrites == writes);
        }
        fixed_screen_overlay(&obj, 0);
        assert(obj.angle == 0);
    }
    std::puts("Rotated text and fixed overlays: orientation, position, resizing and bounds passed");
}

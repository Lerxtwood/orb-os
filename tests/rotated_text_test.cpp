// Exercise the real text rasterizer with synthetic glyphs, independent of panel hardware.
#include "curved_text.h"
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
            curved_text::draw_straight(target, &font, "ABCDEF", 80, 80,
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
            assert(std::fabs(minX + maxX) < 8);
            for (int i = 0; i < guard; ++i) {
                assert(memory[i] == 0xA5);
                assert(memory[memory.size() - 1 - i] == 0xA5);
            }
        }
    }
    std::puts("Rotated text: horizontal glyphs/backgrounds and buffer bounds passed");
}

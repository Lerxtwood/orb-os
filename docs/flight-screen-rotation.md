# Flight Tracker rotation offset

The front page at `http://theorb.local/` has a Flight Tracker rotation offset
control. It accepts whole degrees from 0 to 359 clockwise, saves immediately, and
survives restarts. For example, 315 turns the screen 45 degrees counter-clockwise
when the existing global rotation is zero. Zero removes the flight offset.

This is an additional offset on top of the existing global display orientation.
Flight Tracker's entry/exit hooks apply/remove it. Weather shares the same LVGL
screen, but has its own app hooks, so it keeps the global orientation. The app
switcher also uses the global orientation. The offset rotates the completed
display output, including the plate, text, aircraft, sweep and selection card,
rather than changing the map's north/heading projection.

## Rendering cost

The existing implementation is in `src/display.cpp`; this feature reuses it and
does not allocate additional display buffers.

| Effective angle | Existing rendering path |
| --- | --- |
| 0 | No rotation; optionally stages dirty regions into one panel transaction per frame. |
| 180 | Reverses each pixel block in place. |
| 90 / 270 | Transposes each block into PSRAM scratch storage. |
| Other angles | Copies logical pixels into a full framebuffer, rotates dirty bounds, then inverse-samples output pixels using fixed-point nearest-neighbour sampling. |

Thus quarter turns still have a cost on this driver, but arbitrary angles have
more work: extra memory traffic, pixel sampling, potentially larger dirty bounds,
and overlapping writes when LVGL flushes multiple strips. Arbitrary rotation also
bypasses the staged/TE-synchronised zero-degree path. The result can be lower
animation frame rate and more visible tearing. Nearest-neighbour sampling can
make text and thin lines look jagged. The round display remains the same size;
there is no zoom-to-fit.

The logical RGB565 framebuffer is 466 x 466 x 2 = 434,312 bytes, and the rotation
scratch is 466 x 40 x 2 = 37,280 bytes. Both already allocate at boot in PSRAM.
There is no added per-pixel rotation work on other apps from this offset.

LVGL's documentation also distinguishes quarter-turn software rotation from
hardware rotation: <https://lvgl.io/docs/open/8.2/porting/display>.
The Orb's custom flush implementation, rather than LVGL's built-in rotation,
determines the actual costs described above. No on-device FPS comparison has
been performed for this change, so no numeric slowdown is claimed.

## Hardware verification

After installing the firmware:

1. Open the front page and save 315. Confirm that the whole Flight Tracker view
   turns 45 degrees counter-clockwise, including the text and background.
2. Open the switcher, then Clock, Weather and Settings. Confirm normal orientation.
   Return to Flight Tracker and confirm the offset returns.
3. Restart and verify the saved offset. Save zero and confirm the default returns.
4. Compare 0, 270 and 315 with the same theme, aircraft count and sweep settings.
   Allow warm-up, then record the serial `[memdbg]` FPS over several 15-second
   intervals for each angle. Compare animation smoothness and text edges too.
   `/health` flush timings can supplement this; `pushed_per_s` does not
   currently count the arbitrary-angle path and should not be used to compare it.
5. Check that blank, fractional and out-of-range POST values receive HTTP 400;
   valid values redirect to the front page only after the NVS write succeeds.

# Settings and app restoration

Settings-triggered restarts return to the app that was open immediately before
Settings. Web settings return to the currently open app, or the app before
Settings if Settings is open on the dial.

| Setting | Current restart behavior |
| --- | --- |
| Location (current, recent, or searched city) | Saves and restarts to reload location-dependent state together. |
| Display theme / installed design | Restarts to rebuild screens and load the selected theme's styles and artwork. |
| Successful Wi-Fi change (dial or setup portal) | Restarts for a clean web server and mDNS handover. |
| Web Save (location, range, radar theme, timezone) | Saves and restarts to apply the stored configuration. |
| Web large text | Restarts because font sizes are applied when the UI is built. |

Location does not inherently require a hardware restart: Studio and first-boot
auto-location already use `persist_location()` and `apply_location_live()`.
That live helper changes coordinates and requests a new aircraft query; it does
not force an immediate forecast refresh or reset every existing location-dependent
cache. Settings retains its existing full restart in this change.

The return request is saved only when a setting schedules a restart and is consumed
once at boot. Ordinary power-on and recovery restarts keep their existing boot
behavior. Factory reset still clears settings and opens setup. Wi-Fi setup and
missing-card notices take priority over restoration. If a new design hides the
previous app, restoration selects the first visible app instead. App IDs are
independent of theme display names and conditional build rosters.

Run the host regression checks with the project Python environment:

```powershell
& .pio\build-venv\Scripts\python.exe tools/test_settings_resume.py
```

The checks compile the actual shell switch/return functions against fake LVGL
and Preferences for all four launch/companion roster combinations.

On-device verification after an authorized flash: open Flight Tracker or News,
enter Settings, change location, and confirm the same app opens after restart.
Repeat with theme/design and Wi-Fi changes. Also verify that a design hiding
the previous app opens an available app. These manual device checks have not
been performed by the host tests.

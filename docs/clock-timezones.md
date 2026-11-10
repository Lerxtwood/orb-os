# Clock timezones and daylight saving

The system clock and PCF85063 RTC store UTC. NTP synchronizes UTC when Wi-Fi
connects, and the RTC seeds the clock before NTP is available. Clock faces use
`orb_local_time()` / `localtime_r()` to convert UTC using the saved POSIX timezone.
DST must be applied in this conversion, not added to NTP or the RTC.

NTP starts immediately after the boot Wi-Fi connection succeeds (or when the
background network task later connects). The first network-task iteration keeps
that request running. Servers are `time.cloudflare.com`, `pool.ntp.org`, and
`time.nist.gov`, with the SDK's normal retry/backoff and periodic synchronization.
No network-time wait blocks the UI. If the RTC has lost its time, the clock must
wait for a successful network reply; DNS/server/network failures can delay this.

Only the SNTP synchronization callback marks a network fix. A recent-looking
RTC seed does not count. The main core writes confirmed UTC fixes to the RTC,
retries failed writes, and saves later NTP corrections as well. This fixes an
earlier check that mistook RTC-seeded time for NTP and skipped saving the actual
network correction. The callback only records atomic state; it never uses I2C.

Read-only serial command `?orb time` reports UTC epoch, formatted local time,
timezone rule/name, RTC presence and whether it seeded this boot, NTP start and
last-sync boot milliseconds, sync count, and the sync last saved to the RTC.
Prefer `http://theorb.local/time` for the same snapshot over Wi-Fi, especially
when preserving a failed boot. Opening native USB serial can trigger a reset
even when the probe does not intentionally toggle DTR/RTS. Compare
`ntpStartedMs` with `lastNtpMs` to measure
network sync delay. `rtcSeeded: false` means the RTC could not supply valid time;
it does not by itself distinguish an empty backup battery from an I2C failure.

Previously, automatic IP location and Studio's location command saved only the
offset at setup time (for example UTC-6 in summer). That offset had no transition
rules and stayed an hour wrong after DST ended.

Automatic mode now stores a named IANA region and its recurring POSIX rules.
IP location requests the timezone name as well as its offset. The existing
Open-Meteo forecast request uses `timezone=auto`; its returned zone resolves the
saved coordinates, including manually searched locations. Results for an old
location are ignored. A resolved region's seasonal transitions then work offline
and without a reboot. Studio's old numeric-offset protocol remains supported as
a provisional value, and cannot overwrite the resolved rule for the same place.

On first use of the updated firmware, legacy automatically generated numeric
offsets are resolved on the next successful forecast (normally shortly after
Wi-Fi connects). Until that succeeds, the previous saved timezone stays in use.
Explicit timezone presets remain manual. The legacy web configuration page also
offers **Automatic (location and DST)** to switch back to location-based time.
New manual selections persist that choice so automatic forecasts cannot replace it.

NVS `tz` contains the POSIX rule, `tzName` the automatic region, and `tzAuto` the
mode. Rules are persisted only when they change. Known named recurring rules are
restored at boot, including while offline. The old fork's Spain-default migration
is retained only for installations without the new explicit mode flag.

`src/timezone_rules_data.h` is generated from the pinned official Python tzdata
2026.6 snapshot. Run `python tools/generate_timezone_rules.py` to regenerate it;
normal firmware builds do not access the network for timezone data. It includes
current recurring rules and aliases, rather than historical timezone transitions.
Regions with irregular/non-recurring political changes use their observed offset
from subsequent forecast updates. Future changes to recurring DST laws require
an updated rules snapshot/firmware, as with any embedded timezone database.

Run `python tools/test_timezone.py` with a C++17 compiler and GNU date. On this
Windows checkout, GNU date is supplied by Git for Windows; pass
`--compiler D:/git/Arduino/orb-os/.pio/build-venv/Scripts/python-zig.exe` for the
available compiler. Tests cover saved-offset migration, manual precedence,
lookup aliases, invalid responses, non-hour offsets, stale coordinates, and exact
2026 spring/fall boundaries for Denver, London and Sydney, plus no-DST Phoenix.

No device flash is performed by these tests.

# Orb + PrintSphere companion

Web installer: **https://lerxtwood.github.io/orb-os/**

The installer lists compatible GitHub releases and downloads their checked images
from a same-origin Pages mirror. It detects the connected device's flash layout.
Existing companion installations update only the two application slots. Standalone
Orb migration first checks the artwork cache and offers a verified, full recovery
backup; it preserves Orb settings and retained cache sectors. A completely erased
device can be initialized. Other layouts are refused without writing.

The browser supports Chrome/Edge desktop with Web Serial. Its recovery backup stays
on the user's computer and is never uploaded. Keep that file until the installation
is verified. The installer never erases the whole chip.

For a fresh Wi-Fi-sharing test, connect an existing companion device in the web
installer, open **Reset device settings**, acknowledge the deletion, and choose
**Reset both firmwares’ settings**. This clears both settings partitions (including
Wi-Fi, shared credentials, printer profiles, and cloud sign-ins) and restarts in
Orb. Firmware, artwork, uploaded sounds, and SD-card files are retained. Configure
Wi-Fi in Orb, then open PrintSphere without configuring its Wi-Fi separately.
This action does not install a firmware update; install the PrintSphere version
with Wi-Fi import before testing. Orb needs no Wi-Fi-sharing update. Reset is
unavailable for standalone or unknown layouts.
PrintSphere versions with SD printer backup restore printer/cloud configuration
from the card afterward. Delete the SD card's `/printsphere` backup directory
before resetting if you want to forget that configuration permanently.

For the 16 MiB ESP32-S3 AMOLED 1.75 device. Orb remains the normal firmware;
select **Printer**, then press to reboot into PrintSphere. From v2.16.27-companion,
jig the dial to open **Return to Orb**, then press the dial
to confirm. Turn to cancel, or let the menu close after eight seconds.
From v2.16.38-companion, turn either way and reverse within 250 ms to open
the menu immediately, with no limit on the length of either turn. The same
45 ms minimum reversal gap as Orb filters encoder bounce. Continued gesture
movement is absorbed until the dial has been quiet for 350 ms; a subsequent
turn cancels the menu. A fresh press after the menu opens confirms the return.
Only the running firmware serves its web interface.

PrintSphere's dial uses Orb's GPIO18(A)/17(B)/16(button) wiring and gesture timing.
Clockwise advances right through available print pages; counter-clockwise goes
left. It starts on printer status, stops at the ends, and skips unavailable pages.
Page movement waits until the dial has settled for 250 ms so a jig can open the
menu without bouncing between pages. Touch page swiping and
the old touch return shortcuts are removed; existing print-control taps and
brightness gestures remain. The existing power-save loop wakes the display for
dial activity, while interrupts retain quick turns and presses during rendering.
Input is routed by the existing LVGL worker, without another task stack.

Orb's configuration, USB theme protocol, web theme upload, SD themes, and theme
sync code are retained. PrintSphere uses a separate `ps_nvs` partition and imports
Orb's saved WiFi credentials whenever PrintSphere has no WiFi network of its own.
It reads Orb's normal saved station configuration from default NVS through the
ESP-IDF WiFi driver at startup, before switching the driver to RAM-only storage.
No shared-copy keys or Orb update are required. PrintSphere's own WiFi settings
take priority, and PrintSphere never saves its WiFi configuration over Orb's.
Configure the printer through
PrintSphere's existing interface. The original PrintSphere checkout is untouched.

## SD backup of printer setup

PrintSphere automatically backs up printer-binding settings after a live local
or cloud MQTT connection has been stable for five seconds. A cloud login alone
does not trigger backup. Changes are saved after two seconds without further
configuration writes. An existing setup is backed up too, so no
reconfiguration is needed after installing this feature; let it connect once.
The serial log confirms `Printer profiles and cloud settings backed up to SD`.

The card should stay inserted while using backup/restore. It is mounted only
for a restore or backup operation, then unmounted to release SPI/DMA and file
resources. Devices with existing configuration do not initialize SD before MQTT
starts. Backups wait when internal RAM is too low, and empty profile slots are
omitted to reduce allocation pressure.
The backup memory guard requires 8 KiB free internal RAM with a 4 KiB contiguous
block, matching the short SPI operation rather than reserving another MQTT task.
Larger ordinary allocations (above 1 KiB), including MQTT receive buffers,
prefer PSRAM in the companion build; internal RAM remains available for RTOS
task stacks, DMA and short backup operations. The file is
`/printsphere/printer-settings.json`, with a retained `.prev` copy and a temporary
`.tmp` file during updates. It contains cloud email/password, region and session
token; all printer profiles (serial, host, access code, name, model and cloud
binding); the active printer and source mode. These credentials are readable on
the card. Wi-Fi, artwork, display settings, and sound files are not backed up by
this feature. Cloud sessions may still expire or require fresh verification.

After a settings reset, PrintSphere restores the backup before starting its
printer clients, then uses its usual connection/login behavior. Existing or
partially entered printer/cloud setup takes priority: a network outage never
causes an older backup to replace current settings. Deliberately deleting all
profiles is retained across an ordinary reboot. Backups are restricted to the
same device, version checked, size limited, and protected by a corruption
checksum. A damaged or interrupted update falls back to the retained copy.
An interrupted NVS restore is retried on the next boot.
The authorized `/api/health` response includes `sd_backup_status`,
`heap_internal_free`, and `heap_internal_largest` for diagnosis; `saved` confirms
that the backup has been verified and promoted.

Missing, unreadable, or unformatted cards do not prevent normal PrintSphere use.
The firmware never formats a card. Orb firmware requires no changes.

The flash theme cache shrinks from 9.625 MiB to 4.5 MiB. The installer refuses to
truncate an existing cache. Larger future themes still use Orb's existing SD
fallback, which may be slower. PrintSphere has 4 MiB for firmware, 512 KiB for
settings, and 640 KiB for uploaded sounds.

## Build and initial installation

Run from the Orb repository in PowerShell. These examples use the local PlatformIO
venv; another Python with PlatformIO and pyserial can also be used.

```powershell
.\.pio\build-venv\Scripts\python.exe -m platformio run -e esp32-s3-amoled-175-companion
.\.pio\build-venv\Scripts\python.exe tools\companion\prepare_printsphere.py
powershell -NoProfile -ExecutionPolicy Bypass -File tools\companion\build_printsphere.ps1
```

Before migration, read **all 16 MiB** of the standalone Orb device with esptool
into `.pio/companion/orb-before-dualboot.bin`, and save its SHA256 digest in
`.pio/companion/orb-before-dualboot.sha256`. Never replace this recovery backup.
The backup and packaged image contain private settings; keep them local.

```powershell
.\.pio\build-venv\Scripts\python.exe tools\companion\package.py
.\.pio\build-venv\Scripts\python.exe tools\companion\test_package.py
.\.pio\build-venv\Scripts\python.exe tools\companion\flash.py install --port COM5
```

`package.py` checks the original layout, backup hash, both generated partition
tables, firmware identities/sizes, and theme-cache bounds. It retains Orb's NVS
and cached artwork, installs both firmwares with the newer PrintSphere bootloader,
and initializes the boot selector to Orb. Installation is only for migration from
the original standalone layout; repeating it would discard PrintSphere settings.

## Subsequent firmware updates and recovery

Build the appropriate companion firmware, then write only its app partition:

```powershell
.\.pio\build-venv\Scripts\python.exe tools\companion\flash.py update-orb --port COM5
.\.pio\build-venv\Scripts\python.exe tools\companion\flash.py update-printer --port COM5
```

These commands verify the live partition table and preserve settings, artwork,
and the other firmware. Do not use ordinary PlatformIO upload, IDF flash, or a
stock web **firmware** installer with this layout. Ordinary PlatformIO upload is
blocked for the companion environment. On-device firmware OTA remains disabled.
The **Orb Companion** web installer is the supported browser firmware updater.
Orb Studio theme uploads and web configuration are separate from firmware flashing.

To restore the complete pre-test standalone Orb flash:

```powershell
.\.pio\build-venv\Scripts\python.exe tools\companion\flash.py restore --port COM5
```

Restoration returns flash settings to their backup-time values; it does not roll
back files on the SD card. The helper verifies the backup hash before writing.

## Publishing releases and the website

Use the firmware version from the most recently merged upstream revision with
`-companion` appended: upstream `2.17.00` becomes `v2.17.00-companion`.
Keep upstream's `FW_VERSION` in source; the release workflow validates the tag
against it and stamps the companion suffix into the published firmware. Do not
increment a separate companion version counter. If that tag already exists,
resolve the release naming explicitly rather than overwriting a published tag.

The fork's `release.yml` builds both firmwares on a `v*` tag and publishes exactly
four public images plus `companion-release.json`. PrintSphere's source revision is
pinned in `printsphere-ref.txt`. Its settings-storage and installer-link adaptations
are applied by `prepare_printsphere.py` before building. Release builds stamp Orb
with the tag version and PrintSphere with the same version plus `-orb`.

Enable GitHub Pages with **GitHub Actions** as the source once. After publishing a
release, the reusable `companion-pages.yml` mirrors compatible releases, verifies
their SHA256 hashes, and deploys the installer. It can also be run manually to refresh
the release list. Draft releases are excluded; previews are labeled. The previous
standalone build environment remains available for development, but is not used for
the fork's combined releases.

Local preview from already-built firmware:

```powershell
python tools/companion/release_assets.py --output .pio/companion/public-release --version v2.16.26-companion --commit local --printer-commit 24bf0f4e237be151b036114b2ea9249c89d7a120
python tools/companion/build_site.py --site .pio/companion/site --local .pio/companion/public-release --tag v2.16.26-companion
python -m http.server 8080 --directory .pio/companion/site
```

`test_layout.mjs` tests protected flash regions and rejected layouts/cache sizes.
`test_release.py` tests public asset whitelisting and integrity checks.
`test_orb_wifi.cpp` checks the startup credential snapshot, open networks,
maximum-length SDK fields, read failures, and isolation from later driver changes.
It runs in both CI workflows with the WiFi test stubs.
`test_printer_backup.cpp` tests cloud/local round trips, excluded Wi-Fi settings,
device/version checks, malformed and oversized files, duplicate fields,
unchanged snapshots, retained copies, and interrupted file updates. It runs in
the release build against ESP-IDF's cJSON library.
`test_site.py` uses Playwright with a simulated device to exercise the browser's
update flow, corrupt downloads, foreign layouts, and mobile layout. The simulated
test does not flash hardware. `smoke.py` exercises real Orb configuration pages and
both existing USB/HTTP theme-file transports using a temporary folder.

`test_dial.cpp` runs the production quadrature decoder and gesture router with
direction changes, contact chatter, slow reversals, unrestricted turn lengths,
gesture-tail draining, menu timeout/cancellation, early presses, and millisecond
wrap. Compile with a host
C++17 compiler and `-Itools/companion`. Its tests also run in both CI workflows.

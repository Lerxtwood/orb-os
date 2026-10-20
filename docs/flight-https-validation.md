# Direct aircraft HTTPS validation — 2026-10-19

Firmware: 2.17.02. Hardware: the user's ESP32-S3 Orb on COM6, 8 MB PSRAM,
existing Aviator theme and saved WiFi/location settings.

## Build and transport

- Built with `platformio run -e esp32-s3-amoled-175` using the stock Arduino 3.1.3 / IDF 5.3 libraries.
- Confirmed the linked firmware includes `mbedtls_platform_set_calloc_free` and the PSRAM callback.
- An initial HTTP/1.0 trial returned 426 from adsb.lol, then automatically fetched 12 aircraft
  from adsb.fi over HTTPS. Further adsb.fi polls also returned 200. This exercised the real
  fallback path without disabling a working provider or bypassing rate limits.
- A laptop comparison confirmed adsb.lol returns 426 for HTTP/1.0 and 200 for HTTP/1.1.
  The final client uses HTTP/1.1 and StreamUtils to handle chunked bodies while streaming JSON.
- The final firmware boot log confirms the PSRAM TLS allocator is enabled. Direct adsb.lol
  requests return HTTP 200 and parse 12 retained aircraft. The internal largest free block
  remains 31,732 bytes across these requests.
- A final 180-second serial capture recorded 19/19 successful adsb.lol HTTPS requests,
  each retaining 12 aircraft, with zero TLS allocation errors, crash markers or unexpected
  boot banners. After entering Flights, PSRAM free during requests settled around 3.63 MB;
  its largest free block stayed at 1,736,692 bytes. Internal largest free block stayed
  at 31,732 bytes throughout. Raw local captures are in `.pio/tls-soak.log` (final trial)
  and `.pio/tls-device.log` (the earlier fallback trial).
- Final build and upload to COM6 passed; `git diff --check` passed. The serial capture
  has closed and the device was left displaying Flights.

## Limits of this validation

The certificate policy remains the project's existing `ADSB_HTTPS_INSECURE=1`:
TLS encrypts the connection but does not verify the server certificate. The alternative
SDK root-bundle path is implemented but has not been tested on hardware.

The installed theme exposes Clock, Flights, News and Settings; a weather-radar screen was
not available for a concurrent weather-radar memory stress test. Multi-hour stability
needs a longer soak than this session.

# Aircraft, route, and photo data sources

The device contacts both providers directly over HTTPS; no gateway is used for aircraft positions.

## Primary: adsb.lol

`GET https://api.adsb.lol/v2/point/{lat}/{lon}/{radius_nm}`

## Fallback: adsb.fi

`GET https://opendata.adsb.fi/api/v3/lat/{lat}/lon/{lon}/dist/{radius_nm}`

Use v3 for geographic queries: the older adsb.fi v2 geographic response has a different format.
Both providers return an `ac` array; the parser also accepts `aircraft`. Radius is in nautical
miles and capped at 250. Fields used: `hex`, `flight`, `t`, `lat`, `lon`, `alt_baro`, `track`,
`true_heading`, `gs`, `baro_rate`, `squawk`, `seen_pos`, and `dbFlags`.

The client tries adsb.lol first, then adsb.fi if it fails or is cooling down. Transport/server
failures pause that provider for 60 seconds; HTTP 4xx responses pause it for five minutes.
A successful fallback is a successful poll, so it does not trigger the global failure backoff.
There is at most one request per provider per poll, with at least 1.1 seconds between requests
to the same provider. No DNS-edge rotation is used to bypass rate limits. Multiple devices
sharing a public IP must collectively respect the provider limit.

## Flight routes: FlightAware

The active route lookup in [`route_client.cpp`](../src/route_client.cpp) contacts
[FlightAware](https://www.flightaware.com/) directly by flight callsign:

`GET https://www.flightaware.com/live/flight/{callsign}`

The device reads the public flight page's `origin` and `destination` HTML meta
tags using the streaming parser in [`flightaware_route.h`](../src/flightaware_route.h).
These supply the airport codes shown on flight cards, rather than a detailed
flight path. This implementation uses the public web page, not FlightAware's
AeroAPI, and sends no account credentials or API key. Missing tags, an unavailable
page, or a rejected request produce **No route**.

Successful routes are cached by callsign for 15 minutes, including an NVS cache
that survives restarts when valid wall-clock time is available. The persistent
cache holds up to 200 callsigns. Unavailable results are cached in memory for
one minute to avoid repeated requests.

### Legacy route gateway: adsbdb

The older gateway implementation remains in `route_client.cpp`, but
`DIRECT_FLIGHTAWARE_POC = true` selects the direct FlightAware lookup. The gateway
is **not an automatic fallback** when FlightAware fails.

The retained Arduino path is:

`GET http://buildtheorb.zionbrock.workers.dev/api/route?callsign={callsign}`

Its source comments identify [adsbdb](https://www.adsbdb.com/), using
`/v0/callsign/{callsign}`, as the upstream route source. The gateway returns a
small `{"from":"SMF","to":"PHX"}` response. The gateway implementation itself
is not included in this checkout.

## Aircraft photos

Photo lookup and downloading are implemented in
[`photo_client.cpp`](../src/photo_client.cpp). They are separate from aircraft
position and route lookup.

### Primary: PlaneSpotters.net

`GET https://api.planespotters.net/pub/photos/hex/{icao_hex}`

The device queries the [PlaneSpotters.net](https://www.planespotters.net/) photo
API by the aircraft's ICAO hex address. It selects the first result's
`thumbnail_large.src` and retains the `photographer` credit. The photo viewer
shows the photographer's name and `planespotters.net` beside the image.

### Generic aircraft-type fallback: Wikimedia Commons

If the aircraft-specific image is unavailable or cannot be downloaded/decoded,
the device searches [Wikimedia Commons](https://commons.wikimedia.org/) through
its MediaWiki API at `https://commons.wikimedia.org/w/api.php`.

The search uses the aircraft type designator, such as `B738`, followed by
`aircraft filetype:bitmap`. It searches the file namespace, considers up to three
results, and requests `imageinfo` thumbnail URLs, MIME types, artist names, and
short license names. The highest-ranked usable image thumbnail is selected.
This is a generic image of the aircraft type, not necessarily the tracked plane.
The displayed credit starts with **Generic** and includes the type, Wikimedia
Commons, artist, and supplied license name.

### Image resizing and conversion: weserv / wsrv.nl

Both photo sources' thumbnail URLs are passed to the
[weserv image proxy](https://wsrv.nl/) using:

`GET https://images.weserv.nl/?url={encoded_source_url}&w={width}&h={height}&fit=inside&output=jpg`

The proxy fetches, resizes, and converts the source image to JPEG for the device's
decoder, including conversion of progressive JPEGs. It is an image-processing
service, not the original photographer or photo collection. The current photo
canvas is 232 by 156 pixels. Up to four decoded photos are cached in RAM for
15 minutes; unavailable results are cached for one minute.

## TLS memory

Before networking starts, `tls_memory_begin()` installs the stock mbedTLS runtime allocator
callbacks. TLS allocations explicitly use PSRAM, rather than the SDK default of internal RAM.
The normal malloc threshold alone does not affect SDK allocations that require internal RAM.
The hook is global to mbedTLS and must never be swapped while sessions are active.
Each aircraft request closes its TLS connection before weather/photo requests run; JSON is
streamed and parsed in PSRAM. Requests use HTTP/1.1 (adsb.lol rejects HTTP/1.0 with status 426). StreamUtils decodes
chunked responses before JSON parsing, without accumulating the full response body.
The default certificate policy remains `ADSB_HTTPS_INSECURE=1` (encryption without server
certificate verification). Setting it to 0 enables the SDK's built-in root CA bundle.

## Terms and attribution

- adsb.lol: [API documentation](https://api.adsb.lol/docs), ODbL data.
- [adsb.fi](https://adsb.fi): personal, non-commercial use with attribution and a home-page link;
  public API limit is one request per second per IP. See [official terms](https://github.com/adsbfi/opendata).
- FlightAware route pages: [FlightAware](https://www.flightaware.com/).
- PlaneSpotters photos remain subject to the photographer's rights and
  [PlaneSpotters.net's terms](https://www.planespotters.net/legal/termsofuse).
  The firmware retains and displays the API's photographer credit.
- Wikimedia Commons photos have individual licenses; the firmware displays the
  returned artist and short license name. See
  [Commons reuse guidance](https://commons.wikimedia.org/wiki/Commons:Reusing_content_outside_Wikimedia).
- Image processing: [wsrv.nl documentation](https://wsrv.nl/docs/).

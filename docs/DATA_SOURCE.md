# Aircraft data sources

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

Airport routes are separate: `route_client.cpp` calls the existing Orb gateway, documented
there as querying adsbdb.com. Switching aircraft transport does not change route lookup.

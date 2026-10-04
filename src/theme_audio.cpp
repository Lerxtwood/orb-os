#include "theme_audio.h"

#ifdef ARDUINO
#include "audio.h"
#else
// The simulator builds no audio module, so nothing can be playing and nothing needs releasing
// before it is freed. Compiled out rather than faked, the same way wind_notice does it.
#define audio_release_pcm(p) ((void)0)
#endif
#include "theme_sd.h"
#include "theme_select.h"
#include "theme_style.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <stdio.h>

namespace {

uint8_t *s_wind  = nullptr; size_t s_windLen  = 0;

// The tick bank, and the bag that deals from it.
uint8_t *s_tick[theme_audio::TICK_SLOTS_MAX] = { nullptr };
size_t   s_tickLen[theme_audio::TICK_SLOTS_MAX] = { 0 };
int      s_tickN = 0;
uint8_t  s_bag[theme_audio::TICK_SLOTS_MAX];   // the current shuffle
int      s_bagAt = 0;                          // how far through it we are
int      s_bagLast = -1;                       // the clip the previous shuffle ended on

// Small xorshift rather than rand(), so the sequence does not depend on whatever else in the
// firmware happens to have called rand() first. Seeded once from the clock.
uint32_t s_rng = 0;
uint32_t rnd() {
    if (!s_rng) {
#ifdef ARDUINO
        s_rng = (uint32_t)esp_random() | 1u;
#else
        s_rng = 0x9E3779B9u;
#endif
    }
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
    return s_rng;
}

void reshuffle() {
    for (int i = 0; i < s_tickN; ++i) s_bag[i] = (uint8_t)i;
    for (int i = s_tickN - 1; i > 0; --i) {            // Fisher-Yates
        const int j = (int)(rnd() % (uint32_t)(i + 1));
        const uint8_t t = s_bag[i]; s_bag[i] = s_bag[j]; s_bag[j] = t;
    }
    // Never open a shuffle with the clip the last one closed on: back to back repeats are the
    // one thing the ear catches instantly, and they are what pure randomness gives you.
    if (s_tickN > 1 && s_bag[0] == (uint8_t)s_bagLast) {
        const uint8_t t = s_bag[0]; s_bag[0] = s_bag[s_tickN - 1]; s_bag[s_tickN - 1] = t;
    }
    s_bagAt = 0;
}

// A ceiling with real headroom over what Studio can produce, which is the only number that
// matters here. It was 64 KB on the arithmetic that this format runs at 32 KB per second. It
// is 16 kHz SIXTEEN BIT STEREO: 64,000 bytes per second, so that ceiling was barely one
// second while Studio would emit half as much again. A click baked to 68,544 bytes, sailed
// past Studio, and was refused here with nothing on screen to say why.
//
// 256 KB now, which is four seconds against a Studio limit of two. Twice what the other end
// can produce rather than a hair above it, because these two numbers have already drifted
// apart once and the cost was an evening of hearing the wrong sound.
constexpr size_t WIND_MAX_BYTES  = 256 * 1024;
// A tick fires every second, so it is held in memory and there are several of it. 64 KB each
// is a full second of this format, which is already far longer than any click: the ceiling is
// here to stop a mistake costing half a megabyte, not to be reached.
constexpr size_t TICK_MAX_BYTES  = 64 * 1024;
uint8_t *load_one(const char *name, size_t maxBytes, size_t &outLen) {
    outLen = 0;
    const char *slug = theme_select::activeSlug();
    if (!slug || !slug[0]) return nullptr;
    // Only what the theme SAYS it ships. A push never deletes from the card, so a sound from
    // an older push of the same theme would otherwise keep playing after it was removed —
    // the same trap custom_sprite documents for artwork.
    if (!theme_style::hasAsset(name)) return nullptr;

    char path[64];
    snprintf(path, sizeof(path), "/themes/%s/%s", slug, name);
    size_t len = 0;
    uint8_t *buf = theme_sd::read_whole(path, len, maxBytes);
#ifdef ARDUINO
    if (buf) Serial.printf("[theme_audio] %s: %u bytes\n", path, (unsigned)len);
    else     Serial.printf("[theme_audio] %s: not loaded\n", path);
#endif
    // An odd length would put the stream half a sample out and every frame after it would be
    // noise, so the tail is dropped rather than trusted.
    outLen = len & ~(size_t)1;
    return buf;
}

}  // namespace

void theme_audio::load() {
    // Released before freeing, not just dropped: a theme can be applied while its own winding
    // tick is still sounding, and the playback task would then be reading memory this is
    // about to hand back.
    if (s_wind)  { audio_release_pcm(s_wind);  theme_sd::free(s_wind);  s_wind  = nullptr; s_windLen  = 0; }
    s_wind = load_one("wind.pcm", WIND_MAX_BYTES, s_windLen);

    // The tick bank. Numbered from one because that is how somebody recording six takes of a
    // clock names the files, and a gap just ends the bank rather than being an error: five
    // good takes are a perfectly good bank.
    for (int i = 0; i < theme_audio::TICK_SLOTS_MAX; ++i) {
        if (s_tick[i]) { audio_release_pcm(s_tick[i]); theme_sd::free(s_tick[i]); s_tick[i] = nullptr; s_tickLen[i] = 0; }
    }
    s_tickN = 0; s_bagAt = 0; s_bagLast = -1;
    for (int i = 0; i < theme_audio::TICK_SLOTS_MAX; ++i) {
        char nm[16]; snprintf(nm, sizeof(nm), "tick%d.pcm", i + 1);
        size_t len = 0;
        uint8_t *buf = load_one(nm, TICK_MAX_BYTES, len);
        if (!buf || !len) break;
        s_tick[s_tickN] = buf; s_tickLen[s_tickN] = len; s_tickN++;
    }
    if (s_tickN) reshuffle();
#ifdef ARDUINO
    if (s_tickN) Serial.printf("[theme_audio] tick bank: %d take(s)\n", s_tickN);
#endif
    // The chime is NOT loaded here any more. It belongs to the device rather than to the worn
    // theme (chime_library), and it is streamed off the card when it rings rather than held,
    // so a theme with a two minute chime costs this nothing at all.
}

const uint8_t *theme_audio::wind(size_t &bytes)  { bytes = s_windLen;  return s_wind; }

void theme_audio::testBag(int n, uint8_t *out, int draws) {
    const int keepN = s_tickN; const int keepAt = s_bagAt; const int keepLast = s_bagLast;
    s_tickN = n; s_bagAt = n; s_bagLast = -1;      // s_bagAt >= n forces the first shuffle
    for (int i = 0; i < draws; ++i) {
        if (s_bagAt >= s_tickN) reshuffle();
        out[i] = s_bag[s_bagAt++];
        s_bagLast = out[i];
    }
    s_tickN = keepN; s_bagAt = keepAt; s_bagLast = keepLast;
}

int theme_audio::tickCount() { return s_tickN; }

const uint8_t *theme_audio::nextTick(size_t &bytes) {
    bytes = 0;
    if (s_tickN <= 0) return nullptr;
    if (s_bagAt >= s_tickN) reshuffle();
    const int k = s_bag[s_bagAt++];
    s_bagLast = k;
    bytes = s_tickLen[k];
    return s_tick[k];
}

#pragma once

#include <stddef.h>
#include <stdint.h>

// Sounds a theme brings with it.
//
// Two of them, and they are not the same kind of thing. `wind.pcm` is a tick, fired once per
// detent while somebody is turning the knob to wind the clock, so it has to be short and it
// has to be cheap. `chime.pcm` is the hour, fired once every sixty minutes, so it can be a
// few seconds of something proper.
//
// WHY THE THEME AND NOT THE DEVICE. Zion's vintage radio is the whole argument: the detail
// people talked about was the AM static between stations, which was a SOUND, and it belonged
// to that object rather than to a settings menu. A Steam Punk clock and an Aviator chronometer
// should not click the same way any more than they should share a typeface.
//
// FORMAT. Raw PCM, 16 kHz, 16-bit signed, stereo interleaved, no container — exactly what
// audio.cpp already streams for the built-in chimes, so playback is the code that was already
// there. Studio does the decoding and resampling in the browser, where there is a real audio
// stack, rather than asking an ESP32 to parse an MP3.
//
// Both are OPTIONAL. Absent, the Orb uses its built-in tick and its built-in chime, which is
// what every theme written before this does.

namespace theme_audio {

// Load whatever the active theme ships, freeing whatever the last one did. Call when a theme
// becomes active, on the same path that applies the rest of its settings.
void load();

// The clock's own tick, as a BANK of recordings rather than one.
//
// A clock heard for an hour is the hardest sound on this device to get right, because the ear
// is extremely good at finding a loop. One recording played every second is a one second loop
// and it is audible within about a minute: it stops sounding like a clock and starts sounding
// like a sample. So a theme ships several takes of the same real click, and the Orb plays a
// different one each second.
//
// NOT round robin, despite being asked for in those words, and this is the one place worth
// arguing with the brief. Cycling 1-2-3-4-5-6 is still a loop, just a six second one, and six
// seconds is well inside what somebody notices at a desk. This plays a SHUFFLE BAG: the clips
// are shuffled, played through once each so no take is starved, then reshuffled, with the
// first of a new shuffle never equal to the last of the old one. No period to find, and no
// clip ever doubled back to back, which is the artefact pure randomness would give.
//
// Same format as every other sound here: raw PCM, 16 kHz, 16-bit signed, stereo interleaved.
// At 64,000 bytes a second a 150 ms click is about 9.6 KB, so a bank of six costs under 60 KB
// and is held in memory like the winding tick rather than streamed like a chime.
constexpr int TICK_SLOTS_MAX = 8;

// How many takes the worn theme actually shipped. Zero means it does not tick, which is every
// theme written before this one.
int tickCount();

// The next click to play, chosen by the shuffle above. Null when the theme ships none.
// Advances the bag, so call it once per tick and not for a preview.
const uint8_t *nextTick(size_t &bytes);

// Simulator only: the sequence the bag WOULD deal for n clips, without needing any audio
// loaded. The organic feel is the entire point of the bank, so it is checked rather than
// asserted in a comment.
void testBag(int n, uint8_t *out, int draws);

// Null when this theme ships no winding sound, which is the normal case.
//
// The chime is deliberately NOT here. It belongs to the device rather than to the worn theme
// (see chime_library) and it is streamed off the card as it rings rather than held, so a two
// minute chime costs this nothing.
const uint8_t *wind(size_t &bytes);

}  // namespace theme_audio

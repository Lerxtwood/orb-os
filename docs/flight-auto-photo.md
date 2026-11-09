# Flight cards and close-pass photos

Open **Flight Tracker Options** at `http://theorb.local/` and enable
**Alert and show image when a flight enters the close-pass radius** in the
**Flight cards & automatic displays** section. Set **Close-pass distance
(miles)** and save the flight display options. The default is 5 ordinary miles; accepted values are 0.1 to
100 miles in 0.1-mile steps, independent of the Orb's other unit settings.

The Orb checks each visible airborne flight's actual reported horizontal distance
from the current home location. A flight qualifies only when it is inside the
configured radius; a predicted future flyover no longer triggers a preview.
Heading and speed are not required. Ground traffic and aging positions do not
start a preview. Distance updates with the flight feed; altitude is not part of it.

A qualifying flight plays a lower 660 Hz tone (ordinary arrivals use 880 Hz),
highlights for one second, then opens the aircraft photo viewer. Its image remains
for three seconds after loading completes, then returns to the same highlighted
flight and its info card. Sound respects flight-sound, volume,
and mute settings. The close-pass checkbox controls this distinct cue independently
of the ordinary arrival alert mode. Existing credits, photo caching, and generic
aircraft fallback are reused. Network work stays on the background task. An
unavailable image shows its status for three seconds; lookups taking more than
30 seconds are dismissed.

Already-visible flights can qualify when opening Flight Tracker or enabling the
option if already inside the radius. Simultaneous
candidates queue. Each flight alerts once per tracked in-range encounter;
passing through a Keep-out Area or small distance changes does not replay its
close-pass notification. Leaving range or changing location/range establishes a
new encounter; reopening the tracker or changing options starts a fresh assessment.

Enable **Automatically show flight info for new aircraft** under Flight Tracker
Options to queue a highlight and info card with the normal arrival tone for
newly visible ordinary arrivals. This separate option defaults to off and
persists under the `flightCards` NVS key. With it off, normal arrival sounds
remain enabled according to the existing sound settings, without opening cards.
Changing the option clears queued cards; a card already being read may finish.
Close-pass previews and their returned info cards keep their independent setting.
Automatic cards finish before another arrival takes over;
close-pass previews take priority when choosing the next queued flight. A
close-pass preview returns to its own info card, without replaying a duplicate
arrival card. Knob input still takes control, and new arrivals first observed
during manual selection are skipped.

All cards, automatic or manual, use the duration selected in the same section:
**1–3600 seconds after route lookup completion** (default 5 seconds), or
**Stay visible until I choose another flight or the aircraft leaves the current view / selected range**. A completed miss counts
as completion; cached routes start immediately. Without a callsign, the timer
starts when the card opens. Closing a photo gives the returned card a fresh
configured reading window, or waits for its route if still pending. The route
line reads **Loading route** while pending and **No route** for a completed miss
or missing callsign, on both themed and stock cards. Stock cards do not retain
an untimed second image after the photo viewer closes.

Stay visible overrides both automatic arrival cards and close-pass previews,
including their distinct close-pass tone. Automatic preferences remain saved;
switching back to timed mode restores them. Ordinary arrival sounds still follow
the flight-sound and mute settings. A card dismisses if its aircraft leaves the
selected range, disappears from the feed, or is hidden by the current view's
Keep-out Areas or display limit. Manually selecting another flight replaces
the card; leaving Flight Tracker clears it.

Flights hidden by theme Keep-out Areas or display limits do not start previews.
Visibility is checked during gliding as well as feed updates. Masked or no-longer
qualifying queued flights are removed. Once a preview starts, its image window
is preserved even if its distance changes, provided it stays visible. Knob
input takes control and discards queued previews; qualifications first detected
during manual selection are skipped. Leaving the tracker or disabling the option
cancels automatic previews.

Ordinary arrival sounds remain separate and respect visibility. When an arrival
also qualifies for an enabled close-pass preview, its lower close-pass tone
replaces that arrival beep. Emergency/military and proximity alerts retain their
independent feed-based triggers and may sound for hidden aircraft.

The duration, stay-visible flag, arrival option, close-pass option and distance
share one atomic `flightDisplay` NVS value. Existing `flightCards`, `flightClose`
and older `flightPhoto` preferences are migrated on read. Automatic displays
remain disabled by default on new installations.

Hardware checks after installing firmware 2.18.09:

1. Save a distance and enable state; reload and restart to verify persistence.
2. Approach the configured radius from outside: no preview before actual entry.
   Check a flight already inside the radius when opening the tracker as well.
3. Confirm a qualifying flight highlights, plays the lower tone, then shows its
   image for three seconds after loading, then keeps its highlight and info card.
   An ordinary arrival should show its info card with no automatic photo when
   automatic arrival cards are enabled; with them disabled it should only sound.
4. Change the distance while aircraft are visible and verify reassessment.
5. Confirm no repeated close-pass tone after a Keep-out Area crossing or small
   distance change. Check simultaneous candidates queue.
6. Test knob takeover, mute/flight sounds off, app exit, missing/slow images, and
   a queued flight leaving the close-pass radius.
7. Change the duration, exercise a slow/cached/missing route, and verify dismissal
   after the configured interval. Verify Stay visible blocks all automatic
   displays, stays up for a visible flight, dismisses on feed loss/range exit or
   view exclusion, and permits manual selection.

`tests/flight_close_pass_test.cpp` checks actual-distance entry, boundaries,
invalid data, units, and settings serialization/validation.
`tests/flight_auto_photo_test.cpp` checks transitions, initial close-pass
qualification, queue pruning, cancellation, timing, and tick rollover.
`tests/flight_card_lifetime_test.cpp` checks early/late/cached/missing route
results, pending requests, photo-to-card timing, selection changes, rollover,
configurable duration, pinning, input validation and atomic settings round trips.

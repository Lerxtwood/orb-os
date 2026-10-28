# Predicted close-pass photos

Open **Flight Tracker Options** at `http://theorb.local/` and enable
**Alert and show image for predicted close passes**. Set **Close-pass distance
(miles)** and save. The default is 5 ordinary miles; accepted values are 0.1 to
100 miles in 0.1-mile steps, independent of the Orb's other unit settings.

For each visible airborne flight with known ground track and speed, the Orb
estimates the closest horizontal distance along its current great-circle track.
A flight qualifies if that point is ahead of it, within the next 15 minutes,
and within the configured distance of the current home location. This assumes
unchanged track and ground speed; altitude is not part of the distance. Flights
travelling away, ground traffic, stationary/unknown motion, and aging positions
do not start a preview. Predictions update with the flight feed.

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
option, and a later course change can make a flight eligible. Simultaneous
candidates queue. Each flight alerts once per tracked in-range encounter;
passing through a Keep-out Area or small prediction changes does not replay its
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

All cards, automatic or manual, remain visible for at least ten seconds and wait
for route completion. Their dismissal time is the later of ten seconds after
selection/last knob input and three seconds after route completion, including a
completed miss. Closing a photo starts a fresh ten-second card window. The route
line reads **Loading route** while pending and **No route** for a completed miss
or missing callsign, on both themed and stock cards. Stock cards do not retain
an untimed second image after the photo viewer closes.

Flights hidden by theme Keep-out Areas or display limits do not start previews.
Visibility is checked during gliding as well as feed updates. Masked or no-longer
qualifying queued flights are removed. Once a preview starts, its image window
is preserved even if its prediction changes, provided it stays visible. Knob
input takes control and discards queued previews; qualifications first detected
during manual selection are skipped. Leaving the tracker or disabling the option
cancels automatic previews.

Ordinary arrival sounds remain separate and respect visibility. When an arrival
also qualifies for an enabled close-pass preview, its lower close-pass tone
replaces that arrival beep. Emergency/military and proximity alerts retain their
independent feed-based triggers and may sound for hidden aircraft.

The enabled flag and distance share one atomic `flightClose` NVS value. The old
`flightPhoto` checkbox is migrated on read, preserving its enabled state and using
5 miles until the new options are saved. New installations default to disabled.

Hardware checks after installing firmware 2.18.09:

1. Save a distance and enable state; reload and restart to verify persistence.
2. Compare an inbound close pass, an inbound miss, and a departing flight.
3. Confirm a qualifying flight highlights, plays the lower tone, then shows its
   image for three seconds after loading, then keeps its highlight and info card.
   An ordinary arrival should show its info card with no automatic photo when
   automatic arrival cards are enabled; with them disabled it should only sound.
4. Change the distance while aircraft are visible and verify reassessment.
5. Confirm no repeated close-pass tone after a Keep-out Area crossing or small
   prediction change. Check simultaneous candidates queue.
6. Test knob takeover, mute/flight sounds off, app exit, missing/slow images, and
   a queued flight changing course or leaving range.

`tests/flight_close_pass_test.cpp` checks prediction geometry, direction, missing
motion, time and distance cutoffs, units, and settings serialization/validation.
`tests/flight_auto_photo_test.cpp` checks transitions, initial prediction
qualification, queue pruning, cancellation, timing, and tick rollover.
`tests/flight_card_lifetime_test.cpp` checks early/late/cached/missing route
results, pending requests, photo-to-card timing, selection changes, and rollover.

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
for five seconds after loading completes. Sound respects flight-sound, volume,
and mute settings. The close-pass checkbox controls this distinct cue independently
of the ordinary arrival alert mode. Existing credits, photo caching, and generic
aircraft fallback are reused. Network work stays on the background task. An
unavailable image shows its status for five seconds; lookups taking more than
30 seconds are dismissed.

Already-visible flights can qualify when opening Flight Tracker or enabling the
option, and a later course change can make a flight eligible. Simultaneous
candidates queue. Each flight alerts once per tracked in-range encounter;
passing through a Keep-out Area or small prediction changes does not replay its
close-pass notification. Leaving range or changing location/range establishes a
new encounter; reopening the tracker or changing options starts a fresh assessment.

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

Hardware checks after installing firmware 2.18.07:

1. Save a distance and enable state; reload and restart to verify persistence.
2. Compare an inbound close pass, an inbound miss, and a departing flight.
3. Confirm a qualifying flight highlights, plays the lower tone, then shows its
   image for five seconds after loading. A screen arrival alone must not show it.
4. Change the distance while aircraft are visible and verify reassessment.
5. Confirm no repeated close-pass tone after a Keep-out Area crossing or small
   prediction change. Check simultaneous candidates queue.
6. Test knob takeover, mute/flight sounds off, app exit, missing/slow images, and
   a queued flight changing course or leaving range.

`tests/flight_close_pass_test.cpp` checks prediction geometry, direction, missing
motion, time and distance cutoffs, units, and settings serialization/validation.
`tests/flight_auto_photo_test.cpp` checks transitions, initial prediction
qualification, queue pruning, cancellation, timing, and tick rollover.

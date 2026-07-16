# 08 — DVS (timecode vinyl) + controller coexistence

Status: **core fixes landed on branch `dvs-cueing-2026-07-14`** (branched off
`accessibility-improvements-2026-06-25`). Code-complete with unit tests;
**needs hardware validation** with a timecode setup (user has a Numark
Scratch, a DVS mixer with timecode inputs).

## The complaint

"The cueing implementation jumps backwards and forwards / gets stuck" when
using digital vinyl alongside controllers.

## Root causes found (all in the play-state arbitration)

1. **Cue/vinyl play fight.** While the record spins, `VinylControlXwax`
   re-forces `play` on every analysis window
   (`togglePlayButton(checkSteadyPitch(...))`). A CUE press did
   `play=0 + seek`, vinyl instantly forced play back on → the deck bounced
   between the cue point and playback.
2. **Preview killer.** With the needle up, the no-signal branch force-stopped
   play *continuously* (as soon as the file position moved 0.3 s), so holding
   CUE or a hotcue to preview from keyboard/controller died after 0.3 s —
   "gets stuck".
3. **Dropout cue-yank.** Every brief timecode dropout set `m_bForceResync`;
   on reacquisition in relative mode with needle-drop cueing on, it re-seeked
   to the nearest cue. Marginal signal = playback repeatedly yanked backwards.

## What was changed

- New CO `[ChannelN],vinylcontrol_transport_active` (created in
  `VinylControlControl`, written by `VinylControlXwax::togglePlayButton`,
  cleared on disable/constant mode/destruction). True exactly while the
  timecode signal owns the play state.
- `CueControl::isVinylTransportActive()` gates all stop-like cue actions:
  `cueCDJ`/`cueDenon`/`cuePlay` presses while playing become **seek-only**
  (Serato-style "jump and keep playing"), `cueGotoAndStop`/
  `hotcueGotoAndStop` degrade to gotos, preview releases don't force
  `play=0`/seek-back (they latch). Normal semantics whenever the needle is up
  or vinyl is disabled — that's what makes controller + DVS coexist.
- `VinylControlXwax` no-signal stop is now **one-shot per signal loss**
  (`m_bSignalLostStopIssued`), so software transport works with the needle
  up. The continuous `rate_ratio = 1.0` reset moved inside the one-shot too
  (the pitch fader was being snapped back every 23 ms while stopped).
- Needle-drop cueing seeks only happen if the signal was gone
  ≥ `kNeedleDropCueSeconds` (0.4 s, anon ns in vinylcontrolxwax.cpp) —
  `m_bResyncFromDropout` + `m_dSignalLostSeconds` distinguish a dropout
  reacquire from track-change/mode-change resyncs, which still seek.
- Announcements (always on, not pref-gated): `vinylcontrol_enabled`
  ("vinyl control on/off"), `vinylcontrol_mode`
  ("vinyl absolute/relative/constant mode" — includes the automatic flips:
  loop/seek drops absolute→relative, record end →constant),
  `vinylcontrol_cueing` ("needle drop cueing off / goes to cue point / goes
  to nearest hotcue").
- ACCESSIBILITY_GUIDE.md gained a "Timecode vinyl (DVS)" section (keyboard
  bindings Ctrl+T/Y/U/I enable, Ctrl+Shift+Y/U mode, Ctrl+Alt+Y/U cueing).

## Tests

- `CueControlVinylTest.*` (cuecontrol_test.cpp) — seek-only vs normal stop
  arbitration, incl. stale-transport-flag-with-vinyl-disabled case.
- `AnnouncementManagerVinylTest.*` — the three announcement observers.
- The xwax analysis loop has no test harness (needs a timecoder feed);
  the one-shot stop and dropout guard are hardware-validation items.

## Hardware validation checklist (Numark Scratch + timecode)

1. Record spinning, press CUE on keyboard/controller → deck jumps to cue,
   keeps playing, **no stutter/bounce**.
2. Needle up: hold CUE → preview plays for longer than 0.3 s and stops on
   release. Same for hotcue previews and the play button.
3. Needle-drop cueing "nearest hotcue": drop the needle → jumps to hotcue;
   scratch over a worn spot → playback does NOT jump back to the hotcue.
4. Pitch fader usable while the record is stopped.
5. Mode/enable/cueing announcements speak on Ctrl+T / Ctrl+Shift+Y /
   Ctrl+Alt+Y, and "vinyl constant mode" speaks when the needle reaches the
   record end zone.

## Open ideas (not done)

- Numark Scratch shift-layer buttons for vinyl enable/mode/cueing toggles.
- Announce timecode signal quality on demand (e.g. a `tts_vinyl` readout).
- `vinylcontrol_status` record-end *warning* earcon (status CO blinks at
  record end, needs debouncing — mode→constant announcement covers most of
  it already).

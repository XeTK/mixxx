# Task 07 — Feedback batch 2 (bugs, regressions, and requests)

Field notes from the blind tester, 2026-07-05. Status as of 2026-07-06:
7A, 7B, 7C, 7D, 7J, 7K, 7L, and 7M are implemented, tested, and
documented (see [ACCESSIBILITY_ROADMAP.md](../ACCESSIBILITY_ROADMAP.md)
and [ACCESSIBILITY_GUIDE.md](../ACCESSIBILITY_GUIDE.md)). 7E, 7F, 7G,
7H, and 7I remain open — this brief now covers only that remaining
work. Each announcement item follows the established pattern
(ControlProxy observer in `announcementmanager.cpp`, a settings gate, a
`tr()` string, a unit test).

## Done

### 7A. Deck volume fraction is wrong (half reads "a quarter") — FIXED

Root cause was confirmed: `[ChannelN],volume`, `pregain`, and
`[Master],gain`/`headGain` are `ControlAudioTaperPot`s, whose `get()`
returns linear gain rather than fader position. Readouts now use
`ControlProxy::getParameter()`. Regression tests
(`VolumeChange_TaperedControl_HalfFaderIsAHalf`,
`MainVolume_TaperedControl_CenterIsCenter`) exercise a real
`ControlAudioTaperPot` rather than a plain `ControlObject`.

### 7B. Beat click inaudible; needs options — FIXED

Added a "Beat click volume" slider in Preferences > Accessibility
(mirrors the ducking slider). Default level raised 0.5 → 0.75 and
decay lengthened 8 ms → 12 ms for a more perceptible transient.

### 7C. Percentage vs fractions choice — FIXED

New `MixerReadoutStyle` setting ("Speak mixer values as" combo:
Fractions/Percentages) in Preferences > Accessibility, threaded through
`fractionText`/`centerSplitText` and all mixer/taper readouts.

### 7D. Headphone mix (cue vs main) — FIXED

`[Master],headMix` now speaks "Headphone mix cue/main `<fraction>`" or
"Headphone mix even" under `AnnounceMixer`, debounced.

### 7J. Re-announce loaded track via a numbered hotkey — CLARIFIED & FIXED

User confirmed "Both" (per-deck track re-announce, and the existing
last-spoken repeat). Added `[ChannelN],tts_track`
(Alt+Shift+T / Alt+Shift+Y) to speak the loaded track's artist/title on
demand; `Alt+Shift+R` already covers repeating the last announcement
(including library selections), so no separate control was needed for
that half.

### 7K. Restart-to-start as sound or speech — FIXED

Back-to-start now routes through `emitCue()` with
`EngineEarcon::Id::Restart` and its own `FeedbackModeRestart` combo,
next to the other Playback Announcements.

### 7L. Exit-loop announcement — CLARIFIED & FIXED

User confirmed: "I don't hear the exit loop notification." Rather than
chase which of `loop_exit`/`reloop_toggle`/`reloop_andstop` was silent
in the field (all of them funnel through `setLoopingEnabled()`, which
the existing `loop_enabled` observer already listens to), loop on/off
now also fires a dedicated earcon (`EngineEarcon::Id::LoopOn/LoopOff`)
with its own `FeedbackModeLoop` combo, giving an instant, hard-to-miss
confirmation regardless of the exact live-only cause.

### 7M. "Master needs TTS" — CLARIFIED & FIXED

User meant a clipping/peak warning, and picked the earcon option
("number 3 sounds like a cool idea") over plain speech. Added
`AnnounceClipping` (on by default) observing `[Main],peak_indicator`,
throttled to one warning per 5 seconds, with its own feedback-mode
combo and a center-panned earcon (whole-mix event, not deck-panned).

## Still open

### 7E. Secondary deck modes: beat jump and beat loop

Announce beat-jump size/moves (`[ChannelN],beatjump_size`,
`beatjump_forward`/`_backward`) and beat-loop activations
(`beatloop_N_toggle`, `beatlooproll_N_activate`). New AnnounceLoop-ish
gate or reuse AnnounceLoop. Not started.

### 7F. Effect unit on/off

Announce effect-unit and per-effect enable toggles
(`[EffectRack1_EffectUnitN],group_[ChannelI]_enable` and
`[EffectRack1_EffectUnitN_EffectM],enabled`). New AnnounceEffect gate.
Not started.

### 7G. Effect selected

When an effect is loaded into a slot, announce its name. The loaded
effect exposes metadata; find the control/signal that fires on effect
load (EffectSlot / EffectChain). Likely needs a signal from the effects
system rather than a plain CO. Not started.

### 7H. Effect type when the filter is changed

When the QuickEffect (filter) super-knob's underlying effect is
switched, announce the new effect's name. Related to 7G; the QuickEffect
chain's loaded effect name. Not started.

### 7I. Announce the crate/playlist UI — CLARIFIED, NOT YET BUILT

User confirmed both: (1) the new/rename text-entry dialogs for
playlists and crates, and (2) announcing when a feature view (e.g. the
Playlists or Crates pane) is entered. Neither is implemented yet.
Likely touches the sidebar feature view classes and the
new/rename `QInputDialog` call sites in the library UI — needs a survey
of where those live before wiring in `announcementmanager.cpp`-style
observers or direct `speak()` calls at the dialog/view level.

## Suggested build order for what remains

1. 7I (dialogs + view-entry) — concrete, scoped, no more clarification
   needed.
2. 7E (beat jump/beat loop) — same CO-observer pattern already used
   throughout, just needs the beatjump/beatloop control names surveyed.
3. 7F/7G/7H (effects) as a group — these share the same research need
   (finding the right EffectSlot/EffectChain signal) so are cheapest to
   do together.

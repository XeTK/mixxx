# Task 07 — Feedback batch 2 (bugs, regressions, and requests)

Field notes from the blind tester, 2026-07-05. Status as of 2026-07-12:
**everything in this brief is done.** 7A, 7B, 7C, 7D, 7J, 7K, 7L, and
7M landed 2026-07-06; 7E (beat jump), 7F/7G/7H (effects), and 7I
(rename/duplicate dialogs + Playlists/Crates view entry; create/delete
had landed 2026-07-08) landed 2026-07-12. See
[ACCESSIBILITY_ROADMAP.md](../ACCESSIBILITY_ROADMAP.md) and
[ACCESSIBILITY_GUIDE.md](../ACCESSIBILITY_GUIDE.md).

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

### 7E. Beat jump — DONE (2026-07-12)

`beatjump_size` changes and `beatjump_forward`/`_backward` presses are
announced with the size in beats, gated by AnnounceLoop, debounced.
Beat-loop activations were already covered by the existing
`loop_enabled` observer.

### 7F/7G/7H. Effects — DONE (2026-07-12)

Per-effect enables (`[EffectRack1_EffectUnitN_EffectM],enabled`), unit
routing (`group_[ChannelI]_enable`), effect selection (`loaded_effect`,
debounced), and the deck filter's QuickEffect chain preset
(`loaded_chain_preset`) all speak, under a new AnnounceEffects setting
(on by default). Real effect/preset names come from resolvers injected
in `coreservices.cpp` (`AnnouncementManager::setEffectNameResolvers`),
so the announcement manager stays decoupled from the effects headers;
tests inject fakes and everything falls back to numeric descriptions
without a resolver.

### 7I. Crate/playlist UI — DONE (create/delete 2026-07-08, the rest 2026-07-12)

Create, delete, rename, and duplicate dialogs for playlists and crates
all announce the dialog, echo back the entered text, speak validation
failures, and confirm success. Entering the Playlists or Crates pane
speaks "Playlists view"/"Crates view" (`Library::slotSwitchToView`).
Bonus: reordering tracks (Alt+Up/Down/PageUp/PageDown/Home/End — an
existing upstream feature) now confirms "Moved to position N of M".

# Task 07 — Feedback batch 2 (bugs, regressions, and requests)

Field notes from the blind tester, 2026-07-05. Grouped by type. Some
items need a quick clarification with the user before building — marked
**CLARIFY**. Each announcement item follows the established pattern
(ControlProxy observer in `announcementmanager.cpp`, a settings gate, a
`tr()` string, a unit test).

## Bugs / regressions

### 7A. Deck volume fraction is wrong (half reads "a quarter") — ROOT CAUSE FOUND

`[ChannelN],volume` is a `ControlAudioTaperPot` (see
`EngineMixer` ctor: `ConfigKey(group, "volume"), -20, 0, 1`). Its
`get()` returns the *linear gain* (0..1 on a dB taper), so the physical
half-way fader position is ~0.25 gain — which `fractionText(get())`
speaks as "a quarter". Fix: read the **parameter** (0..1 fader
position) via `ControlProxy::getParameter()`, not `get()`, for tapered
controls. This same bug affects the announcements added in bug-batch 3
for **trim/pregain** and **main/headphone volume** — all
`ControlAudioTaperPot`s. EQ knobs and the crossfader are linear
potmeters and are correct as-is. Add a test that a taper control at
parameter 0.5 announces "a half".

### 7B. Beat click inaudible; needs options

Tester can't hear the metronome. Investigate: default `[BeatClick],volume`
is 0.5 but there is no UI to change it, and clicks only sound when a deck
is playing with a valid beat grid. Add a prefs slider for
`[BeatClick],volume` (mirror the ducking slider), and consider exposing
click pitch/level presets. Verify audibility over a loud track; the
880/1760 Hz sines may need more level or a sharper transient.

## Preferences

### 7C. Percentage vs fractions choice

Bug-batch 3 replaced percentages with fractions; the tester wants it
configurable. Add a `MixerReadoutStyle` setting (0 = fractions,
1 = percent) and branch in `fractionText`/`centerSplitText` (and the
new taper-parameter readouts from 7A). Prefs combo or checkbox.

## Announcements to add

### 7D. Headphone mix (cue vs main) — `[Master],headMix`

Not announced. -1 = full cue, +1 = full main, 0 = even. Speak as a
fraction/center-split under AnnounceMixer, debounced.

### 7E. Secondary deck modes: beat jump and beat loop

Announce beat-jump size/moves (`[ChannelN],beatjump_size`,
`beatjump_forward`/`_backward`) and beat-loop activations
(`beatloop_N_toggle`, `beatlooproll_N_activate`). New AnnounceLoop-ish
gate or reuse AnnounceLoop.

### 7F. Effect unit on/off

Announce effect-unit and per-effect enable toggles
(`[EffectRack1_EffectUnitN],group_[ChannelI]_enable` and
`[EffectRack1_EffectUnitN_EffectM],enabled`). New AnnounceEffect gate.

### 7G. Effect selected

When an effect is loaded into a slot, announce its name. The loaded
effect exposes metadata; find the control/signal that fires on effect
load (EffectSlot / EffectChain). Likely needs a signal from the effects
system rather than a plain CO.

### 7H. Effect type when the filter is changed

When the QuickEffect (filter) super-knob's underlying effect is
switched, announce the new effect's name. Related to 7G; the QuickEffect
chain's loaded effect name.

### 7I. Announce the crate/playlist UI — CLARIFY

"Announce the UI for crates and playlists." Likely the create/rename
text-entry dialogs (new playlist, new crate) and/or the context when
entering those feature views. Confirm exactly which interactions feel
silent before building.

### 7J. Re-announce loaded track via a numbered hotkey — CLARIFY

"Re-announce song names, buttons like the numbered hints." Best guess: a
per-deck hotkey (e.g. `[ChannelN],tts_track`) that speaks the loaded
track's artist/title on demand, matching the Alt+1..0 info-hotkey
family. Confirm whether they want per-deck track re-announce, or a
re-announce of the last library selection.

### 7K. Restart-to-start as sound or speech

The "back to start" event (Start / cue-goto-and-stop) is currently
speech only. Give it an earcon and a per-event feedback mode like the
other transport cues (extend EngineEarcon::Id + emitCue + a
FeedbackModeRestart setting and combo).

### 7L. Exit-loop announcement — CLARIFY

Loop on/off is already announced ("loop off"). Confirm what is missing —
possibly `reloop_toggle`, `loop_out`, or beat-loop-roll release
(momentary) not covered by `loop_enabled`.

### 7M. "Master needs TTS" — CLARIFY

Ambiguous. Bug-batch 3 added a `[Master],gain` (main volume)
announcement — confirm whether that is not firing, or whether "master"
means the booth output, the master VU/limiter, or focusing the master
section. Get a concrete repro.

## Suggested build order

1. 7A (regression, root cause known, small) and 7B (tester can't hear a
   shipped feature) first.
2. 7C (percentage option) — small, unblocks the tester's preference.
3. Clarify 7I/7J/7L/7M, then batch the announcement additions
   (7D–7H, 7K) following the standard pattern.

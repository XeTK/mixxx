# Task 06 — Field-test intake and tuning pass

## Goal

Process the blind tester's next round of feedback. This file lists the
knobs that were built expecting by-ear adjustment, so tuning requests
map to one-line changes instead of investigations.

## How feedback has arrived before

Short bullet lists mixing bugs, wording complaints, and feature ideas
(see git history: commits 06d2217833, 5df59fdfde, 54012b7699,
d491ba8543 all came from such lists). Triage each item into: broken /
wording / tuning / new feature, and confirm ambiguous items with the
user before building.

## Pre-built tuning points

- Beat click (`src/engine/enginebeatclick.cpp`, constants at top):
  beat/bar frequencies (880/1760 Hz), click length (40 ms), decay
  (8 ms), and `[BeatClick],volume` default (0.5).
- Fraction granularity (`src/util/announcementmanager.cpp`,
  `fractionText`): snapped to sixteenths; coarsen to eighths by
  changing the rounding if "5 sixteenths" proves too chatty.
- Debounce/throttle: `kControlDebounceMs` (400), `kMovingThrottleMs`
  (300), selection/search debounce constants — same file, top.
- Concise mode phrasing: the `mixerDeckName` helper and the concise
  branches in the format functions.
- Hotcue suppression window after track load: `kHotcueSuppressMs`.
- Speech wording: all strings are `tr()` literals in
  announcementmanager.cpp — grep the spoken text to find the site.

## Known rough edges the tester may hit

- JAWS speaking over Mixxx's own speech (double narration in library):
  if reported, options are documenting JAWS keystroke muting, or a
  setting to suppress Mixxx's library announcements when a screen
  reader is detected (Qt: `QAccessible::isActive()`).
- Alt+number conflicts with JAWS/other AT hotkeys — rebind if reported.
- Bar/beat counter phase is wrong if playback starts off-grid (counts
  from play start, assumes 4/4).
- Sampler cued during per-deck split cue: headphone effects on it
  process twice per callback (known, documented).

## After changes

Update tests (every announcement has one), run the full accessibility
filter, update the three ACCESSIBILITY*.md docs, rebuild both targets
so `build\mixxx.exe` matches, and note results in the roadmap log.

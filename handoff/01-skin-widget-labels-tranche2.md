# Task 01 — Screen-reader labels for skin widgets (tranche 2)

**Status: blocked on tester feedback — confirm scope with the user
before starting.** The JAWS user's next session should identify which
skin controls they actually reach with Tab/JAWS navigation; label those
first instead of all ~hundreds of skin widgets.

## Goal

Give accessible names to the skin-level custom widgets (deck
play/cue/sync buttons, knobs, faders, spinboxes) so JAWS/NVDA/VoiceOver
announce them meaningfully instead of as anonymous controls.

## Context

Tranche 1 (done, commit 8e19ed6baf) named everything in the preferences
dialogs and the library: search field, sidebar tree, track table,
category tree, plus label/buddy associations on the Accessibility page.
The codebase previously contained zero `setAccessibleName` calls.

Skin widgets are custom-painted (WPushButton, WKnob, WSliderComposed,
etc., in `src/widget/`) and are instantiated by the skin system
(`src/skin/legacy/legacyskinparser.cpp`) from XML skin definitions
(`res/skins/LateNight/...`). They have group/control context at parse
time — that is the labeling opportunity.

## Suggested approach

1. In `LegacySkinParser` where widgets are constructed and bound to a
   ConfigKey, derive a human name from the connected control (e.g.
   `[Channel1],play` → "Deck 1 play") and call `setAccessibleName`.
   A mapping table for the common controls (play, cue, sync, pfl,
   volume, pregain, EQ, pitch, hotcues) covers most value.
2. Skin XML may also carry `<Tooltip>` text — usable as
   `setAccessibleDescription`.
3. Verify with JAWS or NVDA: Tab/arrow to deck controls and confirm
   sensible speech. Note that many skin widgets are not in the tab
   order at all; adding focus policies is a separate decision — ask the
   user before changing tab navigation behavior.

## Acceptance

- The controls the tester identified announce with meaningful names.
- No change to visual appearance or existing keyboard behavior.
- Documented in ACCESSIBILITY.md.

# Task 04 — Minimal / high-contrast skin for low-vision users

**Status: deferred by the user** ("strictly blind first, low vision
later"). Do not start without the user's go-ahead.

## Goal

A skin (or LateNight variant) optimized for partial sight: very high
contrast, large text and controls, no decorative chrome, no waveforms
by default, and a layout that mirrors the spoken/keyboard model (two
decks, mixer, library — nothing else).

## Notes for whoever picks this up

- Skins live in `res/skins/`; LateNight is the default and the most
  maintained. A reduced variant of LateNight (its skin.xml supports
  conditional sections) is far cheaper than a from-scratch skin.
- The QSS color scheme files control palette; a true high-contrast
  scheme (white-on-black, yellow accents, >= 7:1 contrast) is mostly a
  QSS effort.
- Font scaling: Mixxx has a skin `ScaleFactor` preference; verify text
  scales without clipping at 1.5x–2x.
- The user explicitly asked earlier for "controls missing labels
  (maybe a super accessible high contrast theme)" — pair this with
  handoff task 01 (accessible names) so screen reader and visuals
  improve together.
- Get real low-vision user requirements before designing; contrast
  needs vary enormously (some need dark-on-light, not light-on-dark).

# Mixxx Accessibility Fork — Progress & Roadmap

Status document for the accessibility fork, which makes Mixxx usable by a
fully blind DJ (primary end user runs JAWS). Last updated 2026-07-03.

See [ACCESSIBILITY.md](ACCESSIBILITY.md) for how the features work and how
to use them.

## Completed

### Core speech engine (the foundation)

- Text-to-speech synthesized off the audio thread (SAPI on Windows,
  Qt TextToSpeech 6.6+ elsewhere) and mixed into Mixxx's own engine
  output with sidechain ducking — announcements are heard over the
  music through the headphone cue or main output, and are never
  recorded or broadcast. (`src/util/ttsengine.cpp`,
  `src/engine/enginetts.cpp`, `src/util/announcementmanager.cpp`)
- Barge-in: a new announcement interrupts the one in progress.
- Speech falls back to the main output when no headphone bus is
  configured, so a fresh single-output install is never silent.
- Voice, speech rate, output routing (headphones vs. main), and music
  ducking strength are all configurable in Preferences > Accessibility,
  with a Test Speech button that applies the ducking slider live.

### Announcements (each independently toggleable in Preferences)

- "Mixxx ready" at startup
- Track selection while browsing (artist, title)
- Track load: deck letter, artist, title, BPM, musical key
- Play / stop / end of track
- Headphone cue on/off, with deck name
- Sync, key lock, and quantize toggles
- Loop on/off with size in beats
- Hotcues 1–8 set/cleared (suppressed briefly around track loads so
  seeding saved cues doesn't fire a burst)
- Recording started / stopped
- Pitch fader position after it stops moving (debounced)
- Volume faders, EQ knobs, crossfader (debounced; opt-in, off by
  default because they move constantly mid-mix)
- Library: sidebar item names during arrow-key navigation, pane focus
  changes (search bar / sidebar / track list), and search feedback
- "Speech on" confirmation when re-enabling TTS

### Keyboard control (all also controller-mappable)

- `Alt+Shift+A` — toggle speech on/off (also in Options menu)
- `Alt+1` / `Alt+2` — full deck status (playing/stopped, time
  remaining, BPM, pitch)
- `Alt+3` / `Alt+4` — time remaining
- `Alt+5` / `Alt+6` — BPM
- `Alt+7` / `Alt+8` — musical key (keylock-aware)
- `Alt+9` / `Alt+0` — bar and beat position (assumes 4/4)
- `Alt+Shift+R` — repeat last announcement
- Odd numbers = deck A, even = deck B. Bindings in all 12 shipped
  keyboard layouts.

### Screen reader (JAWS/NVDA/VoiceOver) support — first tranche

- Accessible names and label/buddy associations on the Accessibility
  preferences page, the preferences category tree, the library search
  field, the sidebar tree, and the track table. (The codebase
  previously contained zero accessible names.)

### Other accessibility features

- Per-deck `disable_preroll` control + "prevent jogging before track
  start" checkbox under Preferences > Decks — a jog wheel can't seek
  into pre-roll silence.

### Bugs fixed and cleanups along the way

- Reverted a broken commit that referenced non-existent UI widgets and
  couldn't compile; restored the working preferences page.
- Deleted ~2,800 lines of dead "enhanced" accessibility code that was
  never in the build, plus six documents that described it as shipped.
- Fixed the TTS toggle being momentary instead of latching (control
  type), dead deck-status key bindings (Shift+digit never matches),
  and the Options-menu TTS item doing nothing (menu is built before
  the engine; now bridged with signals like Record/Broadcast).
- Fixed "Decka"/"Loadeda" — a comma now forces the TTS engine to
  pronounce the deck letter separately.
- Reverted the `Mixxx-Accessibility` CMake project rename: it broke
  development-build resource lookup (empty resource path, no skin,
  "crash on load" when launching mixxx.exe without --resourcePath).
- All spoken strings are translatable (`tr()`); musical key names are
  the remaining exception.
- Qt TextToSpeech gated to Qt >= 6.6 in CMake; the preferences page
  shows a warning when the build has no speech backend.
- Merged upstream 2.6 (July 1 state), conflict-free.
- 99 unit tests cover the announcement manager and engine sink.

### Local build environment (this machine)

- App-local DLL deployment (81 DLLs next to mixxx.exe) so the exe runs
  by double-click with no PATH setup.
- Interim workaround for the debug-protobuf link bug: the release
  protobuf DLL is provided under the debug import name. A proper CMake
  fix is being worked on in a separate session; until it lands,
  anything protobuf-touching would crash without the shim.

## In progress

- **Screen reader labels, tranche 2** — accessible names for
  skin-level widgets (deck play/cue buttons, knobs, faders reachable
  by keyboard). Large: these are custom-painted widgets.

## To do

### Tier 1 — complete the core blind-DJ loop (current focus)

1. ~~Granular info hotkeys~~ (done)
2. Screen reader widget labels — tranche 2 (skin widgets)
3. **User-facing how-to guide** — every shortcut, every announcement,
   preferences setup, and the basic workflows (browse, load, cue, mix,
   record), written to be read with JAWS
4. **Playlist & crate support** — announce names with type and track
   counts during sidebar navigation, expand/collapse state, and
   add/remove-track confirmations

### Tier 2 — announcement enrichment

Complete as of 2026-07-03: cue-set confirmation, hotcue-pressed
feedback, loop-size changes, trim/pregain and effect-unit mix/super
knobs, announce-while-moving preference (throttled), pitch announces
the resulting BPM, fraction readouts with center-split for
EQ/filter/gains, filter and main/headphone volume announcements,
deck-naming and concise-mode preferences, crossfader lock (Alt+X),
end-of-track includes time left, cue preview says "Cue", back-to-start
announced.

### Tier 3 — bigger projects

- Metronome / beat-click aid to help beginners hear the grid when
  setting beatgrids
- Simplified setup wizard for first-run (sound hardware setup currently
  happens before the speech engine can talk; a screen reader is needed)
- Minimal / high-contrast skin (deferred: current focus is fully-blind
  users; low-vision support comes later)
- Linux and macOS support (the TTS engine already has a Qt backend;
  needs building, testing, and packaging on those platforms)

### Known issues / parked

- **Debug-protobuf link bug** — the build links debug
  `libprotobuf-lited.dll` into a release build; crashes anything
  touching track key data without the shim. Proper CMake fix in
  progress in a separate session; two FormatForLoad unit tests fail
  without the shim.
- **JAWS audio routing during performance** — JAWS speaks through the
  Windows default device, which may be the main (audience) output.
  Plan: document pinning JAWS to a specific sound card, and keep
  Mixxx's own speech on the headphone bus.
- Qt error dialogs and the first-run sound-hardware dialog are not
  routed through the built-in speech (screen reader territory for now).
- Musical key names are spoken in English regardless of locale.
- Bar/beat readout assumes 4/4 time.

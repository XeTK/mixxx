# Mixxx Accessibility Fork — Progress & Roadmap

Status document for the accessibility fork, which makes Mixxx usable by a
fully blind DJ (primary end user runs JAWS). Last updated 2026-07-04.

See [ACCESSIBILITY.md](ACCESSIBILITY.md) for how the features work and how
to use them, [ACCESSIBILITY_GUIDE.md](ACCESSIBILITY_GUIDE.md) for the full
user walkthrough, or
[ACCESSIBILITY_QUICK_REFERENCE.md](ACCESSIBILITY_QUICK_REFERENCE.md) for a
one-page shortcut cheat sheet.

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
- Full key list, including the batch-2 additions (`Alt+B`, `Alt+H`,
  `Alt+X`, `Alt+Shift+T`/`Y`), now lives in
  [ACCESSIBILITY_QUICK_REFERENCE.md](ACCESSIBILITY_QUICK_REFERENCE.md)
  (added 2026-07-06).

### Screen reader (JAWS/NVDA/VoiceOver) support — first tranche

- Accessible names and label/buddy associations on the Accessibility
  preferences page, the preferences category tree, the library search
  field, the sidebar tree, and the track table. (The codebase
  previously contained zero accessible names.)

### Performance monitoring aids (2026-07-04)

- **Beat-click metronome** (`Alt+B`, `[BeatClick],enabled`): a click on
  every beat of each playing deck, deck 1 in the left ear and deck 2 in
  the right, with every fourth beat accented an octave up as a bar
  marker (assumes 4/4, phase counted from play start). Sample-accurate
  against the beat grid; headphone bus with main fallback; never
  recorded or broadcast. Level via the persistent `[BeatClick],volume`.
- **Per-deck split headphone cue** (`Alt+H`, `[Master],headSplitDecks`):
  deck 1's PFL as a mono fold-down in the left ear, deck 2's in the
  right; other cued sources in both ears. Overrides the classic
  cue/main split while active; the head/main mix knob still blends the
  main output on top. Both toggles speak their state.

### Other accessibility features

- Per-deck `disable_preroll` control + "prevent jogging before track
  start" checkbox under Preferences > Decks — a jog wheel can't seek
  into pre-roll silence.
- **Jog wheel touch lock** (2026-07-07, `Alt+J`,
  `[Master],disable_touch_scratch`): ignores click-and-drag scratching
  on the on-screen waveform and vinyl widgets on both decks, so an
  accidental touch (mouse or touchscreen) can't disturb playback. Works
  live at any time, including mid-playback, unlike `disable_preroll`
  which is a start-up preference only. Checkbox under Preferences >
  Decks sets the default state; both states are spoken.
- **Musical key spoken in other notations** (2026-07-07): key
  announcements (track load and the `Alt+7`/`Alt+8` on-demand readout)
  now follow whatever `[Library],key_notation` is set to elsewhere in
  Mixxx (Preferences > Interface > Key Notation) instead of always
  speaking the full traditional name. Open Key ("5d") and Camelot/
  Lancelot ("8A") short codes are spoken as digit + phonetically
  spelled letter ("5, Dee" / "8, Ay") via `KeyUtils::keyToString()`;
  the "…and Traditional" variants append the full name too. Traditional,
  Custom, and ID3v2 notations are unaffected — they already spoke the
  full name.

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
- Fixed deck letter "A" being spoken as the indefinite article ("uh")
  instead of the letter name (2026-07-07). An earlier attempt used an
  SSML `<say-as interpret-as="characters">` tag, but the TTS pipeline
  never renders SSML, so it leaked into speech as literal text
  ("Deck, A,. No track loaded" was actually being read aloud). Replaced
  with a `phoneticLetter()` lookup table in `announcementmanager.cpp`
  that spells every deck letter out (A→"Ay", B→"Bee", … Z→"Zee"),
  which is robust across TTS engines without needing SSML support.
- Reverted the `Mixxx-Accessibility` CMake project rename: it broke
  development-build resource lookup (empty resource path, no skin,
  "crash on load" when launching mixxx.exe without --resourcePath).
- All spoken strings are translatable (`tr()`); musical key names are
  the remaining exception.
- Qt TextToSpeech gated to Qt >= 6.6 in CMake; the preferences page
  shows a warning when the build has no speech backend.
- Merged upstream 2.6 (July 1 state), conflict-free.
- Repaired a pre-existing mangling in the 11 non-English keyboard
  layouts: the `[Tts]` section had been inserted inside `[Microphone]`
  as a stray `n[Tts]` line, so the speech toggle and repeat shortcuts
  never worked outside the US layout.
- 125 unit tests cover the announcement manager, engine speech sink,
  and beat-click metronome.

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

### Feedback batch 2 (2026-07-05, mostly complete) — see [handoff/07-feedback-batch-2.md](handoff/07-feedback-batch-2.md)

Done:

- **Deck volume fraction bug fixed** — `volume`, `pregain`, and
  `[Master],gain`/`headGain` are `ControlAudioTaperPot`s; readouts now
  use `getParameter()` (fader position) instead of `get()` (dB-tapered
  gain), so a physical half-way fader correctly announces "a half".
  Regression tests use a real `ControlAudioTaperPot` to exercise the
  taper, not a plain `ControlObject`.
- **Beat click audibility** — new "Beat click volume" slider in
  Preferences > Accessibility; default level raised (0.5 → 0.75) and
  the click decay lengthened slightly (8 ms → 12 ms) for better cut-through.
- **Percentage vs. fractions** — new `MixerReadoutStyle` setting
  ("Speak mixer values as" combo) switches every fader/knob readout
  between fractions and exact percentages.
- **Headphone mix announcement** — `[Master],headMix` speaks
  "Headphone mix cue/main `<fraction>`" or "Headphone mix even", under
  `AnnounceMixer`.
- **Track re-announce hotkey** — `[ChannelN],tts_track`
  (Alt+Shift+T / Alt+Shift+Y) re-speaks the loaded track's artist/title
  on demand. `Alt+Shift+R` (repeat) already re-announces whatever was
  last spoken, including a library selection, so no separate control
  was added for that half of the request.
- **Back-to-start and loop on/off as sound or speech** — both now route
  through the earcon system (`EngineEarcon::Id::Restart/LoopOn/LoopOff`)
  with their own per-event feedback-mode combos, giving loop toggles an
  instant, hard-to-miss confirmation regardless of the earlier
  "I don't hear the exit loop notification" report's exact cause.
- **Master clipping/peak warning** ("sounds like a cool idea") — new
  `AnnounceClipping` setting (on by default) plus its own feedback-mode
  combo and a center-panned earcon; observes `[Main],peak_indicator`,
  throttled to one warning per 5 seconds during sustained clipping.

166 accessibility tests pass (up from 144).

Done since (2026-07-08):

- **Quick add to crate/playlist** (`Alt+Shift+C` / `Alt+Shift+P`,
  `[Library],AddToCrate`/`AddToPlaylist`) — the first slice of the
  playlist/crate accessibility pass below. With a track selected,
  opens a plain menu of crate/playlist names next to it; each name is
  spoken with its position (`Library::quickPickerItemHighlighted`,
  unconditional — the picker was opened on purpose) as you arrow
  through, Enter adds via the existing `PlaylistDAO`/`TrackCollection`
  calls (which already fire the "Added to crate/playlist X"
  confirmation, so no new wiring was needed there). Speaks a reminder
  instead of an empty menu if you have none yet. Does not yet cover
  "Create New" from inside the picker — use the existing
  `Ctrl+N`/`Ctrl+Shift+N` shortcuts first.
- **New Playlist / New Crate dialog announcements** — pressing
  `Ctrl+N`/`Ctrl+Shift+N` now speaks that the dialog opened and that
  its text box already has a name pre-filled and selected (the exact
  proposed name for crates, since `CrateFeatureHelper` may suffix it
  " 2", " 3", etc. if the plain name is taken). Threaded a `Library*`
  into `CrateFeatureHelper` (optional, nullptr-safe) to reach
  `Library::announceText()`; `BasePlaylistFeature` already had
  `m_pLibrary`. Rename/duplicate dialogs and the validation-failure
  message boxes are unchanged (screen-reader-only, as before) — only
  the two "create new" dialogs got the new announcement.
- **Delete Playlist / Delete Crate dialog announcements** — deleting a
  playlist or crate from its right-click menu speaks what's about to
  be deleted and that No is the default (safe) button before the
  `QMessageBox::question` confirmation opens, then "Deleted
  playlist/crate X" once `deletePlaylist()`/`deleteCrate()` succeeds.
  Same `Library::announceText()` mechanism; no new signal needed.
- **Entered-text echo on the create dialogs** — after pressing Enter in
  the New Playlist/New Crate dialog, Mixxx reads back exactly what was
  typed ("You entered: Warmup") before running the existing duplicate-
  name/blank-name validation, so a typo is caught before it becomes
  the name rather than only visually. Re-announces on every retry
  through the validation loop, not just the first attempt.
- **Duplicate/blank-name validation errors spoken** — if the entered
  name is already taken or blank, Mixxx now speaks the same message
  the `QMessageBox::warning()` shows ("A playlist/crate by that name
  already exists" / "cannot have a blank name") instead of leaving it
  to the screen reader to notice the dialog. The create loop then
  reopens the input dialog as before.

Still open (deferred — see brief 07 for detail):

- Beat jump and beat loop (secondary deck modes) announcements
- Effect unit on/off; effect selected; effect type when the filter
  changes
- Announcing playlist/crate feature-view entry (switching into the
  Playlists or Crates sidebar view) — create and delete dialogs are
  now done (above); rename/duplicate dialogs and view-entry remain
- **Playlist/crate accessibility pass (2026-07-07, in progress)** —
  broader than the announcement work above: audit whether every
  playlist/crate action (create, rename, delete, reorder, remove
  tracks, drag-and-drop equivalents) actually has a keyboard path and
  is operable with the mouse too, not just whether it's announced.
  Quick add (above) is the first slice; rename/delete/reorder and the
  context-menu-vs-keyboard gap generally are still unaudited.

### Tier 1 — complete the core blind-DJ loop

1. ~~Granular info hotkeys~~ (done)
2. Screen reader widget labels — tranche 2 (skin widgets). Waiting on
   JAWS field feedback to scope which controls matter.
3. ~~User-facing how-to guide~~ (done 2026-07-04:
   [ACCESSIBILITY_GUIDE.md](ACCESSIBILITY_GUIDE.md), including the
   JAWS/NVDA audio-device pinning recipe)
4. ~~Playlist & crate support~~ (done 2026-07-04: sidebar position
   "3 of 12", expand/collapse state with child counts, and spoken
   add/remove confirmations for playlists and crates)

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

- ~~Metronome / beat-click aid~~ (done 2026-07-04, with per-deck
  stereo split; see Performance monitoring aids above)
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

# Mixxx Accessibility Fork

This fork adds spoken (text-to-speech) feedback to Mixxx so that a fully
blind DJ can browse the library, load tracks, and perform without a screen
reader having to interpret the custom-painted skin.

**New here? Read [ACCESSIBILITY_GUIDE.md](ACCESSIBILITY_GUIDE.md)** — the
user-facing walkthrough of every shortcut, announcement, and workflow,
written to be read with a screen reader. Already know your way around
and just need the keys? See
[ACCESSIBILITY_QUICK_REFERENCE.md](ACCESSIBILITY_QUICK_REFERENCE.md), a
one-page cheat sheet. This file is the technical overview;
[ACCESSIBILITY_ROADMAP.md](ACCESSIBILITY_ROADMAP.md) tracks progress.

## How it works

Speech is synthesized off the audio thread (SAPI on Windows,
AVSpeechSynthesizer on macOS, Qt TextToSpeech on other platforms when
built with Qt >= 6.6) and mixed into Mixxx's own engine output with
sidechain ducking, so announcements are heard over the music through
the headphone cue or main output like any other engine signal. Speech
is never recorded or broadcast.

macOS uses AVSpeechSynthesizer directly rather than Qt's TextToSpeech
module: Mixxx's macOS dependency bundle doesn't ship that Qt module,
so relying on it would leave TTS silently disabled on a real Mac.
Talking to Apple's framework directly also means no extra dependency
to bundle, matching how SAPI is used on Windows.

Key source files:

- `src/util/ttsengine.cpp` — platform speech synthesis dispatch;
  `src/util/ttsenginemac.mm` — the macOS AVSpeechSynthesizer backend
- `src/engine/enginetts.cpp` — lock-free FIFO + ducking mix-in
  (`[Tts]` control group)
- `src/util/announcementmanager.cpp` — decides what to say and when
- `src/preferences/dialog/dlgprefaccessibility.cpp` — the
  Preferences > Accessibility page, including the macOS-only voice
  quality tier filter

## Using it

- **Toggle speech**: Options > Enable Text-to-Speech, or `Alt+Shift+A`
  ("Speech on" is spoken as confirmation when enabling).
- **Deck status on demand**: `Alt+1` / `Alt+2` speak the deck's state —
  playing or stopped, time remaining, BPM, and pitch.
- **Single facts on demand** (odd numbers = deck A, even = deck B):
  - `Alt+3` / `Alt+4` — time remaining
  - `Alt+5` / `Alt+6` — BPM
  - `Alt+7` / `Alt+8` — musical key (keylock-aware)
  - `Alt+9` / `Alt+0` — bar and beat position (assumes 4/4)
- **Repeat last announcement**: `Alt+Shift+R`.
- **Crossfader lock**: `Alt+X` freezes the crossfader at its current
  position so an accidental bump does nothing; press again to unlock.
  Both states are confirmed audibly.
- **Jog wheel touch lock**: `Alt+J` makes click-and-drag scratching on
  the on-screen waveform and vinyl widgets a no-op on both decks, so
  an accidental touch (mouse or touchscreen) can't disturb playback.
  Works at any time, including mid-playback; press again to unlock.
  Both states are confirmed audibly.
- **Per-deck split cue**: `Alt+S` puts deck 1's headphone cue in the
  left ear and deck 2's in the right (each as a mono fold-down), so
  both decks can be monitored at once. Other cued sources (samplers,
  preview deck) stay in both ears. Overrides the classic split-cue
  option while active; the head/main mix knob still blends the main
  output on top of both ears.
- **Beat click metronome**: `Alt+B` toggles a click on every beat of
  each playing deck — deck 1 in the left ear, deck 2 in the right, so
  both grids can be followed at once. Every fourth beat is a higher
  pitched bar marker (assumes 4/4, counted from where playback
  started). Clicks (and the earcon sound cues) follow the "Speech
  output" setting — headphones by default, falling back to main when
  no headphone output is configured, or main when speech is routed
  there — and are never recorded or broadcast. Level is the
  `[BeatClick],volume` control.
- **Preferences > Accessibility**: choose the voice, speech rate, output
  routing (headphone cue vs. main), and which announcements are spoken.
  A Test button speaks a sample through the current routing.
- All of the above are plain Mixxx controls (`[Tts],enabled`,
  `[Tts],repeat`, `[ChannelN],tts_status`), so they can also be mapped
  to controller buttons.
- **Built-in controller layers**: the shipped Numark Scratch mapping
  carries an accessibility layer on its Shift button (deck status,
  repeat, beat click, split cue), and the DDJ-400 mapping has an
  opt-in setting that turns the Hot Cue pads into accessibility pads.
  See the "Controller mapping" section of
  [ACCESSIBILITY_GUIDE.md](ACCESSIBILITY_GUIDE.md) for the layouts.

## What is announced

- "Mixxx ready" once the interface has loaded
- Track selection while browsing the library (artist, title)
- Track load: deck letter, artist, title, BPM, musical key (in the
  Traditional, Open Key, or Camelot/Lancelot notation, matching whatever
  Preferences > Interface > Key Notation is set to elsewhere in Mixxx)
- Play / stop / end of track
- Headphone cue (PFL) toggling
- Sync, key lock, and quantize toggles
- Loop on/off, including the loop size in beats
- Hotcues being set, cleared, or pressed; the main cue point being set
- Loop size changes
- Pitch-fader position after it stops moving
- Trim knobs and effect-unit mix/super knobs
- Recording started / stopped
- Optionally, continuous controls can announce *while* they move
  (throttled) instead of only at rest
- Volume faders, EQ knobs, the crossfader, and the headphone mix
  (cue vs. main) knob (opt-in — off by default because these move
  constantly during a mix); spoken as fractions or, if preferred,
  percentages (`MixerReadoutStyle` setting)
- Audio clipping on the main output (on by default; throttled)
- Sidebar item names while navigating with arrow keys
- Library pane focus changes (search bar / sidebar / track list)
- Search feedback ("Searching: …" / "Search cleared")
- On-demand track re-announce per deck (`tts_track`), for re-hearing
  a loaded track's name mid-set
- Quick add-to-crate / add-to-playlist (`Alt+Shift+C` / `Alt+Shift+P`):
  a small menu of your crate or playlist names pops up next to the
  selected track, each name spoken (with its position) as you arrow
  through it, `Enter` to add
- The New Playlist / New Crate dialogs (`Ctrl+N` / `Ctrl+Shift+N`):
  Mixxx speaks that the dialog opened and that its text box already has
  a name filled in and selected, ready to type over; after you press
  Enter it reads back exactly what you typed, then speaks a "that name
  already exists" or "cannot have a blank name" error and reopens the
  dialog if the name isn't valid
- Delete Playlist / Delete Crate confirmation dialogs: speaks what's
  about to be deleted and that No is the default (safe) button, plus a
  final "Deleted playlist/crate X" once it's gone
- Vinyl control (DVS) state per deck: enabled/disabled, the mode
  (absolute / relative / constant — including automatic mode changes,
  such as dropping to relative on a seek or to constant at the record
  end), and the needle-drop cueing mode (always on, no preference
  gate — you need to hear why a deck stopped following the turntable)

Each category can be toggled independently in Preferences >
Accessibility.

Play, stop, end of track, headphone cue, back-to-start, and loop on/off
can alternatively (or additionally) be signalled with short percussive
**earcons** instead of speech, chosen independently per event (with an
"All transport feedback" preset covering the first four). Earcons are
deck-panned (deck 1 left, deck 2 right; clipping is centered),
synthesized in `src/engine/engineearcon.cpp` and mixed into the
headphone bus like the beat click. Level is the `[Earcon],volume`
control.

## Other accessibility changes

- Per-deck `disable_preroll` control and a "prevent jogging before track
  start" checkbox under Preferences > Decks, so a jog wheel cannot seek
  into pre-roll silence before the track begins. A "Pre-roll limit" spin
  box (per-deck `preroll_limit_beats` control) sets how much pre-roll is
  still allowed before the clamp: default 4 beats, 0 clamps hard at the
  track start, and 1 second is used when the track has no BPM. This is
  also the same clamp applied to timecode-vinyl lead-in seeks when the
  option is on. (Also proposed upstream as PR #16573.)
- `[Master],disable_touch_scratch` control and a "disable jog wheel and
  waveform touch scratching" checkbox under Preferences > Decks, backing
  the `Alt+J` jog wheel touch lock described above.
- `Alt+Shift+A` TTS toggle added to all shipped keyboard layouts.
- Vinyl control (DVS) transport arbitration
  (`[ChannelN],vinylcontrol_transport_active`): while the timecode
  signal drives a deck, cue and hotcue presses jump without stopping
  (a software stop would fight the vinyl engine and bounce playback);
  with the needle up, play and cue/hotcue previews from keyboard or a
  controller work normally instead of being force-stopped after 0.3 s.
  Brief timecode dropouts no longer re-trigger needle-drop cue seeks.

## Known limitations

- Spoken strings are currently English only.
- First-run sound-hardware setup happens before the speech engine can
  produce audio; a screen reader (e.g. NVDA) is needed for initial setup.
- Qt dialogs (errors, file pickers) are not routed through the built-in
  speech; they remain the screen reader's job.

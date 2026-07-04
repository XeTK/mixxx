# Mixxx Accessibility Fork

This fork adds spoken (text-to-speech) feedback to Mixxx so that a fully
blind DJ can browse the library, load tracks, and perform without a screen
reader having to interpret the custom-painted skin.

## How it works

Speech is synthesized off the audio thread (SAPI on Windows, Qt
TextToSpeech on other platforms when built with Qt >= 6.6) and mixed into
Mixxx's own engine output with sidechain ducking, so announcements are
heard over the music through the headphone cue or main output like any
other engine signal. Speech is never recorded or broadcast.

Key source files:

- `src/util/ttsengine.cpp` — platform speech synthesis to PCM
- `src/engine/enginetts.cpp` — lock-free FIFO + ducking mix-in
  (`[Tts]` control group)
- `src/util/announcementmanager.cpp` — decides what to say and when
- `src/preferences/dialog/dlgprefaccessibility.cpp` — the
  Preferences > Accessibility page

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
- **Per-deck split cue**: `Alt+H` puts deck 1's headphone cue in the
  left ear and deck 2's in the right (each as a mono fold-down), so
  both decks can be monitored at once. Other cued sources (samplers,
  preview deck) stay in both ears. Overrides the classic split-cue
  option while active; the head/main mix knob still blends the main
  output on top of both ears.
- **Beat click metronome**: `Alt+B` toggles a click on every beat of
  each playing deck — deck 1 in the left ear, deck 2 in the right, so
  both grids can be followed at once. Every fourth beat is a higher
  pitched bar marker (assumes 4/4, counted from where playback
  started). Clicks go to the headphone output (or main if no
  headphones are configured) and are never recorded or broadcast.
  Level is the `[BeatClick],volume` control.
- **Preferences > Accessibility**: choose the voice, speech rate, output
  routing (headphone cue vs. main), and which announcements are spoken.
  A Test button speaks a sample through the current routing.
- All of the above are plain Mixxx controls (`[Tts],enabled`,
  `[Tts],repeat`, `[ChannelN],tts_status`), so they can also be mapped
  to controller buttons.

## What is announced

- "Mixxx ready" once the interface has loaded
- Track selection while browsing the library (artist, title)
- Track load: deck letter, artist, title, BPM, musical key
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
- Volume faders, EQ knobs, and the crossfader (opt-in — off by
  default because these move constantly during a mix)
- Sidebar item names while navigating with arrow keys
- Library pane focus changes (search bar / sidebar / track list)
- Search feedback ("Searching: …" / "Search cleared")

Each category can be toggled independently in Preferences >
Accessibility.

## Other accessibility changes

- Per-deck `disable_preroll` control and a "prevent jogging before track
  start" checkbox under Preferences > Decks, so a jog wheel cannot seek
  into pre-roll silence before the track begins.
- `Alt+Shift+A` TTS toggle added to all shipped keyboard layouts.

## Known limitations

- Spoken strings are currently English only.
- First-run sound-hardware setup happens before the speech engine can
  produce audio; a screen reader (e.g. NVDA) is needed for initial setup.
- Qt dialogs (errors, file pickers) are not routed through the built-in
  speech; they remain the screen reader's job.

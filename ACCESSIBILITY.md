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
- **Deck status on demand**: `Alt+Shift+1` / `Alt+Shift+2` speak the
  deck's state — playing or stopped, time remaining, BPM, and pitch.
- **Repeat last announcement**: `Alt+Shift+R`.
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
- Hotcues being set or cleared
- Pitch-fader position after it stops moving
- Recording started / stopped
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

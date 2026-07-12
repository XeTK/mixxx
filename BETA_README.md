# Mixxx Accessibility Beta — Read Me First

Thank you for testing this build. This is a fork of Mixxx 2.6 built from the
ground up so a fully blind DJ can browse music, load tracks, and perform
using a keyboard and spoken feedback — no screen-reader interpretation of
the on-screen skin required during a set.

This document is written to be read with a screen reader: headings mark
every section, and there are no images.

## Installing

1. Run the installer (a `.msi` file, e.g. `Mixxx-Accessibility-Setup.msi`).
   It is a standard Windows Installer package, so your screen reader reads
   its screens the same way as any other installer.
2. Step through the wizard: Next, accept the license, Next, Install,
   Finish. The defaults are fine.
3. Launch Mixxx from the Start Menu (search for "Mixxx") or the desktop
   shortcut, if you chose to create one.

### One-time first-run step

The very first time Mixxx starts, it needs you to choose your sound card
before its own speech can talk — this one screen happens before the
speech engine is running, so you'll need your regular screen reader (JAWS
or NVDA) for it. After that single dialog, Mixxx announces "Mixxx ready"
and everything else is spoken.

## Where to start

- **[ACCESSIBILITY_GUIDE.md](ACCESSIBILITY_GUIDE.md)** — the full
  walkthrough: quick start, every announcement, every shortcut, and how
  to use a screen reader alongside Mixxx. Start here.
- **[ACCESSIBILITY_QUICK_REFERENCE.md](ACCESSIBILITY_QUICK_REFERENCE.md)**
  — a one-page shortcut cheat sheet once you know your way around.
- Toggle speech at any time with `Alt+Shift+A`.

Both files are in the same folder as this one.

## What's in this build

- Spoken feedback for track browsing, loading, play/stop/cue, loops,
  hotcues, sync/key lock/quantize, recording, and the mixer (volume, EQ,
  filter, crossfader, headphone mix) — each independently toggleable in
  Preferences > Accessibility.
- Deck info on demand (`Alt+1`–`Alt+0`), track re-announce
  (`Alt+Shift+T`/`Y`), and repeat-last-announcement (`Alt+Shift+R`).
- Short percussive sounds ("earcons") as an alternative or addition to
  speech for play, stop, end of track, headphone cue, back-to-start,
  loop on/off, and audio clipping.
- A beat-click metronome (`Alt+B`) and per-deck split headphone cue
  (`Alt+H`) for monitoring both decks by ear.
- A jog wheel touch lock (`Alt+J`) so an accidental touch on the
  on-screen waveform or vinyl widget can't derail playback.
- Musical key spoken in whichever notation you use elsewhere in Mixxx —
  full names ("A Minor"), Open Key ("5, Dee"), or Camelot/Lancelot
  ("8, Ay").
- Quick add to crate/playlist (`Alt+Shift+C` / `Alt+Shift+P`): pops a
  menu of your crate/playlist names next to the selected track, each
  one spoken as you arrow through it, `Enter` to add.
- Spoken New Playlist / New Crate / Delete Playlist / Delete Crate
  dialogs (`Ctrl+N`, `Ctrl+Shift+N`, and their right-click Delete
  actions): announces the dialog opening, reads back what you typed,
  and speaks validation errors like a duplicate name.
- Accessible names for the preferences dialogs, library search box,
  sidebar, and track table, for use with JAWS/NVDA/VoiceOver.

See [ACCESSIBILITY.md](ACCESSIBILITY.md) for the full technical rundown.

## Known issues

- **JAWS may speak through the wrong sound card during a set.** JAWS
  uses the Windows default audio device, which may be your main
  (audience) output rather than your headphones. Pin JAWS to a
  headphone/laptop sound device in JAWS's own settings (Utilities >
  Sound Card selection in JAWS; Preferences > Settings > Audio > Output
  device in NVDA) — Mixxx's own speech is unaffected since it addresses
  your interface directly.
- **Windows error dialogs and the first-run sound-hardware dialog** are
  not spoken by Mixxx — they're your regular screen reader's job, as
  noted above.
- **Musical key names are spoken in English** regardless of your Windows
  language.
- **The bar/beat position readout (`Alt+9`/`Alt+0`) assumes 4/4 time.**

## Not built yet

Please don't be surprised if these don't do anything yet — they're on
the list for a future build:

- Beat jump and beat loop announcements (the secondary loop/jump modes)
- Effects: announcing an effect unit turning on/off, an effect being
  selected, or the effect changing when you switch the filter
- Announcing the crate/playlist rename/duplicate dialogs (create and
  delete are done, above), and announcing when you switch into the
  Playlists or Crates view
- The rest of the playlist/crate accessibility pass — reordering
  tracks and any remaining drag-and-drop-only actions may still be
  easier with a mouse than a keyboard. If you find one that's
  mouse-only, that's expected for now and exactly the kind of thing
  worth noting in your feedback
- More screen-reader labels on the custom-painted deck widgets (play/cue
  buttons, knobs, faders) — the preferences dialogs and library already
  have them
- A simplified first-run setup wizard (see the one-time step above)
- Linux builds (currently Windows only)
- A packaged, installable macOS build. The speech engine itself now
  works on macOS (native AVSpeechSynthesizer backend, not Qt's, since
  Mixxx's macOS dependency bundle doesn't ship the Qt TextToSpeech
  module) and is verified by automated tests, but there's no signed
  `.app`/installer yet for testers — for now it only runs from a
  source build. See `handoff/03-linux-macos.md` for the technical
  details if you're building it yourself.

## Giving feedback

However you'd like — a quick note of what you did, what you expected,
and what you actually heard (or didn't hear) is all that's needed. Exact
wording of an announcement, or "I pressed X and nothing happened," is
more useful than a general impression, but any note at all helps.

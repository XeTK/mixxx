# Mixxx Accessibility Quick Reference

A one-page cheat sheet of every keyboard shortcut this accessibility fork
adds. For the full walkthrough and what gets announced automatically, see
[ACCESSIBILITY_GUIDE.md](ACCESSIBILITY_GUIDE.md). All shortcuts below are
also plain Mixxx controls, so they can be remapped to a controller if you
prefer — see the Controller mapping section of that guide.

## Quick toggles

These four flip a feature on or off and always confirm both states out
loud, so you never have to guess what just happened:

- `Alt+Shift+A` — Speech on/off. Says "Speech on" when enabled (once
  speech is off, there is nothing left to say it).
- `Alt+B` — Beat click metronome on/off. Says "Beat click on" or
  "Beat click off".
- `Alt+S` — Per-deck split headphone cue on/off. Says "Split cue on.
  Deck 1 left, deck 2 right" or "Split cue off".
- `Alt+X` — Crossfader lock on/off. Says "Crossfader locked" or
  "Crossfader unlocked".
- `Alt+J` — Jog wheel touch lock on/off. Ignores click-and-drag
  scratching on the on-screen waveform and vinyl widgets on both
  decks, so an accidental touch can't disturb playback. Says
  "Jog wheel touch locked" or "Jog wheel touch unlocked".

## Deck info on demand

Odd numbers are deck 1 (A), even numbers are deck 2 (B):

- `Alt+1` / `Alt+2` — Full deck status: playing or stopped, time
  remaining, BPM, pitch.
- `Alt+3` / `Alt+4` — Time remaining.
- `Alt+5` / `Alt+6` — BPM.
- `Alt+7` / `Alt+8` — Musical key (keylock-aware).
- `Alt+9` / `Alt+0` — Bar and beat position (assumes 4/4 time).
- `Alt+Shift+T` / `Alt+Shift+Y` — Re-announce the loaded track's
  artist and title.

## Speech control

- `Alt+Shift+R` — Repeat the last announcement, whatever it was
  (deck info, a library selection, a toggle confirmation).

## Getting a track loaded

- `Shift+Left` / `Shift+Right` — Load the highlighted library track
  to deck 1 / deck 2, with an announcement of deck, artist, title,
  BPM, and key.

## Adding a track to a crate or playlist

With a track selected in the library:

- `Alt+Shift+C` — Add to crate. Opens a small menu of your crate names;
  arrow through it to hear each one (with its position, e.g. "House,
  2 of 5"), `Enter` to add, `Escape` to cancel.
- `Alt+Shift+P` — Add to playlist. Same as above, for playlists.

If you have none yet, it speaks a reminder instead of opening an empty
menu: `Ctrl+Shift+N` makes a new crate, `Ctrl+N` a new playlist.

## Reordering tracks in a playlist

With one or more tracks selected in a playlist:

- `Alt+Up` / `Alt+Down` — Move the selection one row up or down.
- `Alt+PageUp` / `Alt+PageDown` — Move it a screenful.
- `Alt+Home` / `Alt+End` — Move it to the very top or bottom.

Every move is confirmed out loud: "Moved to position 4 of 12".

## Where the rest lives

Everything else — per-event sound/speech choices, mixer announcements,
ducking strength, voice and rate — is a checkbox or combo box in
Options > Preferences > Accessibility, not a keyboard shortcut. See the
"Preferences reference" section of
[ACCESSIBILITY_GUIDE.md](ACCESSIBILITY_GUIDE.md) for the full list.

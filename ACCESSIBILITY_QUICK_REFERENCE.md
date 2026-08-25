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

## Auto DJ

- `Shift+F12` — Enable/disable Auto DJ. Says "Auto DJ on. Next:
  Artist, Title" (or just "Auto DJ on" if the queue is empty) and
  "Auto DJ off".
- `Shift+F11` — Fade to the next track now. Says "Fading now".
- `Shift+F10` — Skip the next queued track without playing it. Says
  "Skipped".
- `Shift+F9` — Shuffle the Auto DJ queue.
- `Ctrl+Shift+F9` — Add a random track to the Auto DJ queue.
- `Alt+Shift+N` — What's next: on/off state, the next queued track's
  artist and title, and roughly how long until the currently playing
  deck hands off (an estimate — the real crossfade can start earlier,
  at the outro point).

## Getting a track loaded

- `Shift+Left` / `Shift+Right` — Load the highlighted library track
  to deck 1 / deck 2, with an announcement of deck, artist, title,
  BPM, and key. If the deck is playing and loading is set to Reject,
  Mixxx says why nothing happened ("Deck 1 is playing, load
  blocked") instead of staying silent. With Smart cue on (the
  default, Preferences > Decks), loading into a stopped deck also
  moves the headphone cue there automatically.

## Fixing a wrong BPM analysis

Fast tracks (drum and bass, footwork) are often analysed at half
their real tempo. Deck 1 keys listed; add `Shift` for deck 2:

- `Ctrl+Alt+H` — Halve the deck's BPM grid ("Deck 1 B P M halved").
- `Ctrl+Alt+D` — Double it ("Deck 1 B P M doubled"). Check the
  result with `Alt+5` / `Alt+6`.

## Quantize

- `Ctrl+Alt+Q` — Quantize on/off for deck 1 (add `Shift` for
  deck 2). Spoken both ways; the state is remembered per deck across
  restarts.

## Adding a track to a crate or playlist

With a track selected in the library:

- `Alt+Shift+C` — Add to crate. Opens a small menu of your crate names;
  arrow through it to hear each one (with its position, e.g. "House,
  2 of 5"), `Enter` to add, `Escape` to cancel.
- `Alt+Shift+P` — Add to playlist. Same as above, for playlists.

For the track loaded in a deck (no library selection needed — file
what's playing mid-mix):

- `Ctrl+Alt+C` — Add deck 1's loaded track to a crate (add `Shift`
  for deck 2).
- `Ctrl+Alt+P` — Add deck 1's loaded track to a playlist (add
  `Shift` for deck 2).

If you have none yet, it speaks a reminder instead of opening an empty
menu: `Ctrl+Shift+N` makes a new crate, `Ctrl+N` a new playlist.

## Reordering tracks in a playlist

With one or more tracks selected in a playlist:

- `Alt+Up` / `Alt+Down` — Move the selection one row up or down.
- `Alt+PageUp` / `Alt+PageDown` — Move it a screenful.
- `Alt+Home` / `Alt+End` — Move it to the very top or bottom.

Every move is confirmed out loud: "Moved to position 4 of 12".

## Numark Scratch mixer (hold Shift)

- `Shift+CUE` (either channel) — Speak that deck's status. Does not
  change the cue/PFL state.
- `Shift+Echo` (Effect Unit 1, leftmost FX button) — Repeat the last
  announcement.
- `Shift+Delay` (Effect Unit 1, middle FX button) — Beat click
  metronome on/off.
- `Shift+Flanger` (Effect Unit 1, right FX button) — Per-deck split
  cue on/off.

Without Shift everything behaves as normal. Effect Unit 2's FX buttons
(Reverb/V.Echo/Phaser) keep their usual Shift-to-toggle-that-effect
behavior — only Unit 1's three are repurposed.

Pressing Shift itself says "Shift", and the pad-mode selector button
announces where the pads landed each time it cycles — "Pads, hot
cues", "Pads, loop roll", "Pads, sampler" — so press it until you
hear the layer you want.

## Pioneer DDJ-400 accessibility pads (opt-in)

Turn on "Use the Hot Cue pads as accessibility pads" under
Preferences > Controllers > DDJ-400. In Hot Cue pad mode, left pads
speak deck 1, right pads deck 2:

- Pad 1 — Full deck status
- Pad 2 — Time remaining
- Pad 3 — BPM
- Pad 4 — Musical key
- Pad 5 — Bar and beat position
- Pad 6 — Track name
- Pad 7 — Repeat last announcement
- Pad 8 — Beat click metronome on/off
- `Shift+Pad 1` — Halve this deck's BPM grid
- `Shift+Pad 2` — Double this deck's BPM grid
- `Shift+Pad 7` — Per-deck split cue on/off
- `Shift+Pad 8` — Speech on/off

Hot cues are unavailable from the pads while this is on (Shift+pads
3-6 do nothing, so nothing can be cleared by accident). Untick the
setting to get normal hot cues back; other pad modes are unaffected.

Independent of that setting: pressing Shift says "Shift", and each
pad mode button announces its layer ("Pads, hot cues", "Pads, beat
loop", "Pads, beat jump", "Pads, sampler", plus the shifted modes).
Two more settings on the same preferences page: "Disable jog wheel
scratching" (platter touch does nothing; rotation still nudges
pitch) and "Jog wheel sensitivity" (scales the nudge, the Shift+jog
fast seek, and the scratch response — lower it if Shift+jog feels
like the track shoots along).

## Where the rest lives

Everything else — per-event sound/speech choices, mixer announcements,
ducking strength, voice and rate — is a checkbox or combo box in
Options > Preferences > Accessibility, not a keyboard shortcut. See the
"Preferences reference" section of
[ACCESSIBILITY_GUIDE.md](ACCESSIBILITY_GUIDE.md) for the full list.

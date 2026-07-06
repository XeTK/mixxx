# Mixxx Accessibility Guide

A practical guide to DJing with this accessibility build of Mixxx as a
blind or visually impaired user. Everything here works with the
keyboard and spoken announcements; a screen reader (JAWS, NVDA, or
VoiceOver) is recommended for the menus and preferences dialogs but is
not needed while performing.

This guide is written to be read with a screen reader: headings mark
every section, keyboard shortcuts are spelled out, and there are no
images.

## Quick start

1. Start Mixxx. When the interface has loaded you will hear
   "Mixxx ready."
2. Press Tab or use your screen reader to reach the library search
   box, type part of a track name, then press Escape to move to the
   track list.
3. Arrow up and down: each track's artist and title is spoken.
4. Load the highlighted track: Shift plus Left Arrow loads deck 1,
   Shift plus Right Arrow loads deck 2. You will hear, for example:
   "Loaded deck, A. Artist. Title. 128 B P M. Key: A Minor."
5. Press D to play deck 1 or L to play deck 2 (US keyboard layout).
   You will hear "Playing."

## The speech system

Announcements are produced inside Mixxx and mixed into Mixxx's own
audio output, ducking the music underneath so speech stays
intelligible. Speech is never recorded or broadcast.

- Toggle speech: Alt plus Shift plus A, or the Options menu item
  "Enable Text-to-Speech." Turning speech on says "Speech on."
- Repeat the last announcement: Alt plus Shift plus R.
- Where speech goes: by default the headphone output, so the audience
  never hears it. If no headphone output is configured, it
  automatically falls back to the main output. Change this under
  Preferences, Accessibility, "Speech output."
- Voice, speaking rate, and how far the music ducks under speech are
  all in Preferences, Accessibility, with a Test Speech button that
  plays a sample through your current settings.

## Information on demand

Press these at any time to hear the state of a deck. Odd numbers are
deck 1, even numbers are deck 2.

- Alt plus 1 or 2: full deck status — playing or stopped, time
  remaining, BPM, and pitch position.
- Alt plus 3 or 4: time remaining only.
- Alt plus 5 or 6: BPM only.
- Alt plus 7 or 8: musical key (follows keylock).
- Alt plus 9 or 0: bar and beat position, for example "Bar 17,
  beat 2." Assumes 4/4 time.

Tip: turn on "Concise announcements" in Preferences, Accessibility to
shorten these to just the value — Alt plus 5 then says "128." instead
of "Deck, A. 128 B P M."

## Sounds instead of speech (earcons)

Frequent transport events can be signalled with short percussive sounds
instead of — or as well as — speech. A sound plays instantly and does
not tie up the speech channel, which matters when you are acting fast.
The events covered are play, stop, end of track, and headphone cue on
and off; everything else is always spoken.

Each of the four events is set independently under Preferences,
Accessibility, in the Playback Announcements group ("Play feedback",
"Stop feedback", "End of track feedback", "Headphone cue feedback").
Each offers:

- **Speech** — spoken as before.
- **Sounds** — a short percussive cue, panned to the deck (deck 1 in
  the left ear, deck 2 in the right, matching the beat click and split
  cue). Rising = start/engage, falling = stop, three quick pips = end
  of track.
- **Sounds and speech** (default) — both, so you learn which sound
  means what. Once the sounds are familiar, switch to Sounds only.

Because it is per event, you can, for example, keep play and stop as
sounds but have end of track still spoken with its time remaining. Each
event's on/off checkbox above still applies: unchecking it silences the
event entirely regardless of the feedback choice.

Sound level is the `[Earcon],volume` control. In Sounds mode the
end-of-track cue is just the alert; the spoken time-remaining is only
added when speech is on.

## Performance tools

- Crossfader lock: Alt plus X freezes the crossfader where it is, so
  a bump against the fader does nothing. Press again to unlock. Both
  states are spoken.
- Beat click metronome: Alt plus B plays a click on every beat of
  each playing deck — deck 1 in your left ear, deck 2 in your right.
  Every fourth beat is a higher pitched bar marker. Use it to check
  beat grids or to practice beatmatching. The clicks go to your
  headphones and are never recorded.
- Per-deck split cue: Alt plus H puts deck 1's headphone cue in your
  left ear and deck 2's in your right, each folded to mono. Combined
  with the beat click, each ear carries one deck's audio and grid.
  Press again to return to normal stereo cueing.
- The headphone mix knob still works in split mode: turning it toward
  main blends the master output into both ears.

## What gets announced automatically

Every category below has its own checkbox in Preferences,
Accessibility.

Transport and decks:

- Track loads, with deck, artist, title, BPM, and key
- Play and stop; holding the cue button says "Cue" instead
- End of track, including how much time is left
- Jumping back to the start of the track
- Headphone cue (PFL) on and off, with the deck name

Performance controls:

- Sync, key lock, and quantize toggles
- Loops turning on and off with their size, and loop size changes
- Hotcues 1 to 8 being set, cleared, or pressed; setting the main cue
  point
- Pitch fader position after it stops moving, with the resulting BPM
- Recording started and stopped

Mixer (off by default — turn on "Announce mixer controls"):

- Channel volume faders, trim knobs, EQ knobs, filter knobs
- Main and headphone volume
- Crossfader position
- Effect unit dry/wet and super knobs

Values are spoken as fractions of the control's travel, for example
"volume three quarters" or "E Q low minus a quarter" — center-detented
knobs speak their deviation from center. If you prefer running
commentary while a control moves, enable "Announce controls while they
move"; otherwise only the resting value is spoken.

Library:

- The focused pane: search bar, sidebar, or track list
- Sidebar items as you arrow through them, with your position ("3 of
  12") and, for folders, whether they are expanded and how many items
  are inside
- Search feedback while typing
- Confirmation when you add a track to or remove it from a playlist
  or crate

## Preferences reference

All settings live under Options, Preferences, Accessibility. In order:

1. Announce Mixxx ready at startup
2. Speech output: headphones (DJ only) or main output
3. Voice and speech rate, with a test button
4. Music ducking during announcements: how far the music drops while
   speech plays
5. Speak deck names as numbers: "Deck 1" instead of "Deck A"
6. Concise announcements: shortest possible phrasing
7. One checkbox per announcement category listed above

## Using a screen reader alongside Mixxx

JAWS, NVDA, and VoiceOver read the menus, preferences dialogs, library
search box, sidebar, and track list — these all carry accessible
names. Mixxx's own speech covers the performance surfaces the screen
reader cannot see.

Important for live sets: your screen reader speaks through the Windows
default audio device. If that device is your main (audience) output,
the audience will hear it. To prevent this:

- In JAWS: open the JAWS window, choose Utilities, then Sound Card
  selection, and pick your headphone or laptop sound device instead of
  the performance interface.
- In NVDA: NVDA menu, Preferences, Settings, Audio, "Output device."
- Also set the Windows default device to something other than your
  performance interface, so system sounds stay off the main output.
  Mixxx addresses its performance interface directly, so this does not
  affect your mix.

## Controller mapping

Every accessibility feature is an ordinary Mixxx control and can be
mapped to buttons on a DJ controller:

- `[Tts],enabled` and `[Tts],repeat` — speech toggle, repeat
- `[ChannelN],tts_status`, `tts_time`, `tts_bpm`, `tts_key`,
  `tts_bar` — the information readouts
- `[Master],crossfader_lock` — crossfader lock
- `[Master],headSplitDecks` — per-deck split cue
- `[BeatClick],enabled` and `[BeatClick],volume` — metronome
- `[Tts],duckStrength` — music ducking level

## Known limitations

- Announcements are English only for now.
- The first-run sound hardware dialog appears before Mixxx's speech
  can produce audio; use your screen reader for initial setup.
- Error dialogs are read by your screen reader, not by Mixxx's speech.
- The bar and beat readout assumes 4/4 time.

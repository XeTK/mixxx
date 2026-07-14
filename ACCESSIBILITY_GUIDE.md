# Mixxx Accessibility Guide

A practical guide to DJing with this accessibility build of Mixxx as a
blind or visually impaired user. Everything here works with the
keyboard and spoken announcements; a screen reader (JAWS, NVDA, or
VoiceOver) is recommended for the menus and preferences dialogs but is
not needed while performing.

This guide is written to be read with a screen reader: headings mark
every section, keyboard shortcuts are spelled out, and there are no
images. Already familiar with the workflow and just want the key list?
See [ACCESSIBILITY_QUICK_REFERENCE.md](ACCESSIBILITY_QUICK_REFERENCE.md)
for a one-page cheat sheet.

## Quick start

1. Start Mixxx. When the interface has loaded you will hear
   "Mixxx ready."
2. Press Tab or use your screen reader to reach the library search
   box, type part of a track name, then press Escape to move to the
   track list.
3. Arrow up and down: each track's artist and title is spoken.
4. Load the highlighted track: Shift plus Left Arrow loads deck 1,
   Shift plus Right Arrow loads deck 2. You will hear, for example:
   "Loaded deck, Alpha. Artist. Title. 128 B P M. Key: A Minor."
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

### macOS: getting better-sounding voices

macOS ships every voice at three possible quality levels, but only the
robotic-sounding "Default" tier is installed out of the box. The much
more natural "Enhanced" and "Premium" (neural) versions of the same
voices — Ava, Nathan, Zoe, Samantha, and others — are free and fully
offline, but have to be downloaded once:

1. Open System Settings, then Accessibility, then Spoken Content.
2. Next to "System Voice," open the voice picker and choose "Manage
   Voices" (or click the small info button next to the dropdown,
   depending on your macOS version).
3. Voices are grouped by language. Look for entries marked Enhanced or
   Premium — most languages offer several. Click the download icon
   next to any voice you want; each is a few hundred MB, one-time,
   no internet needed afterward.
4. Tip: press the play button next to a voice in this list to preview
   it before downloading, so you're not guessing.

Once downloaded, the new voices appear automatically in Mixxx's voice
list (reopen Preferences, Accessibility if it was already open). A
"Voice quality" combo next to the voice picker — macOS only — lets you
filter the list to All, Default, Enhanced, or Premium, so you can jump
straight to the voices worth using instead of scrolling past roughly
150 Default-tier entries.

## Information on demand

Press these at any time to hear the state of a deck. Odd numbers are
deck 1, even numbers are deck 2.

- Alt plus 1 or 2: full deck status — playing or stopped, time
  remaining, BPM, and pitch position.
- Alt plus 3 or 4: time remaining only.
- Alt plus 5 or 6: BPM only.
- Alt plus 7 or 8: musical key (follows keylock). Spoken in whatever
  notation Preferences, Interface, Key Notation is set to — the full
  name ("A Minor") for Traditional, or the short code for Open Key
  ("5, Delta") and Camelot/Lancelot ("8, Alpha"), with the letter
  always spelled out using the NATO phonetic alphabet (Alpha, Bravo,
  Charlie, …) so it isn't misheard.
- Alt plus 9 or 0: bar and beat position, for example "Bar 17,
  beat 2." Assumes 4/4 time.
- Alt plus Shift plus T (deck 1) or Alt plus Shift plus Y (deck 2):
  re-announce the loaded track's artist and title — useful if you
  missed the original load announcement or forgot what's playing.

Tip: turn on "Concise announcements" in Preferences, Accessibility to
shorten these to just the value — Alt plus 5 then says "128." instead
of "Deck, Alpha. 128 B P M."

## Sounds instead of speech (earcons)

Frequent events can be signalled with short percussive sounds instead
of — or as well as — speech. A sound plays instantly and does not tie
up the speech channel, which matters when you are acting fast. The
events covered are play, stop, end of track, headphone cue on/off,
jumping back to the start of the track, loop on/off, and audio
clipping; everything else is always spoken.

Set them under Preferences, Accessibility. The core four (play, stop,
end of track, headphone cue) live in the Playback Announcements group,
where "All transport feedback" sets all four at once, or set each with
its own combo ("Play feedback", "Stop feedback", "End of track
feedback", "Headphone cue feedback"); when the four differ, "All
transport feedback" reads Custom. Back-to-start feedback sits alongside
them. Loop on/off feedback is next to the loop checkbox in Performance
Announcements, and Clipping feedback is next to the clipping checkbox
near the top of the page. Each combo offers:

- **Speech** — spoken as before.
- **Sounds** — a short percussive cue. Deck-scoped events (play, stop,
  end of track, cue, restart, loop) are panned to the deck — deck 1 in
  the left ear, deck 2 in the right, matching the beat click and split
  cue; clipping is centered since it's a whole-mix issue. Rising =
  start/engage, falling = stop/disengage, three quick pips = end of
  track, a double-tap = back to start, a low double-buzz = clipping.
- **Sounds and speech** (default) — both, so you learn which sound
  means what. Once the sounds are familiar, switch to Sounds only.

Because it is per event, you can, for example, keep play and stop as
sounds but have end of track still spoken with its time remaining. Each
event's on/off checkbox still applies: unchecking it silences the
event entirely regardless of the feedback choice.

Sound level is the `[Earcon],volume` control. In Sounds mode, events
that also carry information when spoken (end of track's time
remaining, loop's beat count) only add that detail when speech is on;
the sound alone is just the alert.

## Performance tools

- Crossfader lock: Alt plus X freezes the crossfader where it is, so
  a bump against the fader does nothing. Press again to unlock. Both
  states are spoken.
- Jog wheel touch lock: Alt plus J stops the on-screen waveform display
  and the vinyl-look widget from responding to click-and-drag, on both
  decks, so an accidental brush of the mouse or a touchscreen can't
  scratch or bend the pitch of whatever is playing. Press again to
  unlock. Both states are spoken, and it works at any time, including
  while a track is already playing. Turn it on by default under
  Preferences, Decks ("Disable jog wheel and waveform touch
  scratching").
- Beat click metronome: Alt plus B plays a click on every beat of
  each playing deck — deck 1 in your left ear, deck 2 in your right.
  Every fourth beat is a higher pitched bar marker. Use it to check
  beat grids or to practice beatmatching. The clicks are never
  recorded, and they go wherever your announcements go: the "Speech
  output" setting in Preferences, Accessibility routes speech, the
  beat click, and the earcon sound cues together — headphones or main
  output. So if you can hear Mixxx talking, you can hear the click.
  Its volume has its own slider ("Beat click volume") — turn it up if
  the clicks are hard to hear over a loud mix.
- Per-deck split cue: Alt plus S puts deck 1's headphone cue in your
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
- Audio clipping on the main output (on by default — a safety and
  audio-quality signal). Throttled so sustained clipping doesn't
  repeat the warning constantly.

Performance controls:

- Sync, hold-aware: a quick press of the sync button speaks "beat
  synced. Hold sync to lock"; keep it held and "sync locked" confirms
  once the lock actually engages; pressing again speaks "sync off"
- Key lock and quantize toggles
- Loops turning on and off with their size, and loop size changes
- Beat jump: size changes and forward/back jumps, with the size in
  beats
- Hotcues 1 to 8 being set, cleared, or pressed; setting the main cue
  point
- Pitch fader position after it stops moving, with the resulting BPM
- Recording started and stopped

Effects (its own "Announce effects" checkbox, on by default):

- An effect turning on or off, by name ("Unit 1 Echo on")
- A different effect being loaded into a slot ("Unit 2: Flanger
  loaded")
- Your deck being routed through an effect unit ("Deck A effect unit
  2 on")
- The filter knob's effect type when you switch it ("Deck A filter:
  Moog Filter")

Mixer (off by default — turn on "Announce mixer controls"):

- Channel volume faders, trim knobs, EQ knobs, filter knobs
- Main and headphone volume
- Crossfader position
- Headphone mix (how much cue vs. main you hear), for example
  "Headphone mix cue three quarters" or "Headphone mix main a half"
- Effect unit dry/wet and super knobs

Values are spoken as fractions of the control's travel by default, for
example "volume three quarters" or "E Q low minus a quarter" —
center-detented knobs speak their deviation from center. Fractions
resolve in eighths by default; "Fraction detail" in Preferences,
Accessibility offers Quarters (coarser) or Sixteenths (finer). Prefer
exact numbers? Switch "Speak mixer values as" to Percentages, and the
same readouts become "volume 75 percent" / "E Q low minus 25 percent".
If you prefer running commentary while a control moves, enable
"Announce controls while they move"; otherwise only the resting value
is spoken.

Library:

- The focused pane: search bar, sidebar, or track list
- Sidebar items as you arrow through them, with your position ("3 of
  12") and, for folders, whether they are expanded and how many items
  are inside
- Search feedback while typing
- Confirmation when you add a track to or remove it from a playlist
  or crate
- With a track selected, Alt plus Shift plus C (crate) or Alt plus
  Shift plus P (playlist) opens a small menu of your crate or playlist
  names. Arrow through it and each name is spoken with its position
  ("House, 2 of 5"); press Enter to add the track, Escape to back out
  without adding it. If you don't have any yet, it tells you instead
  of opening an empty menu — Control plus Shift plus N makes a new
  crate, Control plus N a new playlist.
- Control plus N (new playlist) or Control plus Shift plus N (new
  crate) speaks that the dialog opened, and that its text box already
  has a name filled in and selected — type to replace it, or press
  Enter to accept it as-is. Once you press Enter, it reads back
  exactly what you typed ("You entered: Warmup") before checking
  whether the name is valid, so you can catch a typo before it becomes
  the playlist or crate's name. If the name is already taken or blank,
  it speaks the problem ("A playlist by that name already exists") and
  the dialog reopens so you can try again.
- Deleting a playlist or crate (from its right-click menu) speaks what
  you're about to delete and reminds you that No is the default button
  — pressing Enter without moving focus cancels, it does not delete.
  Once you do confirm, it speaks "Deleted playlist/crate X".
- Renaming or duplicating a playlist or crate works the same way: the
  dialog announces itself with the prefilled name, reads back what you
  typed, speaks any problem with the name, and confirms the result
  ("Renamed playlist Warmup to Openers").
- Switching into the Playlists or Crates pane speaks "Playlists view"
  or "Crates view".
- Reordering inside a playlist: select a track (or several) and press
  Alt plus Up or Alt plus Down to move it; Alt plus Page Up / Page
  Down move further, Alt plus Home / End go to the ends. Every move is
  confirmed: "Moved to position 4 of 12". (Dragging with the mouse
  speaks the same confirmation.)

## Preferences reference

All settings live under Options, Preferences, Accessibility. In order:

1. Announce Mixxx ready at startup
2. Announce audio clipping, and its feedback style (speech/sounds/both)
3. Speech output: headphones (DJ only) or main output
4. Voice and speech rate, with a test button (macOS also has a "Voice
   quality" filter — see the macOS section above)
5. Music ducking during announcements: how far the music drops while
   speech plays
6. Beat click volume
7. Speak deck names as numbers: "Deck 1" instead of "Deck A"
8. Concise announcements: shortest possible phrasing
9. Speak mixer values as fractions or percentages
10. Fraction detail: quarters, eighths (default), or sixteenths
11. One checkbox per announcement category — including "Announce
    effects" — most with their own feedback-style combo
    (speech/sounds/both) for play, stop, end of track, headphone cue,
    back-to-start, and loop on/off

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
  `tts_bar`, `tts_track` — the information readouts
- `[Master],crossfader_lock` — crossfader lock
- `[Master],headSplitDecks` — per-deck split cue
- `[Master],disable_touch_scratch` — jog wheel touch lock
- `[BeatClick],enabled` and `[BeatClick],volume` — metronome
- `[Tts],duckStrength` — music ducking level
- `[Earcon],volume` — earcon (sound cue) level

### Numark Scratch (built in)

The shipped Numark Scratch mapping already carries an accessibility
layer on the Shift button (hold Shift, then press):

- Shift plus a channel's CUE button — speak that deck's full status
  (playing or stopped, time remaining, BPM, pitch). The CUE (PFL)
  state itself is not changed.
- Shift plus Effect Unit 1's Echo button — repeat the last
  announcement.
- Shift plus Effect Unit 1's Delay button — beat click metronome
  on/off.
- Shift plus Effect Unit 1's Flanger button — per-deck split cue
  on/off.

Each action confirms itself out loud. Without Shift, all these buttons
keep their normal functions, and Effect Unit 2's three FX buttons
(Reverb, V.Echo, Phaser) are untouched — Shift still toggles those
effects individually, same as upstream. (The single pad-mode selector
button was deliberately left alone: it cycles blind through Hotcue,
Roll, and Sampler with no way to tell which state you're in without
looking, so it isn't a reliable target for a blind DJ.)

### Pioneer DDJ-400 (built in, opt-in)

The shipped DDJ-400 mapping has an optional accessibility pad layer,
off by default. Turn it on under Preferences, Controllers, DDJ-400:
check "Use the Hot Cue pads as accessibility pads". While enabled, the
Hot Cue pad mode speaks instead of triggering hotcues:

- Pad 1 — full deck status (playing or stopped, time remaining, BPM,
  pitch)
- Pad 2 — time remaining
- Pad 3 — BPM
- Pad 4 — musical key
- Pad 5 — bar and beat position
- Pad 6 — track name (artist and title)
- Pad 7 — repeat the last announcement
- Pad 8 — beat click metronome on/off
- Shift plus pad 7 — per-deck split cue on/off
- Shift plus pad 8 — speech on/off

Pads 1 to 6 speak about their own deck (left pads deck 1, right pads
deck 2). Shift plus pads 1 to 6 deliberately do nothing, so a stray
press can't clear stored hotcues. The other pad modes (Beat Loop, Beat
Jump, Sampler) are unaffected. Untick the setting to get normal hot
cues back.

## Known limitations

- Announcements are English only for now.
- The first-run sound hardware dialog appears before Mixxx's speech
  can produce audio; use your screen reader for initial setup.
- Error dialogs are read by your screen reader, not by Mixxx's speech.
- The bar and beat readout assumes 4/4 time.

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
   "Mixxx ready." The very first time Mixxx is ever launched, a short
   spoken orientation follows once a sound device is confirmed open:
   the speech toggle (Alt plus Shift plus A), the full deck status
   readouts (Alt plus 1 / Alt plus 2), how to repeat the last thing
   spoken (Alt plus Shift plus R), how to open the accessibility short
   menu (Alt plus Shift plus M, or holding the browse knob on a
   DDJ-400), and a pointer to this guide and the quick reference for
   everything else. It plays once ever, not on
   every launch — there is no menu item or shortcut to replay it
   deliberately yet, so come back to this guide or the quick reference
   if you want to hear it described again. If speech is off at that
   first launch, nothing is spoken, but the one-time flag is still
   marked as done — this is a first-run nudge, not a nag that follows
   you around after you've started using Mixxx sighted.
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
tapping the cue button (cue preview — a single very short high tick,
so rapid cue taps while beatmatching read as a rhythm instead of "Cue
Cue Cue"), jumping back to the start of the track, loop on/off, and
audio clipping; everything else is always spoken. The cue tap follows
the same "Headphone cue feedback" combo as the headphone cue.

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
- Fixing a wrong BPM analysis: fast genres (drum and bass, footwork,
  hard techno) are often analysed at half their real tempo. Control
  plus Alt plus D doubles deck 1's BPM grid, Control plus Alt plus H
  halves it; add Shift for deck 2. Each press confirms itself ("Deck 1
  B P M doubled") — then Alt plus 5 or 6 reads the exact new number.
- Quantize toggle: Control plus Alt plus Q for deck 1, add Shift for
  deck 2. Quantize snaps cues, loops, and play presses to the beat
  grid; it's on by default and remembered per deck across restarts, so
  if it ever ends up off this is the way back on. Both states are
  spoken ("Deck 1 quantize on").
- Filing the playing track: Control plus Alt plus P opens the playlist
  picker for the track loaded in deck 1, Control plus Alt plus C the
  crate picker; add Shift for deck 2. It's the same spoken menu as the
  library's Alt plus Shift plus P/C, but for what's on the deck — so
  when a track is going down well you can file it mid-mix without
  hunting it down in the library. The add is confirmed out loud.
- Mixer, EQ, and filter without a controller: Alt plus U and G step
  deck 1's volume down and up, Alt plus H and D step trim (gain), Alt
  plus L and F11 step EQ low, Alt plus E and I step EQ mid, Alt plus Q
  and F10 step EQ high, and Alt plus F and W step the filter (the
  QuickEffect knob); add Shift for deck 2. These are the same
  underlying controls a DDJ-400 or any other controller would use —
  this just gives the same access from the keyboard alone. There's no
  fine ("small step") variant yet, only the coarse step. The
  crossfader (H/G, Shift+H/Shift+G for a fine step) and the EQ low
  kill toggle (B/N) already worked without a controller and aren't
  new. (These used to be on Control plus Alt chords; they moved to
  plain Alt because Control plus Alt collides with macOS's own
  Command plus Option shortcuts, and with AltGr character entry on
  non-US Windows/Linux keyboard layouts — issue #56.)
- Effects without a controller: Alt plus N turns Effect Unit 1 on/off,
  Alt plus F9 cycles to the next chain preset (a different bundle of
  effects), and Alt plus Z enables/disables the effect in slot 1; add
  Shift for Effect Unit 2. Effect Units 3 and 4, and slots 2-4 within a
  unit, aren't wired to the keyboard yet. Cycling which of the unit's
  slots is focused (Control plus Alt plus L) and selecting the
  next/previous effect in slot 1 (Control plus Alt plus J / X) are
  still on the old Control plus Alt chords — there wasn't enough safe
  letter-space this round to move them too, so they're pending a
  follow-up that routes them through the Accessibility menu as actions
  ("focus next effect slot", "load next/previous effect") instead of
  dedicated keyboard chords, since they're more setup actions than
  moment-to-moment mixing ones (issue #56 follow-up).
- Smart cue (on by default): loading a track into a stopped deck moves
  the headphone cue to that deck automatically — like the smart cue on
  Denon players, the thing you just loaded is what you preview next.
  The switch is spoken through the normal cue announcements ("Deck 2
  headphone cue on. Deck 1 headphone cue off"). A playing deck never
  has its cue taken away, and loading into a playing deck (when
  allowed) doesn't touch the cue at all. This is a general deck-loading
  behavior, not accessibility-specific, so it's not tucked away on the
  Accessibility page — turn it off with the "Smart cue" checkbox in
  Preferences, Decks, next to "Loading a track, when deck is playing".

## Auto DJ

- Enable/disable: Shift plus F12. Both states are always spoken;
  enabling also speaks the next queued track ("Auto DJ on. Next:
  Artist, Title"), or just "Auto DJ on" if the queue is empty.
- Fade now: Shift plus F11 crossfades to the next track immediately.
  Says "Fading now"; the resulting track load still gets its own
  separate load announcement (artist, title, BPM, key) once it lands.
- Skip next: Shift plus F10 drops the queued track without playing
  it. Says "Skipped".
- Shuffle the queue: Shift plus F9.
- Add a random track to the queue: Control plus Shift plus F9.
- What's next, on demand: Alt plus Shift plus N speaks whether Auto
  DJ is on, the next queued track's artist and title (or "Queue is
  empty"), and — if a deck is currently playing — roughly how long
  until it hands off, for example "About 1 minute 30 seconds
  remaining on Deck, Alpha." This last part is an estimate: the real
  crossfade can start earlier than the track's end, depending on the
  outro point and fade mode, so treat it as a rough warning rather
  than an exact countdown.
- The Auto DJ panel's transition-time spinbox, fade-mode combo, and
  the enable/fade/skip/shuffle/add-random/repeat buttons are all
  reachable with Tab — they used to be mouse-only.

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
- Main and headphone volume, spoken as plain knob travel ("Headphone
  volume three quarters"); the halfway point is unity gain
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

The name and the value are split around the movement: the moment a
control starts moving it names itself ("Deck 1 volume"), and the value
("three quarters") follows once it stops. Keep adjusting the same
control and you hear only new values — "a half", "5 eighths" — with no
chatter while it travels. After anything else is announced, or about
eight seconds of quiet, the next touch names the control again. A
control that lands back on the readout it already announced stays
silent entirely — no name, no value — so a worn, jittery pot can't
chant at you, and nudging a knob that's already where you want it says
nothing new. ("Announce controls while they move" overrides all of
this with running values as before.)

Library:

- The focused pane: search bar, sidebar, or track list
- Sidebar items as you arrow through them, with your position ("3 of
  12") and, for folders, whether they are expanded and how many items
  are inside
- Search feedback while typing, including how many tracks matched
  ("Searching: techno. 42 tracks") — the search box understands
  filters like `bpm:170-180`, `key:am`, `genre:jungle`, and
  `year:>2020`, so this doubles as a spoken way to slice the library
- Loading into a playing deck, when "Loading a track, when deck is
  playing" is set to Reject (the default), speaks why nothing
  happened: "Deck 1 is playing, load blocked. Stop the deck first."
  instead of silently ignoring the keypress
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
12. "Allow controller navigation when Mixxx isn't focused" — off by
    default; see "Controller navigation without window focus" under
    "Controller mapping" below

Smart cue (headphone cue follows the loaded track, on by default) is a
general deck-loading behavior rather than an accessibility setting, so
it lives in Preferences, Decks instead — see "Performance tools" above.

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
- `[ChannelN],quick_add_to_playlist` / `quick_add_to_crate` — the
  filing pickers for the track loaded in a deck
- `[Tts],shift` — set to 1 while a controller's shift button is held;
  "Shift" is spoken on the press (mappings drive this)
- `[Tts],pad_mode` — spoken pad-layer feedback for controllers with
  pad mode buttons; a mapping writes 1 hot cues, 2 beat loop, 3 beat
  jump, 4 sampler, 5 keyboard, 6 pad effects 1, 7 pad effects 2,
  8 key shift, 9 loop roll. Writing the value already set stays
  silent — a mapping that wants a re-press to re-announce (so a
  blind DJ can query the current layer) bounces the value through 0
  first, which is outside the spoken vocabulary and so doesn't itself
  announce anything.

### Controller navigation without window focus

Normally, controller-driven library navigation (browse/rotate,
sidebar and track-list movement) is dropped whenever the Mixxx window
does not have OS keyboard focus — the same as stock Mixxx. This
becomes a real problem for a blind DJ using a controller such as the
DDJ-400 alongside a screen reader: alt-tabbing to VoiceOver, JAWS, or
NVDA to read something takes window focus away from Mixxx, and
controller navigation input is silently dropped until you tab back.

To keep controller navigation working in that situation, either:

- Check "Allow controller navigation when Mixxx isn't focused" under
  Preferences, Accessibility. Takes effect immediately, no restart
  required.
- Or start Mixxx with the `--controller-navigation-without-focus`
  command-line flag (handy for scripting or testing). Either one is
  enough on its own; you don't need both.

This only affects library/browse navigation controls (see "Controller
mapping" above); it does not change how deck transport, mixer, or
effects controls behave when Mixxx is unfocused — those already work
regardless of window focus.

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
effects individually, same as upstream.

Two spoken confirmations cover the layer itself: pressing Shift says
"Shift", and the pad-mode selector button announces where the pads
landed ("Pads, hot cues", "Pads, loop roll", "Pads, sampler") each
time it cycles — so the blind-cycling mode button is now usable: press
it until you hear the layer you want.

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
- Shift plus pad 1 — halve this deck's BPM grid
- Shift plus pad 2 — double this deck's BPM grid (the pad-sized
  version of Control plus Alt plus H/D, for fixing a half-tempo
  analysis by ear — confirmed out loud)
- Shift plus pad 7 — per-deck split cue on/off
- Shift plus pad 8 — speech on/off

Pads speak about and act on their own deck (left pads deck 1, right
pads deck 2). Shift plus pads 3 to 6 deliberately do nothing, so a
stray press can't clear stored hotcues. The other pad modes (Beat
Loop, Beat Jump, Sampler) are unaffected. Untick the setting to get
normal hot cues back.

Two more settings live next to it on the same page:

- "Disable jog wheel scratching" — touching the top of the jog wheel
  no longer grabs the track like vinyl, so a stray touch can't stop or
  scratch playback; the platter still nudges the pitch for
  beatmatching, and Shift plus jog still seeks.
- "Jog wheel sensitivity" — scales how strongly the jog nudges the
  pitch, how fast Shift plus jog seeks through the track, and how
  fast scratching responds while scratching is enabled. 1.0 is the
  stock feel; lower is gentler, higher more aggressive. (Shift plus
  jog is the mapping's fast seek — deliberately quick, about 150
  times a normal nudge, for skipping through a track; if it feels
  like the track "shoots along", lower this setting.)

Independent of that setting, the mapping speaks the layer buttons
themselves: pressing Shift says "Shift", and each pad mode button
announces the layer it selected — "Pads, hot cues", "Pads, beat
loop", "Pads, beat jump", "Pads, sampler". The shifted modes
("Pads, keyboard", "Pads, pad effects 1", "Pads, pad effects 2",
"Pads, key shift") have no pad layer behind them yet — see the "Not
implemented" note at the top of the DDJ-400 mapping script — so their
announcement adds "(not yet supported)"; the pads themselves stay
dead in those four layers. Pressing the mode you're already in
re-announces it (rather than staying silent), so you can check which
of the eight layers you're on without cycling through the rest.

## Timecode vinyl (DVS)

Mixxx can be driven from timecode records or CDs through a supported
sound card or DVS mixer (the Numark Scratch is one). Configure the
deck inputs under Preferences, Sound Hardware, Input, then enable
vinyl control per deck.

Keyboard controls (all spoken as they change):

- Ctrl+T / Ctrl+Y / Ctrl+U / Ctrl+I — vinyl control on/off for decks
  1 to 4 ("vinyl control on").
- Ctrl+Shift+Y (deck 1) / Ctrl+Shift+U (deck 2) — cycle the vinyl
  mode: absolute (the needle position is the track position),
  relative (the record controls speed and direction, software
  controls position), constant (emergency mode near the end of the
  record).
- Ctrl+Alt+Y (deck 1) / Ctrl+Alt+U (deck 2) — cycle needle-drop
  cueing for relative mode: off, "goes to cue point", or "goes to
  nearest hotcue". With cueing on, lifting the needle and dropping it
  anywhere jumps playback to the cue instead of following the needle.

Mode changes are announced even when Mixxx changes them by itself:
setting a loop or seeking while playing drops absolute mode to
relative, and reaching the end of the record switches to constant
mode, so you always hear why the deck stopped following the
turntable.

How cue buttons behave with vinyl control:

- While the record is spinning, the timecode signal owns play and
  stop. Cue and hotcue presses jump the deck to the cue point and
  playback continues from there — the same as other DVS systems. To
  stop, stop the record.
- With the needle up, all software transport works normally: play,
  cue preview and hotcue previews from the keyboard or a controller
  behave exactly as without vinyl control. The moment the needle
  comes back down, the timecode takes over again.
- Brief timecode dropouts (a dirty needle, a worn spot) no longer
  yank playback back to the nearest cue in needle-drop cueing mode;
  only a real needle lift of about half a second or more counts as a
  needle drop.

"Prevent jogging before track start" (Preferences, Decks — on by
default in this fork) also applies to vinyl: dropping the needle in
the record's lead-in area clamps the deck near the track start
instead of leaving you in silent pre-roll. The "Pre-roll limit" spin
box next to it sets how much run-up is still allowed — 4 beats by
default, so you keep a natural backspin-and-release feel; set it to 0
to clamp hard at the first beat, or turn the checkbox off to allow
unlimited pre-roll like stock Mixxx.

## Known limitations

- The first-run spoken orientation (see "Quick start" above) plays
  exactly once and cannot yet be replayed on demand from a menu item
  or shortcut; this guide and the quick reference are the fallback.
- Announcements are English only for now.
- The first-run sound hardware dialog appears before Mixxx's speech
  can produce audio; use your screen reader for initial setup.
- Error dialogs are read by your screen reader, not by Mixxx's speech.
- The bar and beat readout assumes 4/4 time.

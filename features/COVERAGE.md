# Coverage — what this plan tests, and what it does not

21 pull requests, #68 through #88, closing issues #47 through #67. This
file maps each one to the scenarios that exercise it, and then says what is
still untested afterwards.

Six more pull requests — #38, #39, #40, #41, #42 and #45, closing issues
#30, #36, #33, #17, #14 and #32 respectively — merged before this plan
existed and were consequently never mapped anywhere. They are covered
below in their own section, added after the fact and verified against the
merged source rather than against a spec, same as everything else here.
Issue #11 (a CMake/build config fix) and the `pi-arm64-build-and-updater`
merges are deliberately excluded from both sections: there is nothing for
a blind user to manually test in either.

The second half is the more useful half. A test plan that only lists what
it covers invites the reader to assume the rest is fine.

**Totals:** 257 scenarios as written across 9 feature files; 363
individual runs once `Scenario Outline` examples are expanded.

---

## Per-pull-request coverage

### PR #68 — issue #47 — DDJ-400 browse knob decode and clamp

| | |
|---|---|
| Feature file | `ddj400_hardware.feature` |
| Scenarios | Browse knob byte convention matches what the mapping assumes; One detent moves the library selection exactly one row; The browse knob is not inverted; A fast browse-knob spin does not run away; The browse knob drives the spoken AccessMenu when it is open; The tempo fader sends an MSB and an LSB; The tempo fader changes the deck's pitch smoothly |
| Tags | `@inference @blocking @midimonitor` |

The byte convention is an inference drawn from the DDJ-FLX4's identical
`0xB6/0x40` binding and from what Mixxx's own `<SelectKnob/>` option
decodes. Nobody has put a MIDI monitor on a DDJ-400 and looked. The
scenario is written to capture the raw bytes rather than the observed
behaviour, because the most likely wrong answer — offset-64 encoding —
produces a knob that appears to work while scrolling one direction only.

**Still untested:** the DDJ-FLX4 itself, so if the inference is wrong on
the DDJ-400 we still will not know whether it was ever right anywhere.
Whether the encoder can emit multi-detent values under a fast spin (the
clamp assumes it might, but that has never been observed). The emulator's
14-bit tempo pair is likewise an unverified reading of the hardware.

---

### PR #69 — issue #49 — "Mixxx ready" held until devices open

| | |
|---|---|
| Feature file | `first_run_boot.feature` |
| Scenarios | Mixxx says it is ready, once, and I can hear it; Mixxx ready arrives after the audio device is open, not before; Mixxx ready is not spoken when the setting is off; Mixxx ready is not repeated when sound hardware is reconfigured; Reloading the skin while running says ready again; A sound device that never opens does not falsely claim readiness; Fixing the device afterwards releases the queued announcement |
| Tags | `@firstrun @blocking @timing` |

**Still untested:** the race where the device opens *during* skin load
rather than after it. Machines slow enough or fast enough to reorder those
two events differently from the test machine. What happens if the device
opens, then fails, then opens again before the skin loads.

---

### PR #70 — issue #52 — error dialogs speak themselves

| | |
|---|---|
| Feature file | `first_run_boot.feature` |
| Scenarios | A broadcasting connection error speaks itself; A recording disk-space warning speaks itself; A recording file failure speaks itself; A controller mapping error speaks itself; A long error message is truncated rather than read forever; Markup in an error message is not read out as markup; Error speech does not replace the screen reader's own reading; Errors during startup, before speech is wired up |

**Still untested:** the encoder-failure dialog (needs a broken encoder
setup, and its title is built by concatenation and contains a double
space — worth a look if anyone ever triggers it). Every other
`ErrorDialogHandler` call site in the codebase; only four are scripted
here. Whether the 300-character truncation lands mid-word in a way that
misleads. The informative-text and details sections are deliberately not
spoken, so any dialog whose meaning lives there is only partially
accessible — the controller mapping error is the known case, but there may
be others nobody has catalogued.

---

### PR #71 — issue #50 — keylock and pitch bindings

| | |
|---|---|
| Feature files | `ddj400_hardware.feature`, `macos_voiceover.feature` |
| Scenarios | Shift + Pad 3 toggles keylock on the pad's own deck; Shift + Pad 3 on the right deck affects deck 2 only; Shift + Pads 4 and 5 shift the pitch by ear; Shift + Pad 6 resets the key to the track's original; Shift + pads 3-6 no longer clear hotcues; Keylock and pitch chords from issue #50 are in the eaten namespace too |

**Still untested:** `sync_key` (`Ctrl+Alt+M`) has no scenario of its own and
no pad equivalent at all — it exists only as a keyboard binding, and
matching one deck's key to the other by ear is hard to write a pass
condition for. The keyboard chords `Ctrl+Alt+K` / `Ctrl+Alt+Up` /
`Ctrl+Alt+Down` / `Ctrl+Alt+R` are only spot-checked, and they sit in the
namespace VoiceOver claims — see the note under PR #77. The choice to put
these on Shift+pads rather than in the Key Shift pad mode was made because
Key Shift's hardware note layout is unknown; nothing here establishes what
that layout actually is.

---

### PR #72 — issue #51 — full track-row readout

| | |
|---|---|
| Feature files | `windows_screenreader.feature`, `library_and_dialogs.feature` |
| Scenarios | Selecting a track speaks its full state, not just its name; Rating is spoken correctly at every value; Played state is spoken correctly; Track colour is read as a hex code, not a colour name; A single-row table omits the position; The row readout only fires when the track table has focus; Navigating cell by cell reads each cell's accessible text; Arrowing through a playlist tells me where I am; A track with no artist announces what it has; Holding the arrow key does not produce a burst of speech; Row readouts can be turned off |

**Still untested:** colours are spoken as hex — `Color #ff0000` — which is
technically correct and practically poor. There is a scenario to capture
what it sounds like, but no scenario asserts a better behaviour, because
none exists yet. Multi-row selections do not produce a row readout at all
and nothing here checks what happens instead. The Hidden Tracks, Missing
Tracks and Recordings views are not covered.

---

### PR #73 — issue #48 — speech batching

| | |
|---|---|
| Feature file | `audio_path.feature` |
| Scenarios | Loading a track with Smart Cue speaks the load AND the cue move; The load announcement is not lost when the cue moves away from a deck; Loading into a playing deck does not disturb the cue or the speech; Load announcements survive back-to-back loads; The AccessMenu speaks one utterance per action, not two |
| Tags | `@audio @blocking @timing` |

The highest-value scenarios in the plan after the browse knob, because the
bug this fixed was invisible to unit tests: the string was generated
correctly and then discarded by the barge-in flush before it was
synthesised.

**Still untested:** only `slotNewTrackLoaded()` is wrapped in a batch. Every
other place where two announcements can fire from one synchronous call
stack still has the original problem, and nobody has enumerated them. The
batch join inserts `". "` between parts, which after a load announcement
that already ends in a full stop yields a double period — audible as a
slightly longer pause on some voices, and not asserted anywhere.

---

### PR #74 — issue #53 — destructive-action confirmations

| | |
|---|---|
| Feature file | `destructive_actions.feature` |
| Scenarios | All 29, in particular: Pressing Enter on the purge dialog does not purge; Pressing Enter on the hide dialog does not hide; Pressing Enter on the delete dialog does not delete; Pressing Escape on the delete dialog does not delete; Repeated Enter presses on a stack of dialogs stay safe |
| Tags | `@destructive @blocking` |

**Known mismatch captured rather than fixed:** the hide and remove
narration says "Yes" and "No", while the dialog that appears uses "Ok" and
"Cancel". The safe default is correct either way, but the spoken
instruction names a button that is not there. There is a scenario that
records this rather than asserting one behaviour, because it is not clear
yet which side should change.

**Still untested:** the "Hiding tracks" playlist warning got a safe default
but no narration, so it depends entirely on a screen reader. Batch
operations over very large selections. What happens if the confirmation is
dismissed by the window manager rather than by a button. The
`Don't ask again during this session` checkbox's effect across a whole
session is only partly covered.

---

### PR #75 — issue #55 — sampler speech

| | |
|---|---|
| Feature file | `audio_path.feature` |
| Scenarios | Loading a sampler is announced with its number and the track; A sampler with no metadata announces only what it has; Sampler transport is announced; Ejecting a playing sampler stays silent; Samplers do not steal the headphone cue |

**Still untested:** samplers beyond the first handful — the paged sampler
submenus exist and the naming is 1-based off an index, so sampler 16 and
above are worth a look but have no scenario. There is no sampler earcon,
so all sampler feedback is speech-only and competes for the speech channel
during a mix; nothing here tests what that feels like with four samplers
firing.

---

### PR #76 — issue #58 — chords off the VoiceOver modifier

| | |
|---|---|
| Feature file | `macos_voiceover.feature` |
| Scenarios | Deck 1 chords reach Mixxx with VoiceOver running; Deck 2 chords reach Mixxx with VoiceOver running; Quantize toggles for both decks; The deck-1 filing pickers open on the new chords; Deck 2's filing pickers use Ctrl+Shift, not Alt+Shift; Old Ctrl+Alt chords no longer do anything in Mixxx (10 rows); The old chords are also gone with VoiceOver switched off; VoiceOver navigation still works inside Mixxx; The VoiceOver rotor opens over the Mixxx window; VoiceOver reads the Mixxx menu bar |
| Tags | `@macos @voiceover @blocking @regression` |

**Still untested:** `vinylcontrol_cueing` on `Ctrl+Alt+Y` / `Ctrl+Alt+U` was
deliberately left in the eaten namespace as out of scope. It is presumably
just as unreachable under VoiceOver, and nothing here confirms or denies
that. VoiceOver's own behaviour varies by macOS version and by whether
"Allow VoiceOver to be controlled with AppleScript" and similar options are
set; only one configuration will be tested.

---

### PR #77 — issue #56 — mixer, EQ, filter and effects bindings

| | |
|---|---|
| Feature files | `keyboard_only.feature`, `macos_voiceover.feature`, `blind_dj_workflow.feature` |
| Scenarios | Deck volume can be set by ear; Deck 2's volume uses the Shift variant; Trim is separate from volume and says so; A control that lands back where it started stays quiet; Every EQ band moves in both directions on deck 1 (6 rows); Every EQ band moves in both directions on deck 2 (6 rows); A bass swap can be performed entirely by keyboard; The filter sweeps in both directions; Effect unit 1 can be switched on and off; Holding the effect unit chord is momentary, not latching; Chain presets cycle and are announced; Effect focus cycles through the slots and names each one; An individual effect slot can be enabled and its effect changed; Effect unit 2 responds on the Shift variants; Effect announcements can be turned off; Do the new mixer chords survive VoiceOver at all (18 rows) |

**A cross-branch problem this plan surfaces rather than solves.** PR #76
moved ten chords out of `Ctrl+Alt` because macOS VoiceOver claims that
modifier. PR #77 then put roughly two dozen new chords into it. Neither
commit mentions the other. If #76's premise is correct, the entire
keyboard-only mixing and effects workflow is unusable on macOS with a
screen reader running — which is the fork's primary user on one of its
three platforms. The `Do the new mixer chords survive VoiceOver at all`
outline exists to settle it with evidence.

**Still untested:** the `_small` fine-step variants of every one of these
controls, which were explicitly out of scope. Effect units 3 and 4. Effect
slots 2, 3 and 4. Whether PowerWindow's momentary behaviour on the two
`enabled` controls is desirable for a keyboard user at all — it is
recorded as expected behaviour here, not evaluated.

---

### PR #78 — issue #57 — AccessMenu on the keyboard

| | |
|---|---|
| Feature files | `keyboard_only.feature`, `macos_voiceover.feature` |
| Scenarios | The menu opens, navigates and closes from the keyboard; I can walk the whole root menu and hear every item; Toggle items announce their current state (3 rows); Fullscreen state stays correct when changed from outside the menu; Disabling keyboard shortcuts warns me first and leaves me a way back; Enabling keyboard shortcuts does not produce the warning; The AccessMenu chords do not fire twice; Value editing from the keyboard; Cancelling a value edit leaves the value alone; The Recording item reports ready as on; The AccessMenu chords are not claimed by VoiceOver (6 rows) |

**Still untested:** the Preferences submenu's thirteen entries are listed in
the code but only the root menu is walked here. The Values submenu is
covered for two items out of four. The key sequences are read from config
once at startup, so a user who rebinds them in a `.kbd.cfg` needs a
restart — that limitation is documented in the source but has no scenario,
because nobody has decided whether it is acceptable. The `Broadcasting`
toggle needs a broadcast setup to exercise meaningfully.

---

### PR #79 — issue #54 — four newly-announced DDJ-400 controls

| | |
|---|---|
| Feature file | `ddj400_hardware.feature` |
| Scenarios | Cycling the tempo range is spoken immediately; The tempo range announcement lands before the next pitch readout; Shift + CUE announces the jump back to the start; Halving and doubling an active loop announces the new size; Loop size announcements survive a fresh loop; Moving effect focus announces the effect by name; Effect focus falls back to a slot number when the name is unknown; Sweeping effect focus quickly does not flood the speech channel |

**Still untested:** two of the four depend on a mapping claim that appears
only in a code comment — that this fork remaps Shift+CUE to `start_stop`
and Shift+SYNC to `rateRange`. The mapping XML for those remaps was not in
the diff. If the presses do nothing, the announcement code may be perfectly
correct and simply never reached. The `Shift + CUE` scenario is tagged
`@inference` for that reason; the tempo range one is not, and probably
should be if it also fails.

The loop-size tracking keeps its own running count seeded from
`loop_enabled` and `beatloop_size`. Nothing tests what happens when that
seed is stale — for example after a loop is set by one route and scaled by
another, repeatedly, across a long set.

---

### PR #80 — issue #60 — menu narration

| | |
|---|---|
| Feature file | `library_and_dialogs.feature` |
| Scenarios | Arrowing through the track context menu speaks every item; Every submenu of the track menu narrates its contents (13 rows); The find-on-web submenu and its per-service submenus narrate; The search-related-tracks submenu narrates; Playlist and crate names are spoken as written; The sampler submenus narrate, including the paged ones; The same item hovered twice is not repeated; Menu items are not given a position in the list; The sidebar playlist context menu narrates; The sidebar crate context menu narrates; The sidebar root context menus narrate |

**Still untested:** hover narration does not bubble from a submenu to its
parent, so each one needed wiring individually — the risk is a submenu that
was missed, and the only way to find it is to walk all of them. The list in
the scenario comes from the diff, so a submenu added later will not be
covered. Menu items whose `text()` still contains an `&` mnemonic marker
are spoken as-is; whether any real item is affected has not been surveyed.
The stem-track submenus need a stem file to reach.

---

### PR #81 — issue #59 — library sort from the keyboard

| | |
|---|---|
| Feature files | `keyboard_only.feature`, `macos_voiceover.feature` |
| Scenarios | Cycling the sort column announces each one; Cycling backwards through the sort columns; Sorting the same column again flips the order; Reversing the sort order without changing column; The column visibility menu opens and can be worked by ear; Sorting by a column with no spoken name stays silent; The library sort chords are not claimed by VoiceOver (4 rows) |

**A probable bug this plan is written to catch.** `[Library],sort_order` is a
plain push button in Push mode with nothing connected to interpret a
press — its raw value *is* the sort direction. A key press sends 1 on key
down and 0 on key up, so `Alt+Shift+O` most likely flips the list to
descending while held and snaps it back to ascending on release, producing
two announcements per press rather than the documented "reverse the current
sort order". The scenario is written to record what actually happens rather
than to assert the documented behaviour.

**Still untested:** the column visibility menu produces no Mixxx speech at
all — its items are read by the platform screen reader — so on a machine
with no screen reader running that feature is entirely inaccessible and the
scenario cannot be completed. Two sort columns share a spoken name
(`location` and `date added` are each produced by two different columns),
which makes the readout ambiguous when cycling; there is no scenario for
it because there is no correct answer to assert.

---

### PR #82 — issue #65 — pad-mode honesty

| | |
|---|---|
| Feature file | `ddj400_hardware.feature` |
| Scenarios | Each pad mode announces the layer it selected (8 rows); A dead pad layer really is dead, exactly as announced; Re-pressing the mode you are already in announces again; The Numark Scratch mode button still announces once, not twice; Pad-mode button note numbers match the mapping (8 rows); Right-deck pad mode buttons use status 0x91 |
| Tags | `@ddj400 @inference @blocking` |

The note numbers are long-standing issue #18 and have never been confirmed
against hardware. A wrong number means a mode announces as the wrong
layer, which is worse than silence — it actively misinforms.

The re-announce fix works by bouncing the control through 0 before writing
the real value. The `pad_mode` control keeps its no-op-ignoring default
because the Numark Scratch mapping fires its mode button on both decks and
depends on the duplicate being dropped; hence the Numark regression
scenario.

**Still untested:** the `loop roll` mode (value 9) is in the vocabulary but
has no DDJ-400 pad-mode button, so it is only reachable from other
mappings and is not covered. Whether the four "not yet supported" layers
could be implemented — the announcement is honest about the current state,
not a plan.

---

### PR #83 — issue #64 — controller navigation without focus

| | |
|---|---|
| Feature file | `keyboard_only.feature` |
| Scenarios | The controller navigation checkbox is reachable and named; The setting takes effect without a restart; Navigating with no focused window does not crash Mixxx; The toggle itself says nothing |

The crash scenario matters more than it looks: the code path previously
dereferenced a null focus window, and it is reached exactly when the
feature is enabled and Mixxx is genuinely unfocused — which is the only
situation in which anyone would turn the feature on.

**Still untested:** the interaction with the `--controller-navigation-without-focus`
command-line flag, which is OR'd with the preference, so with the flag set
unticking the box does nothing. Controllers other than the DDJ-400.
Whether the "live apply" is live enough in practice given the value is
only written on Apply or OK rather than on toggle.

---

### PR #84 — issue #62 — label buddies across ten preferences pages

| | |
|---|---|
| Feature file | `windows_screenreader.feature` |
| Scenarios | Every control on a preferences page announces a useful name (10 rows); Live Broadcasting no longer announces the wrong field names; Decks no longer announces the wrong field for intro start; Controls that were unreachable by Tab are now in the chain; The Effects page glyph buttons say what they do; The Effects page tables are named; Vinyl Control names each deck's controls by deck number; Key Detection has a tab order at all; Unlabelled sliders and spin boxes are named; The Library page names its list and its font controls; The same preferences pages read correctly under JAWS |
| Tags | `@windows @nvda @jaws @blocking` |

The two corrected buddies in Live Broadcasting are the most valuable
scenarios here — a wrong buddy is worse than a missing one, because the
screen reader confidently names the wrong field.

**Still untested:** the preferences pages this PR did not touch. There are
more than ten preferences pages in Mixxx; the ones outside this list have
not been audited at all and may have the same gaps. macOS VoiceOver reading
of these pages is not covered — this file is Windows-only, and Qt's
accessibility bridge differs by platform, so passing under NVDA does not
imply passing under VoiceOver. Orca on Linux is not covered anywhere.

---

### PR #85 — issue #67 — YouTube source

| | |
|---|---|
| Feature file | `library_and_dialogs.feature` |
| Scenarios | The YouTube source is in the sidebar and explains itself; A search reports that it started, and what it found; A search with one result uses the singular; A search with no results says so; Clearing the search box says nothing; Repeating the same search does not re-announce; Search results are readable by ear; An already-downloaded result says so; A short result is announced in seconds only; Downloading a track reports start and progress but not completion; Progress milestones do not repeat or go backwards; A very fast download may skip milestones; Download and search failures are spoken usefully (4 rows); A yt-dlp error message is read rather than swallowed; The downloaded track carries its source in the comment; The on-screen download progress is not spoken; The YouTube source can be hidden |

**Still untested:** everything about this feature depends on an external
binary and an external service, both of which change without warning.
yt-dlp version differences, YouTube's own search-result changes, and
rate-limiting will all produce failures that look like Mixxx bugs. The
licence check that would have restricted downloads to Creative Commons
material is commented out in the source and unreachable. Long downloads,
concurrent downloads beyond the one-at-a-time guard, and network
interruption mid-download have no scenarios.

---

### PR #86 — issue #61 — Auto DJ accessibility

| | |
|---|---|
| Feature files | `keyboard_only.feature`, `blind_dj_workflow.feature` |
| Scenarios | Enabling and disabling Auto DJ is announced; Enabling Auto DJ with an empty queue; Auto DJ transport actions are announced (2 rows); The next-track readout tells me everything I need mid-set; The next-track readout with Auto DJ off and nothing queued; The readout omits the time clause when nothing is playing; The time remaining is time to end of track, not time to the mix; With both decks playing only the first is reported; Adding a random track is silent but effective; Shuffling the queue is also silent; The Auto DJ panel can be tabbed through and escaped; Enter also releases focus from the transition controls |

Note the deliberate wording asymmetry: the toggle says "Auto DJ on", the
on-demand readout says "Auto DJ is on". Easy to mis-transcribe when
recording a result.

**Still untested:** the Auto DJ announcements have no individual preference
checkbox — unlike almost every other category, they fire whenever speech is
on. Whether that is right has not been evaluated. `Ctrl+Shift+F9` and
`Shift+F9` produce no confirmation at all, which is recorded as expected
behaviour here but is arguably a gap. Crate-restricted random track
selection is not covered. `[AutoDJ],tts_next` is not listed in the
controller-mapping table in the guide, so a controller user will not find
it.

---

### PR #87 — issue #66 — eject, talkover, per-channel clipping, xrun earcon

| | |
|---|---|
| Feature file | `audio_path.feature` |
| Scenarios | The xrun earcon cannot be mistaken for the clipping earcon; The xrun announcement obeys the clipping preference; The clipping feedback style applies to the xrun tone too (3 rows); Repeated dropouts do not turn into a continuous alarm; Channel clipping is panned to the deck that is clipping; Main output clipping is still centred and unnamed; One deck clipping does not silence the other deck's warning; The microphone toggle is always spoken; The microphone toggle is spoken even with announcements turned down; Ejecting a playing deck is refused out loud; The eject refusal is spoken even with track-load announcements off; Ejecting a stopped deck is confirmed; Ejecting an empty deck says nothing; Double-pressing eject reloads and announces confusingly |
| Tags | `@audio @blocking @timing` |

**Still untested:** forcing a reliable audio dropout is awkward and
machine-dependent; the scenario says to shrink the buffer and load the CPU,
which is not repeatable across machines. The xrun tone rides entirely on
the clipping preference and has no setting of its own — whether that is
right has not been evaluated, only documented. Only the first microphone
group is watched, so a second or third mic is silent. The eject
double-press edge case is captured as known behaviour rather than fixed.

---

### PR #88 — issue #63 — dialog traps and track info

| | |
|---|---|
| Feature files | `windows_screenreader.feature`, `library_and_dialogs.feature`, `first_run_boot.feature` |
| Scenarios | Every field in the track info dialog announces its label; The colour picker is named; The star rating can be set from the keyboard; Up and Down also change the star rating; The star rating does not announce its new value; The cover art is reachable and operable; The multi-track info dialog is also labelled; Tab is no longer trapped in the key wheel; Up and Down cycle the notation and speak it (7 rows); The notation choice sticks after closing the dialog; The scanner announces itself when it appears; The scanner announcement does not repeat during one scan; A second scan announces again; Per-file progress is not read out; The library scan on first run announces itself |

**Known gap captured, not fixed:** the star rating widget exposes an
accessible *name* but no accessible *value*, and Mixxx's own speech is not
wired to it. Changing a rating from the keyboard is therefore silent, and
the only confirmation is closing the dialog and hearing the row readout.
There is a scenario that records this so it can be fixed deliberately.

**Still untested:** other dialogs in Mixxx with the same Tab-trap shape.
`DlgKeywheel` was found and fixed; nobody has surveyed the rest. The
`Custom notation` mode's 24 per-key edit boxes got buddies in PR #84 but
the interaction between editing them and the key wheel's notation cycling
is not covered.

---

## Six issues merged before this plan existed

PRs #38, #39, #40, #41, #42 and #45 all merged into
`accessibility-improvements-2026-06-25` before this plan's first commit,
closing issues #30, #36, #33, #17, #14 and #32. `README.md`'s original
scope statement named only #47 through #67, so these six had zero manual
scenarios until now. Covered below in PR order, verified against the
actual merged diffs rather than assumed from the issue titles.

### PR #38 — issue #30 — use-after-free crash in AnnouncementManager::speak() during shutdown

| | |
|---|---|
| Feature file | `first_run_boot.feature` |
| Scenarios | Quitting while an announcement is speaking does not crash |
| Tags | `@blocking @regression @timing` |

`AnnouncementManager` held a raw pointer to the `EngineTts` sink (and a
`ControlProxy` on its `[Tts],enabled` control) that outlived the sink,
which `EngineMixer` owns and `CoreServices::finalize()` tore down before
the manager. A control change firing that proxy during shutdown called
`speak()` on freed memory. Fixed by destroying the manager before the
engine in `finalize()`, and by `EngineTts` emitting a `sinkDestroyed()`
signal from its destructor as a defensive backstop.

**Still untested:** this is a narrow race, not a workflow, and a manual
Gherkin scenario is a blunt instrument against it — a single clean exit
is weak evidence the fix holds, and a single crash is strong evidence it
does not, but neither is conclusive. The 21 gtest regression tests
(`Speak_AfterSinkDestroyed_DoesNotCrash` and friends) are the real
guard here; this scenario exists to catch the case they cannot, which is
the fix regressing at the integration level (e.g. a future PR
reintroducing a raw pointer somewhere else in the shutdown path) rather
than the unit level.

### PR #39 — issue #36 — mixer knob/fader readouts default ON

| | |
|---|---|
| Feature file | `audio_path.feature` |
| Scenarios | A fresh profile speaks EQ, filter, volume and the crossfader with nothing configured; Mixer readouts can still be turned off by someone who wants quiet |
| Tags | `@blocking @firstrun` |

**The highest-priority scenario in this whole addendum.** This was the
single BLOCKING finding from the original accessibility audit: a blind
DJ moving a channel fader or an EQ knob heard nothing at all unless they
had already found and ticked "Announce mixer controls" in Preferences,
Accessibility — a setting undiscoverable without sight. `AnnounceMixer`'s
default flipped from `false` to `true`; the readout mechanism itself
(name on touch, debounced value on settle, "announce while moving" still
opt-in) was not otherwise changed. Every scenario elsewhere in this plan
that exercises these controls (`keyboard_only.feature`'s EQ/volume/
filter/crossfader scenarios) explicitly enables `AnnounceMixer` in its
own `Background`, so none of them actually prove the *default* — this PR
section's first scenario is the only one in the plan that does, which is
why it needs a fresh profile (`@firstrun`).

**Correction to the brief this section was written from:** the pitch/
tempo fader is gated by a separate preference, `AnnounceTempo`, which was
already `true` by default before this PR. It is included in the scenario
as a sanity check, not because #36 touched it — #36's actual scope is
volume, trim, the three EQ bands, the filter (QuickEffect super knob) and
the crossfader, all of which share the one `AnnounceMixer` check in
`AnnouncementManager`.

**Still untested:** trim (pregain) is gated by the same `AnnounceMixer`
flag and therefore also defaulted on by this PR, but is not re-verified
against the fresh-profile default here — it is covered with the setting
explicitly enabled in `keyboard_only.feature`'s "Trim is separate from
volume and says so". Decks 3 and 4 are not touched, consistent with the
rest of this plan.

### PR #40 — issue #33 — DDJ-400 auto-configures 2 decks on init

| | |
|---|---|
| Feature file | `ddj400_hardware.feature` |
| Scenarios | Connecting the controller gives me two usable decks (pre-existing, undocumented until now); Deck 2 is playable straight from a fresh connection, no Preferences visit needed |
| Tags | `@regression @blocking` |

`PioneerDDJ400.init()` raises `[App],num_decks` to 2 on connect, using
the same raise-only guard (`if (... < deckCount) { setValue(...) }`)
already used for `num_samplers`, so a blind user's two physical decks
work without ever finding Preferences to configure a deck count by hand.
A scenario for this already existed in `ddj400_hardware.feature`
("carried over from issue #33" per its own comment) but was never listed
in this file, because it predates this plan's #47–67 scope statement.
It proves deck 2 is announced rather than silent; the new scenario goes
further and proves deck 2 is genuinely operable — loadable and playable
— end to end.

**Still untested:** the raise-only guard's actual purpose — that it never
lowers a user's own higher deck count — has no scenario, because there is
no ordinary, documented Preferences control that lets a manual tester set
`num_decks` above 2 first in order to check it is not clobbered back
down. Decks 3 and 4 generally are out of scope for this plan, per the
"What this plan does not test at all" section below.

### PR #41 — issue #17 — track-list sort column/order spoken feedback

| | |
|---|---|
| Feature file | `keyboard_only.feature` |
| Scenarios | Clicking a column header also announces the new sort |
| Tags | none |

**Mostly already covered — see the resolution below before assuming a gap.**
`slotAnnounceSort()` is wired to `[Library],sort_column` and `sort_order`
*changing value*, not to any specific keyboard binding. Issue #59's later
keyboard chords (`Alt+Shift+S`, `Ctrl+Alt+Shift+S`, `Alt+Shift+O`) happen
to change those same controls, and `keyboard_only.feature`'s existing
"Cycling the sort column announces each one" and neighbouring scenarios
already exercise this PR's announcement thoroughly by that route — so
those needed no duplicate. What none of #59's scenarios checked is the
OTHER, pre-existing trigger this PR's own commit message calls out
explicitly: a column-header click, which predates #59 and was never a
keyboard action to begin with. `WTrackTableView::slotSortingChanged()`
(fired by `QHeaderView::sortIndicatorChanged`, i.e. a header click) writes
the same `[Library],sort_column`/`sort_order` control objects directly via
`ControlProxy::set()`, so the one new scenario added here confirms the
announcement fires from that path too, independent of any keyboard chord.

Worth noting: because the header-click path writes `sort_order` directly
from a `Qt::SortOrder` value rather than through a push-button control, it
is a different code path from the one behind `keyboard_only.feature`'s
"LIKELY BUG" scenario for `Alt+Shift+O` — nothing here suggests the
header-click path shares that double-announcement risk, though nobody has
specifically ruled it out either.

**Still untested:** whether a controller-mapped sort trigger (as opposed
to keyboard or mouse) behaves the same way — no controller in this fork
maps anything to the sort controls today, so it is moot for now but would
be worth revisiting if one ever does.

### PR #42 — issue #14 — translatable spoken musical key names

| | |
|---|---|
| Feature file | `audio_path.feature` |
| Scenarios | The on-demand key readout follows the app locale; The load announcement's key name also follows the locale; A locale with no Mixxx translation at all still falls back safely |
| Tags | `@locale`, one also `@regression` |

`keyForSpeech()` in `AnnouncementManager` wraps its 24 fully-spelled key
names ("C Major" … "B Minor") in `tr()`, so a translated build can speak
the key in the user's language. Only Traditional notation (and the
"…and Traditional" variants) is affected; Open Key and Lancelot/Camelot
codes are spoken as a digit plus a phonetic letter and were untouched by
this PR.

**Correction to the brief this section was written from:** as of this
commit, **no shipped translation file contains a translated msgid for any
of these 24 strings** — confirmed by grepping every `res/translations/
mixxx_*.ts` for "Sharp Major" and "Flat Major" and finding zero matches.
These strings are new to `tr()` as of this PR and have not yet been
through a Transifex sync. That means switching to a non-English locale
today will **not** produce a translated key name; Qt's `tr()` falls back
silently to the English source string. The three scenarios here are
written around that fact: the first two record whatever you actually
hear (translated once a translation exists, English until then) rather
than asserting a specific non-English string that cannot currently be
produced, and the third scenario explicitly tests the safe-fallback case
for a locale with no Mixxx translation at all.

**Still untested:** real translated output for any of the 24 strings,
because none exists yet — re-run once a translation lands. Interaction
between locale and "Concise announcements" wording. Open Key and
Lancelot/Camelot notations, which this PR did not touch and are spoken
identically regardless of locale (only the phonetic-letter helper is
locale-independent by construction). Plural forms and non-Latin scripts
are called out generally in "What this plan does not test at all" below
and apply here too.

### PR #45 — issue #32 — value editor added to the spoken AccessMenu

| | |
|---|---|
| Feature file | `ddj400_hardware.feature` |
| Scenarios | Opening and closing the spoken menu from the DDJ-400's BROWSE knob; Holding BROWSE again while the menu is already open does nothing new; Opening the value editor from the DDJ-400's spoken menu; Adjusting two Values settings from the browse knob; Cancelling a value edit from the DDJ-400 leaves the value alone |
| Tags | `@blocking` on two of the five |

**Correction to the brief this section was written from:** by the time
this addendum was written, a keyboard path to the value editor already
existed and was already covered — `keyboard_only.feature`'s "Value
editing from the keyboard" and "Cancelling a value edit leaves the value
alone" scenarios (added later by issue #57/#78's `Alt+Shift+M` chords)
exercise the same `AccessMenuController` value-edit mode via the
keyboard. The brief assumed no keyboard path existed at merge time,
which was true in isolation but stale once #78 landed. So this PR
section is scoped to what genuinely had no coverage anywhere: driving
the value editor from the DDJ-400's own physical controls. That also
surfaced a real, previously undocumented quirk worth its own two
scenarios — the DDJ-400 has no dedicated "close menu" gesture at all;
holding BROWSE only ever calls `openMenu()`, which no-ops if already
open, so closing requires activating "Back" or Shift+BROWSE at the root
level (both call `goBack()`, which closes at the top of the stack), or
waiting out the 30-second timeout.

**Still untested:** the Values submenu's "Speech on/off" boolean item is
not exercised by name here — it is redundant with the root menu's own
"Speech on/off" toggle, which the existing `keyboard_only.feature`
Scenario Outline already covers, so it was not duplicated. Clamping at
the min/max of a value (covered for the keyboard path in
`accessmenucontroller_test.cpp`'s unit tests) is not separately verified
from the DDJ-400's physical knob. The 30-second auto-close timeout itself
is not timed out here — it would make an already-long manual scenario
much longer for a low-risk path.

---

## What this plan does not test at all

Listed so nobody mistakes a green run for full coverage.

**The twelve non-`en_US` keyboard layouts.** Every branch that touched
keyboard config mirrored the `en_US` sequences byte-for-byte into
`cs_CZ`, `da_DK`, `de_CH`, `de_DE`, `el_GR`, `es_ES`, `fi_FI`, `fr_CH`,
`fr_FR`, `it_IT` and `ru_RU` rather than remapping per layout. On AZERTY
and QWERTZ the physical key positions differ, so a chord chosen because it
sat under the left hand on QWERTY may be somewhere else entirely. There is
also a pre-existing collision in `fr_FR` where `beatjump_backward` and
`beatloop_activate` are both bound to `a`. Nothing here tests any of it,
and the maintainer uses `en_US`.

**JAWS in any depth.** Two scenarios are tagged `@jaws` and both amount to
"repeat the NVDA run and note differences". The maintainer does not use
JAWS, so JAWS-specific quirks — its own virtual cursor behaviour, its
handling of Qt's accessibility bridge, its sound card selection — will not
be found by this plan.

**Orca and Linux accessibility generally.** Not covered anywhere. The Linux
build also ships with HID disabled, which is unrelated but means Linux is
the least-exercised platform overall.

**Translations.** Every spoken string goes through `tr()`. Only English is
tested, and the fork's own documentation lists English-only announcements
as a known limitation. Plural forms (`%n track(s)`, `%n star(s)`,
`%n result(s)`) will behave differently in languages with more than two
plural forms, and nothing here would notice.

**Decks 3 and 4.** Almost every scenario uses decks 1 and 2. Four-deck
setups are a different mental model and a different set of announcements.

**The Numark Scratch beyond one regression check.** It is a supported
controller with its own accessibility layer documented in the guide, and it
gets a single scenario here to guard the `pad_mode` deduplication.

**Timecode vinyl.** The DVS surface has its own documented keyboard chords
and its own spoken mode changes. None of the 21 PRs touched it, so it is
out of scope, but it is also therefore unverified by anything.

**Recording and broadcasting as working features.** They appear here only
as error sources and as AccessMenu toggles. Nothing tests a completed
recording or a successful broadcast.

**Long-running stability.** No scenario runs for more than a few minutes.
A four-hour set, memory growth in the announcement path, TTS engine
behaviour after thousands of utterances — all unknown.

**Announcement configuration permutations.** The plan mostly assumes
"Speak deck names as numbers" on and "Concise announcements" off. The
phonetic deck names, the concise phrasings, the fraction-versus-percentage
mixer readouts and the three fraction-detail settings each produce
different strings and are essentially untested.

**Windows N editions.** The build machine turned out to be a Windows N
edition missing Media Foundation, which broke test execution entirely.
Whether a user on an N edition can run the shipped build at all is a
packaging question nobody here answers.

**Anything about how it feels.** The scenarios can establish that an
announcement is correct and audible. They cannot establish that it arrives
soon enough to act on, that the phrasing is what you would want at minute
90 of a set, or that the whole thing is less tiring than the alternative.
The end-to-end run in `blind_dj_workflow.feature` is the closest this plan
gets, and it is one person's opinion recorded as a stopwatch time and a
note about where they got lost.

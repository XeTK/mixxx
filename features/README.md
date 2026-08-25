# Manual test plan — accessibility work (issues #47-#67)

Gherkin scenarios for the parts of this fork's accessibility work that
**CI cannot verify**. Unit tests already cover the string formatting, the
control plumbing, and the debounce arithmetic. What they cannot cover is
whether a real DDJ-400 sends the byte we guessed it sends, whether
VoiceOver swallows a chord before Mixxx ever sees it, whether the speech
you needed actually reached your ears in the two seconds you had, and
whether a blind DJ can get from "app launched" to "track mixed and filed"
without once asking a sighted person what is on screen.

These are **manual** scenarios. No runner executes them. There is no step
definition layer and none is planned — the Gherkin is here because it is a
good format for a checklist a human works through: numbered, tagged,
filterable, and unambiguous about what counts as a pass.

## The scenarios are written for a tester who cannot see the screen

Every `Then` is something you can **hear** or something your **screen
reader speaks**. There are no steps of the form "the dialog shows X" or
"the button is highlighted". If a scenario cannot be checked without
sight, it does not belong in this plan and should be rewritten or dropped.

Where an exact spoken string matters, it is quoted verbatim from the
source. A near-miss is a fail: "Deck 1 keylock on" and "Keylock on deck 1"
are different results, and the difference is usually a real regression in
whichever branch reworded it.

## Files

| File | Area | Issues covered |
|---|---|---|
| `ddj400_hardware.feature` | Real DDJ-400 over MIDI | 47, 50, 54, 65, plus long-standing 18 |
| `macos_voiceover.feature` | Chords surviving VoiceOver | 58, and 56 / 50 as collateral |
| `windows_screenreader.feature` | JAWS/NVDA reading the UI | 62, 63, 51 |
| `audio_path.feature` | Things only a listener can confirm | 48, 55, 66, plus beat click / split cue / ducking |
| `first_run_boot.feature` | Cold start and device errors | 49, 52, 63 |
| `keyboard_only.feature` | Full workflow, no controller | 56, 57, 59, 61, 64 |
| `destructive_actions.feature` | Confirmations before data loss | 53 |
| `library_and_dialogs.feature` | Library narration, dialogs, YouTube | 51, 60, 63, 67 |
| `blind_dj_workflow.feature` | End-to-end acceptance run | all of them, in anger |

244 scenarios as written; 350 individual runs once `Scenario Outline`
examples are expanded.

`COVERAGE.md` maps each of the 21 pull requests to the scenarios that
touch it, and — more usefully — states what is still untested afterwards.

## If you only have an hour, run these five

Ordered by how likely they are to reveal a real problem, not by how
important the feature is.

1. **`ddj400_hardware.feature` → "Browse knob byte convention matches what
   the mapping assumes"** (`@inference @blocking @midimonitor`). The one
   thing in the fork whose result genuinely cannot be predicted. Needs a
   MIDI monitor and five minutes.
2. **`macos_voiceover.feature` → "Do the new mixer chords survive VoiceOver
   at all"** (`@inference @blocking`). Two PRs contradict each other about
   the `Ctrl+Alt` namespace. If VoiceOver eats these, the whole
   keyboard-only mixing workflow is dead on macOS.
3. **`audio_path.feature` → "Loading a track with Smart Cue speaks the load
   AND the cue move"** (`@blocking @timing`). The bug this fixed was
   invisible to unit tests because the string was generated correctly and
   then thrown away before it was synthesised.
4. **`ddj400_hardware.feature` → "Pad-mode button note numbers match the
   mapping"** (`@inference @blocking @midimonitor`). Eight note numbers
   nobody has ever confirmed. A wrong one announces the wrong layer, which
   is worse than saying nothing.
5. **`destructive_actions.feature` → "Pressing Enter on the purge dialog
   does not purge"** (`@destructive @blocking`). Purge had no confirmation
   at all until recently. Cheap to run, and the failure mode is losing
   library entries.

## Tags

Filter a session down to what you can actually run today. A scenario
carries every tag that applies, so `@hardware @ddj400 @blocking` means you
need the controller plugged in and the result gates the release.

**Equipment / platform**

| Tag | You need |
|---|---|
| `@hardware` | Physical hardware beyond the computer |
| `@ddj400` | A Pioneer DDJ-400, USB-connected, powered |
| `@midimonitor` | A MIDI monitoring tool alongside Mixxx (see below) |
| `@macos` | macOS build |
| `@windows` | Windows build |
| `@linux` | Linux build |
| `@voiceover` | macOS VoiceOver running (Cmd+F5) |
| `@jaws` | JAWS running |
| `@nvda` | NVDA running |
| `@audio` | Headphones and a working audio interface; a quiet room |
| `@twodevices` | Two separate audio outputs (main + headphones) |

**Nature of the test**

| Tag | Meaning |
|---|---|
| `@manual` | On every scenario in this plan. Nothing here is automated. |
| `@blocking` | Do not ship if this fails. |
| `@inference` | **Tests an assumption, not a known-good behaviour.** The implementation was written from a sibling mapping, a datasheet, or a reasonable guess — nobody has ever seen it work. Read the comment above the scenario before running. |
| `@regression` | Checks that something that used to work still works. |
| `@keyboard` | Keyboard only; unplug the controller first. |
| `@firstrun` | Requires a clean profile (see below). |
| `@destructive` | Can delete tracks or files. Use a throwaway library. |
| `@e2e` | Long end-to-end run, not a single-behaviour check. |
| `@timing` | Outcome depends on when things happen, not just what. Run it more than once. |

Useful selections:

- `@blocking` — the minimum bar for a release.
- `@inference` — the highest-value hour you can spend. These are the
  scenarios most likely to fail.
- `@ddj400 and not @midimonitor` — what you can do with just the
  controller.
- `not @hardware and not @voiceover and not @jaws and not @nvda` — what
  you can do on a plain laptop with headphones.

## Setting up

### A throwaway profile

Several scenarios (`@firstrun`, `@destructive`) need a Mixxx settings
directory you do not mind losing. Do not test against your real library.

- macOS: `~/Library/Application Support/Mixxx`
- Windows: `%LOCALAPPDATA%\Mixxx`
- Linux: `~/.mixxx`

Move the real one aside, run the test, move it back. Mixxx recreates a
fresh one on launch, which is exactly what `@firstrun` is testing.

You also want a small throwaway music folder — a dozen tracks, at least
one of which is analysed at the wrong tempo (a drum-and-bass or footwork
track usually is), and at least one with an accented or non-Latin title.

### MIDI monitoring (`@midimonitor`)

Some DDJ-400 scenarios need you to see the raw bytes the controller sends,
independently of what Mixxx decides they mean. That is the whole point:
the scenario is checking that Mixxx's interpretation matches the hardware,
so reading Mixxx's interpretation back proves nothing.

- macOS: **MIDI Monitor** (free, snoize.com), or `receivemidi` from the
  `midi-tools` Homebrew formula for a scriptable text log.
- Windows: **MIDI-OX**.
- Linux: `amidi -d -p hw:1` or `aseqdump -p "DDJ-400"`.

Mixxx will normally hold the DDJ-400 port exclusively. Either close Mixxx
while monitoring, or use a virtual port / the OS's multi-client MIDI
support. Where a scenario needs Mixxx *and* the monitor at once it says
so.

Mixxx's own **Controller Debug** output is a useful cross-check but is
**not** a substitute — it shows the bytes after Mixxx's own parsing. Enable
it by launching with `mixxx --controllerDebug` and watch the log.

### Screen readers

VoiceOver, JAWS and NVDA each speak through their own audio device, which
is usually the system default. Before an `@audio` run, make sure the
screen reader and Mixxx's own speech are not fighting for the same ears —
scenarios that care about this say which device should be which.

## Recording results

Keep a run log. A plain text file is fine; a spreadsheet is easier to diff
between builds. Per scenario record:

1. Scenario name and file.
2. Build under test — `git rev-parse --short HEAD`, plus OS and version.
3. Equipment — controller firmware version, audio interface, screen reader
   and version, TTS voice and rate.
4. **Pass / Fail / Blocked / Not-run.** "Blocked" means you could not run
   it (no hardware, prerequisite scenario failed); it is not a pass.
5. On fail: **what you actually heard**, verbatim, versus what the scenario
   said you should hear. This is the single most valuable thing in the
   log. "Announcement wrong" is not actionable; "said 'Keylock enabled'
   instead of 'Deck 1 keylock on'" is a one-line fix.
6. On fail with hardware: the raw MIDI bytes from the monitor.

A scenario that passed on a previous build and fails now is a regression
and outranks a scenario that has never passed.

## When a scenario fails

Triage in this order.

**1. Is it the test or the code?** These scenarios quote strings that were
read out of the source. If the code deliberately changed the wording after
this plan was written, the plan is wrong — fix the `.feature` file in the
same PR as the wording change, and say so in the commit. A stale test plan
that cries wolf gets ignored, which is worse than having no plan.

**2. Is it an `@inference` scenario?** Then a failure is the expected
outcome roughly as often as not. It is not a regression — it is the
assumption being wrong, which is what you were checking. Capture the real
behaviour (raw MIDI bytes, the actual note numbers) and file it as a
finding with that evidence, not as a bug report saying "browse knob
doesn't work". The evidence is the deliverable.

**3. Is speech missing entirely, or just wrong?** Missing speech has a
short list of causes that are not the feature under test: speech toggled
off (`Alt+Shift+A`), the category's checkbox unticked in Preferences →
Accessibility, "Speech output" routed to a device you are not listening
to, or the TTS engine failing to produce audio at all. Rule those out with
a known-good announcement (`Alt+1` reads deck 1's status) before blaming
the scenario's feature.

**4. Is it timing-dependent?** Anything tagged `@timing` — speech batching,
"Mixxx ready", the ducking envelope — can pass or fail depending on
machine load and on how fast you pressed the keys. Run it three times. Two
of three failing is a bug; one of three is a note in the log with the
conditions attached.

**5. Does it reproduce without the screen reader?** If a chord fails under
VoiceOver, retry it with VoiceOver off. Working without and failing with
means the screen reader is intercepting it — that is issue #58's whole
subject and the finding belongs there. Failing both ways is a Mixxx bug.

**6. File it.** One issue per finding, on gitea.xetk.co.uk/xetk/mixxx.
Title it with the behaviour, not the scenario number — scenario numbering
will drift, behaviour descriptions will not. Include: scenario name, build
hash, equipment, expected string, heard string, and whether it reproduces
without a screen reader.

## Keeping this plan honest

The strings in these scenarios were transcribed from the source at the
time of writing. They will rot. When you change a spoken string, a key
chord, or a MIDI note number, grep this directory for it:

```
grep -rn "the string you changed" features/
```

and update it in the same commit. `COVERAGE.md` should be updated when a
PR adds behaviour that nothing here covers — including, deliberately,
adding it to the "still untested" list rather than writing a scenario that
cannot really be run.

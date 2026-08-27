# Spec 08 — Input layer: keyboard bindings and controller mappings

**Status:** Draft for review
**Branch:** `spec-controller`, based on `accessibility-improvements-2026-06-25` @ `2390edf423`
**Owner:** accessibility fork
**Related:** Spec 01 (DDJ-400 spoken menu), Spec 03 (DDJ-400 emulator), Spec 04
(E2E testing), Spec 09 (library / UI accessibility surface)

## Goal

Document the two ways a blind DJ physically drives Mixxx on this fork — the
**keyboard mappings** (`res/keyboard/*.kbd.cfg`) and the **controller
mappings** (`res/controllers/`) — as a contract that the upstream-2.6 rebase
must preserve. This is a specification of *what is true on this branch today*,
plus the invariants that make it work.

It also records an **unresolved conflict** over the `Ctrl+Alt` modifier
namespace between three in-flight PRs (§5). That section is a decision record,
not a decision.

---

## 1. Background / current state (verified on this branch)

### 1.1 Keyboard

| Fact | Evidence |
|---|---|
| 12 locale files ship | `res/keyboard/{cs_CZ,da_DK,de_CH,de_DE,el_GR,en_US,es_ES,fi_FI,fr_CH,fr_FR,it_IT,ru_RU}.kbd.cfg` |
| Fork adds exactly 34 lines to each | `git diff 0e0589c751..HEAD -- res/keyboard/` → `12 files changed, 408 insertions(+)`, 34 per file |
| The 34 added lines are **byte-identical** in all 12 | md5 of the sorted added-line set is `a4b9d27f…` for every file |
| Nothing has touched them since `ac931df0ed` | `git diff --stat ac931df0ed..HEAD -- res/keyboard/` is empty |

### 1.2 Controllers

| File | Fork delta | Role |
|---|---|---|
| `res/controllers/Pioneer-DDJ-400-script.js` | +219 | Primary accessibility controller: spoken menu entry, accessibility pad layer, jog safety, layer feedback |
| `res/controllers/Pioneer-DDJ-400.midi.xml` | +354 / −(rewrites) | Script-Binding conversions, `<settings>` block, pad-mode announce bindings |
| `res/controllers/Numark-Scratch-scripts.js` | +69 | Accessibility shift layer (secondary controller) |
| `res/controllers/Numark-Scratch.midi.xml` | +2/−2 | `<description>` documenting the shift layer |

`git diff --stat 0e0589c751..HEAD -- res/controllers/` also shows Traktor S2
MK3, Traktor MX2, Yaeltex MiniMixxx and `mixxx-controls.d.ts` changes; those
are **upstream churn carried across the fork point**, not accessibility work,
and are out of scope for this spec.

---

## 2. Keyboard binding layer

### 2.1 How bindings are loaded

`CoreServices::initializeKeyboard()` — `src/coreservices.cpp:825-872`.

| Step | Line | Behaviour |
|---|---|---|
| Default `[Keyboard],Enabled` to 1 if unset | `830-832` | First run enables shortcuts |
| Prefer `Custom.kbd.cfg` from the settings dir | `836`, `841-843` | User override wins over every shipped locale file |
| Otherwise pick `<locale>.kbd.cfg` from `inputLocale()` | `846-851` | Locale comes from the **input method / keyboard layout**, not the UI language |
| Fall back to `en_US.kbd.cfg`, then to no bindings at all | `854-861` | Missing locale file is non-fatal |
| Build a second, **empty** `ConfigObject<ConfigValueKbd>` | `839` | `m_pKbdConfigEmpty` — the "shortcuts off" config |
| Construct `KeyboardEventFilter` with whichever config `[Keyboard],Enabled` selects | `868-871` | The gate |

Toggling at runtime is `CoreServices::slotOptionsKeyboard()`
(`src/coreservices.cpp:874-884`): it swaps the filter's config pointer between
`m_pKbdConfig` and `m_pKbdConfigEmpty` and writes `[Keyboard],Enabled`.

**Consequence:** when a user hits `Ctrl+`` ` (`OptionsMenu_EnableShortcuts`),
**every accessibility binding in the table below stops working at once** —
readouts, quick-add, BPM halve/double, quantize. The only exception is the
`Qt::ApplicationShortcut` escape hatch in §2.5.

Key sequences are matched by `KeyboardEventFilter::getKeySeq()`
(`src/controllers/keyboard/keyboardeventfilter.cpp:150-178`), which composes a
`QKeySequence` string from the individual Qt modifier flags
(`Shift+`/`Ctrl+`/`Alt+`/`Meta+`) rather than from `e->modifiers()` wholesale.
This matters for §5.4.

### 2.2 The 34 fork-added bindings (en_US, identical in all 12)

#### Category A — global quick toggles

| Binding | Group, control | Effect |
|---|---|---|
| `Alt+X` | `[Master],crossfader_lock` | Lock the crossfader so a stray knock can't kill the mix |
| `Alt+S` | `[Master],headSplitDecks` | Split headphone cue |
| `Alt+J` | `[Master],disable_touch_scratch` | Jog touch lock |
| `Alt+B` | `[BeatClick],enabled` | Beat-click metronome |
| `Alt+Shift+A` | `[Tts],enabled` | Speech on/off — **also a `QAction`, see §2.5** |
| `Alt+Shift+R` | `[Tts],repeat` | Repeat last announcement |

`crossfader_lock` and `disable_touch_scratch` are fork-added engine controls
(`src/engine/enginemixer.cpp:111`, `:135`), announced by
`src/util/announcementmanager.cpp:536` and `:712`.

#### Category B — per-deck on-demand readouts (the odd/even convention)

The digit row is split **odd digit = deck 1, even digit = deck 2**, so the two
decks' readouts sit next to each other under the same finger:

| Readout | Deck 1 | Deck 2 | Handler |
|---|---|---|---|
| Status (playing/stopped, time, BPM, pitch) | `Alt+1` | `Alt+2` | `formatDeckStatus` |
| Time remaining | `Alt+3` | `Alt+4` | `formatTimeRemaining` |
| BPM | `Alt+5` | `Alt+6` | `formatBpm` |
| Musical key | `Alt+7` | `Alt+8` | `formatKey` |
| Bar position | `Alt+9` | `Alt+0` | `formatBarPosition` |
| Track name | `Alt+Shift+T` | `Alt+Shift+Y` | `formatTrackName` |

Controls are `[ChannelN],tts_status` / `tts_time` / `tts_bpm` / `tts_key` /
`tts_bar` / `tts_track`, dispatched from a table at
`src/util/announcementmanager.cpp:986-991`. Semantics are Spec 06's territory;
this spec owns only the binding.

Note the convention breaks at the wrap: `Alt+9` is deck 1's bar position but
`Alt+0` is deck 2's, because 0 sits to the right of 9 on the number row. That
is deliberate — the pairing is *positional*, not arithmetic.

Track name escapes the digit row entirely (`Alt+Shift+T`/`Y`), because the
digits were exhausted at five readouts per deck.

#### Category C — quick add to crate / playlist

| Binding | Group, control | Target |
|---|---|---|
| `Alt+Shift+C` | `[Library],AddToCrate` | The **selected** library track |
| `Alt+Shift+P` | `[Library],AddToPlaylist` | The **selected** library track |
| `Ctrl+Alt+C` | `[Channel1],quick_add_to_crate` | The track **loaded in deck 1** |
| `Ctrl+Alt+P` | `[Channel1],quick_add_to_playlist` | The track **loaded in deck 1** |
| `Ctrl+Alt+Shift+C` / `+P` | `[Channel2],quick_add_to_crate` / `_playlist` | Deck 2 |

See Spec 09 §2 for the control objects and the spoken picker behind these.

#### Category D — BPM grid and quantize

| Binding | Deck 1 | Deck 2 | Purpose |
|---|---|---|---|
| `beats_set_halve` | `Ctrl+Alt+H` | `Ctrl+Alt+Shift+H` | Fix a double-tempo analysis by ear |
| `beats_set_double` | `Ctrl+Alt+D` | `Ctrl+Alt+Shift+D` | Fix a half-tempo analysis by ear |
| `quantize` | `Ctrl+Alt+Q` | `Ctrl+Alt+Shift+Q` | First keyboard binding quantize has ever had in a shipped layout |

Categories C (per-deck half) and D are the **10 bindings that sit in the
`Ctrl+Alt` namespace** and are the subject of §5.

### 2.3 Why the a11y block is deliberately NOT localised

All 12 files receive the same 34 bytes-for-bytes-identical lines. The upstream
bindings around them *are* localised (deck play is `d`/`l` on `en_US`, and
moves on AZERTY/QWERTZ layouts). The fork's block does not move.

| Reason | Detail |
|---|---|
| **Muscle memory is the interface** | A blind DJ cannot look at a cheat sheet mid-set. `Alt+5` must be "deck 1 BPM" on every machine they touch, including a borrowed one with a different system locale. |
| **The locale is chosen by keyboard layout, not by user intent** | `initializeKeyboard()` derives the file from `inputLocale()` (`coreservices.cpp:846`). Plugging in a different keyboard, or a locale-misdetecting OS, silently swaps the whole mapping file. Identical a11y blocks make that swap invisible for accessibility features. |
| **The documentation is single-sourced** | `ACCESSIBILITY.md`, `ACCESSIBILITY_QUICK_REFERENCE.md`, `BETA_README.md` and the DDJ-400 mapping `<description>` all quote one set of key names. Twelve variants would need twelve doc sets. |
| **It makes drift mechanically detectable** | The blocks either hash identically or they do not. See §6 (recommended lint). |

**Invariant K1 — locale-invariance.** The fork's accessibility bindings are
byte-identical across all 12 `res/keyboard/*.kbd.cfg` files. Any change must be
applied to all 12 in the same commit. A rebase that resolves a conflict in only
one locale file silently breaks every user on the other 11.

**Cost accepted:** the block is chosen for US-QWERTY ergonomics and takes no
account of what those physical positions are on AZERTY/QWERTZ, nor of what
characters `Ctrl+Alt+<letter>` produces on layouts where `Ctrl+Alt` is
**AltGr** (§5.4). This is a known, deliberate trade.

### 2.4 What gates the bindings

| Gate | Effect on the 34 bindings |
|---|---|
| `[Keyboard],Enabled = 0` (`Ctrl+`` `) | All 34 dead |
| A modal dialog has focus | `KeyboardEventFilter` is installed application-wide, but Qt delivers to the focused window; dialog-local handling wins |
| `Custom.kbd.cfg` exists in the settings dir | The shipped file is **not read at all** — a user with an old `Custom.kbd.cfg` from before the fork gets **zero** accessibility bindings (`coreservices.cpp:841-843`) |

That last row is a real trap: it is silent, and the user's symptom is "the
accessibility keys just don't work", with nothing in the UI to explain it.

### 2.5 The `Qt::ApplicationShortcut` escape hatch

`Alt+Shift+A` (speech on/off) is bound **twice**, on purpose:

1. As a normal `.kbd.cfg` line: `[Tts] enabled Alt+Shift+a`.
2. As a `QAction` in `WMainMenuBar::createOptionsMenu()` —
   `src/widget/wmainmenubar.cpp:524-552`:

```cpp
pOptionsTts->setShortcut(
        safeKeySequence(m_pKbdConfig->getValue(
                ConfigKey("[Tts]", "enabled"),
                QStringLiteral("Alt+Shift+A"))));
pOptionsTts->setShortcutContext(Qt::ApplicationShortcut);
```

The `QAction` reads its key **from the same kbd config**, so the two can never
disagree, and the in-source default `"Alt+Shift+A"` is the last-resort fallback
if the config has no entry.

| Property | Why it matters |
|---|---|
| Qt's shortcut map consumes the key press **before** `KeyboardEventFilter` sees it | No double-toggle; the two bindings are mutually exclusive at runtime, not additive |
| `Qt::ApplicationShortcut` scope | Fires regardless of which window/widget in the application has focus — including modal dialogs |
| It is a `QAction`, not a kbd.cfg line | Survives `[Keyboard],Enabled = 0` |
| It is in the menu | Discoverable by screen-reader menu traversal; the menu item advertises its own shortcut |

`WMainMenuBar` deliberately bridges to the control **via a signal**
(`WMainMenuBar::toggleTts`, connected at `wmainmenubar.cpp:546`) rather than a
`ControlProxy`, because the menu bar is constructed before `CoreServices`
creates the engine; the connection to `[Tts],enabled` is made later in
`MixxxMainWindow::connectMenuBar()`.

**Invariant K2 — the always-works pattern.** Anything that must work when
shortcuts are disabled, when a modal dialog has focus, or when the skin is in a
broken state, must be a `QAction` with `Qt::ApplicationShortcut` in
`wmainmenubar.cpp`, reading its key from `m_pKbdConfig` with an in-source
default. A `.kbd.cfg` line alone is not sufficient. Speech-toggle is the
canonical case: if speech is off *and* the toggle is gated behind the thing
that turned it off, the user is locked out with no feedback channel.

Twenty-odd other menu actions already use `setShortcutContext(Qt::ApplicationShortcut)`
(`wmainmenubar.cpp:137, 160, 210, 223, 360, 405, 441, 469, 489, 513, 536, 563,
586, 599, 619, 639, 661`), so this is an established pattern, not a one-off.

---

## 3. Controller mapping layer — Pioneer DDJ-400

Spec 01 owns the spoken-menu **interaction design**; Spec 03 owns the
**emulator**. This section owns the mapping itself and its invariants.

### 3.1 The `[Tts]` layer-feedback controls

These exist because the DDJ-400 (and the Numark Scratch) change their pads'
behaviour **inside the hardware**, with only an LED to say so. A blind DJ
otherwise has no way to know which layer they are in.

| Control | Type | Set by | Spoken as |
|---|---|---|---|
| `[Tts],shift` | `ControlPushButton` | `PioneerDDJ400.shiftPressed` (`script.js:616-623`), `NumarkScratch.shift/unshift` | "Shift" — **on the press only**; release is silent (`announcementmanager.cpp:795-807`) |
| `[Tts],pad_mode` | `ControlObject` | `PioneerDDJ400.padModePressed` (`script.js:630-647`), `NumarkScratch.setMode` | "Pads, <mode>" (`announcementmanager.cpp:809-854`) |
| `[Tts],repeat` | trigger | Accessibility pad 7, Numark Shift+Echo | Repeats last announcement |

`[Tts],shift` is OR-ed across both decks' shift buttons
(`script.js:621-622`), so holding both or rolling between them stays quiet.

`[Tts],pad_mode` is a **fixed cross-controller vocabulary of 9 values**, not 8:

| Value | Spoken | DDJ-400 mode button (`control` byte) | Numark Scratch |
|---|---|---|---|
| 1 | "hot cues" | `0x1B` | HOTCUE |
| 2 | "beat loop" | `0x6D` | — |
| 3 | "beat jump" | `0x20` | — |
| 4 | "sampler" | `0x22` | SAMPLER |
| 5 | "keyboard" | `0x69` | — |
| 6 | "pad effects 1" | `0x1E` | — |
| 7 | "pad effects 2" | `0x6B` | — |
| 8 | "key shift" | `0x6F` | — |
| 9 | "loop roll" | — | ROLL |

Value 9 (`loop roll`) was added for the Numark Scratch. Any value outside 1-9
is a silent no-op (`default: return;` at `announcementmanager.cpp:849-851`).

**Invariant C1 — same-value writes are silent.** `[Tts],pad_mode` only speaks
on a `valueChanged`, so re-pressing the current mode button says nothing, and
hardware that fires one mode press per deck (the Numark Scratch's single mode
button reaches both `[Channel1]` and `[Channel2]` `PadSection` instances) is
deduplicated for free. Mapping code relies on this — see the comment at
`Numark-Scratch-scripts.js` `setMode`. Converting `pad_mode` to a
`ControlPushButton` or adding a forced re-emit would produce double speech.

### 3.2 Mapping settings (`<settings>` block, `Pioneer-DDJ-400.midi.xml`)

The fork added an `<settings><group label="Accessibility">` block. All three
options are read once at script load
(`Pioneer-DDJ-400-script.js:807`, `812-817`, `830`).

| Option | Type | Default | Effect |
|---|---|---|---|
| `accessibilityPads` | boolean | `false` | Opt-in accessibility pad layer (§3.3) |
| `disableJogScratch` | boolean | `false` | Sets `PioneerDDJ400.vinylMode = false` — platter touch becomes a no-op so a stray hand can't stop or scratch playback. Rotation still nudges pitch; Shift+jog still seeks. |
| `jogSensitivity` | real, 0.1–5.0, step 0.1 | `1.0` | Scales pitch-bend nudge (`script.js:596`), Shift+jog seek rate (`:606`) and scratch response (`:594`) |

The last two are the **jog-safety** settings. They exist because the DDJ-400's
capacitive platter top is the single largest accidental-input hazard for a DJ
who cannot see where their hands are.

### 3.3 The accessibility pad layer (opt-in)

When `accessibilityPads` is on, the eight **Hot Cue mode** pads are re-purposed
(`PioneerDDJ400.hotcuePad`, `script.js:823-841`; `hotcuePadShift`, `:843-863`):

| Pad | Unshifted | Shifted |
|---|---|---|
| 1 | `[ChannelN],tts_status` | `beats_set_halve` |
| 2 | `[ChannelN],tts_time` | `beats_set_double` |
| 3 | `[ChannelN],tts_bpm` | *(deliberately nothing)* |
| 4 | `[ChannelN],tts_key` | *(deliberately nothing)* |
| 5 | `[ChannelN],tts_bar` | *(deliberately nothing)* |
| 6 | `[ChannelN],tts_track` | *(deliberately nothing)* |
| 7 | `[Tts],repeat` | `[Master],headSplitDecks` |
| 8 | `[BeatClick],enabled` toggle | `[Tts],enabled` toggle |

Shift + pads 3–6 are intentionally inert so a stray press cannot clear hot cues
the DJ cannot see (`script.js:861-862`).

**Invariant C2 — the pad layer is opt-in and destructive of hot cues.** While
`accessibilityPads` is on, hot cues cannot be set, triggered, or cleared from
the pads at all — the early-return at `script.js:824-827` and `:844-847` is the
whole gate. This is a real functional sacrifice and is why the default is
`false` and the setting carries a `<description>` saying so. It must not be
flipped to default-on during a rebase.

Every action confirms itself out loud, so no LED feedback is needed
(`script.js:805`).

### 3.4 Browse knob and spoken-menu entry

The mapping converts five browse/load bindings from declarative MIDI options to
`<Script-Binding/>` so the script can branch on whether the spoken menu is open:

| Control | MIDI | Before (upstream) | After (fork) |
|---|---|---|---|
| BROWSE rotate | CC `0xB6` / `0x40` | `<SelectKnob/>` → `[Library],MoveVertical` | `PioneerDDJ400.browseRotate` |
| BROWSE press | Note `0x96` / `0x41` | `[Library],MoveFocusForward` | `PioneerDDJ400.browsePress` |
| BROWSE + SHIFT | Note `0x96` / `0x42` | `[Library],MoveFocusBackward` | `PioneerDDJ400.browseShiftPress` |
| LOAD Deck 1 | Note `0x96` / `0x46` | `[Channel1],LoadSelectedTrack` | `PioneerDDJ400.loadDeck1` |
| LOAD Deck 2 | Note `0x96` / `0x47` | `[Channel2],LoadSelectedTrack` | `PioneerDDJ400.loadDeck2` |

Each script handler checks `browseMenuActive()` (`[AccessMenu],active == 1`)
and routes to `[AccessMenu]` when open, or the original control when closed
(`script.js:227-299`). Hold threshold is 0.4 s (`script.js:215`), implemented
with `engine.beginTimer` started on press-down and cancelled on press-up.

The mapping also auto-raises deck count on init:
`if (engine.getValue("[App]", "num_decks") < deckCount) engine.setValue(...)`
(`script.js:176-177`).

### 3.5 Relative-encoder decoding — Invariant C3

**This was a real, shipped bug.** Converting the browse knob from
`<SelectKnob/>` to `<Script-Binding/>` (§3.4) silently discarded the relative
decode that `midicontroller.cpp` performs for `<SelectKnob/>`. Two successive
fixes were needed:

| Stage | Code | Result |
|---|---|---|
| Original fork conversion | `engine.setValue("[Library]", "MoveVertical", value)` — raw MIDI byte | One detent scrolled the track list by up to 127 rows |
| First fix (`acddc3333f`) | `const delta = value - 0x40` | Assumed an **offset-64** encoding (`0x41` = up, `0x3F` = down). Wrong convention: the hardware sends 7-bit two's complement, so `0x01` (one detent up) decoded as `−63` |
| **Shipped fix (issue #47, PR #68, merged)** | `Pioneer-DDJ-400-script.js:227-253` | Two's-complement decode **and** clamp to ±1 |

Current, correct implementation:

```js
let delta = value;
if (delta >= 64) { delta -= 128; }
if (delta === 0) { return; }
delta = delta > 0 ? 1 : -1;   // clamp defensively
```

`0x01`–`0x3F` are positive detents (1..63), `0x7F`–`0x40` are negative
(−1..−64) — mirroring exactly what `midicontroller.cpp` does for
`<SelectKnob/>`.

**Invariant C3 — one detent, one step.** A relative encoder bound through a
`<Script-Binding/>` must decode the device's byte convention **in the script**
and emit exactly `±1` per detent, regardless of convention. The clamp is not
belt-and-braces: for a blind user a runaway jump through the track list or the
spoken menu is unrecoverable, because there is no visual anchor to jump back
to. Whenever a control moves from a declarative MIDI option (`<SelectKnob/>`,
`<Rot64/>`, `<Rot64Inv/>`, …) to `<Script-Binding/>`, the decode moves with it.

Regression coverage exists at
`src/test/controllerscriptenginelegacy_test.cpp:128-170`
(`ddj400BrowseRotateDecodesTwosComplementToSingleStep`), which asserts both the
menu-open (`[AccessMenu],navigate`) and menu-closed (`[Library],MoveVertical`)
paths for `0x01` and `0x7F`.

The emulator was corrected to match (`tools/ddj400-emulator/ddj400_emulator.py`):
`BROWSE_UP = (0xB6, 0x40, 0x01)`, `BROWSE_DOWN = (0xB6, 0x40, 0x7F)` — real
two's-complement bytes, not the offset-64 bytes it previously sent. The same PR
also gave the emulator **14-bit tempo pairs** (`tempo()`, `:322-338`): the real
hardware sends the TEMPO fader as an MSB CC followed by an LSB CC, and
`PioneerDDJ400.tempoSliderLSB` only applies the rate once the LSB arrives — so
sending the MSB alone is a silent no-op and both messages must always be sent,
MSB first.

**Invariant C3b — the emulator must be byte-faithful.** Spec 03/04 use the
emulator as the Layer 1 test driver. An emulator that sends a *convenient*
byte convention rather than the *hardware's* convention will pass tests against
a broken decoder — which is precisely how the offset-64 bug survived a fix.

### 3.6 Focus dependency — Invariant C4

Controller-driven library navigation works by **synthesising Qt key events**
(`LibraryControl::emitKeyEvent`, `src/library/librarycontrol.cpp:995-1030`).
That path has a hard precondition:

```cpp
if (!QApplication::focusWindow() &&
        !CmdlineArgs::Instance().getControllerNavigationWithoutFocus()) {
    qInfo() << "No Mixxx window, popup or menu has focus."
            << "Don't send key events.";
    return;
}
```

The same guard appears at `librarycontrol.cpp:1034-1037` (`getFocusedWidget()`
returns `FocusWidget::None`) and `:1090-1095` (`setLibraryFocus()` bails).

| Situation | Without the flag | With the flag |
|---|---|---|
| Mixxx window focused | Works | Works |
| Mixxx window unfocused (e.g. the DJ tabbed to a screen reader's own window, or a browser) | **Browse knob does nothing, silently** — only a `qInfo` line in the log | Event is delivered to the current track table view (`librarycontrol.cpp:1021-1029`) |

`--controller-navigation-without-focus` is a **command-line flag only** — it is
not a preference and has no UI. Declared at
`src/util/cmdlineargs.cpp:386-392`, accessor `getControllerNavigationWithoutFocus()`
at `src/util/cmdlineargs.h:48-50`, defaults `false`
(`src/util/cmdlineargs.cpp:56`). Covered by
`src/test/cmdlineargs_test.cpp:27-42`.

**Invariant C4 — controller navigation dies on focus loss.** For a blind user
who is running a screen reader with its own windows and switching between
applications by sound, "Mixxx does not have focus" is the *normal* state, not
an edge case, and the failure is silent. The three guard sites above and the
CLI flag must all survive the rebase together; removing the flag while keeping
the guards makes controller browsing unusable whenever focus wanders, and
removing the guards while keeping the flag re-introduces the upstream crash
risk they were added to prevent.

Discoverability of a CLI-only flag is poor. See §6.

---

## 4. Controller mapping layer — Numark Scratch (secondary)

The Numark Scratch has **no dedicated accessibility pad layer** — instead the
fork overloads the existing **shift layer** on three physical buttons where
that overload costs least.

| Physical control | Fork behaviour | Rationale (from the source comments) |
|---|---|---|
| SHIFT (either) | `[Tts],shift` = 1 on press, 0 on release | Same layer-feedback contract as the DDJ-400 |
| Shift + CUE (either channel) | `[ChannelN],tts_status` | Speaks deck status **without touching PFL state** |
| Pad mode button | `[Tts],pad_mode` ∈ {1, 4, 9} | The mode button cycles blind through three states with no tactile or LED cue |
| Shift + Echo (FX unit 1 only) | `[Tts],repeat` | |
| Shift + Delay (FX unit 1 only) | `[BeatClick],enabled` toggle | |
| Shift + Flanger (FX unit 1 only) | `[Master],headSplitDecks` toggle | |

FX **unit 2**'s Shift+button behaviour (toggle that effect) is deliberately
**unchanged**. The stated reason is that unit 1's three buttons are
individually addressable physical buttons, unlike the pad-mode selector which
cycles blind — so unit 1 is the cheap place to spend three shift chords, and
unit 2 stays a normal effects unit.

The fork also fixed a genuine double-fire in `modeButtonPress`
(`Numark-Scratch-scripts.js`): the single physical mode button fires once per
deck (MIDI channels 5 and 6), and both press and release were routed to
`setMode()`. The fix filters on `(status & 0xF0) === 0x80 || value === 0`.
The `[Tts],pad_mode` announce then relies on Invariant C1 to deduplicate the
remaining both-decks-at-once firing.

**Invariant C5 — layer feedback is a cross-controller contract, not a DDJ-400
feature.** `[Tts],shift` and `[Tts],pad_mode` are set by two independent
mappings and consumed by one handler in `AnnouncementManager`. Their value
vocabulary (§3.1) is shared. Adding a third controller means picking values
from the existing 1-9 table, not extending it ad hoc.

---

## 5. UNRESOLVED — the `Ctrl+Alt` namespace conflict

**Status: open. PR #71 is held from merge pending a maintainer decision on
modifier policy.** This section states the contradiction and the evidence, and
offers a *recommendation*. The maintainer decides.

### 5.1 The contradiction

Three in-flight PRs disagree about whether `Ctrl+Alt` is usable at all.

| PR | Branch | Direction | Count | Verified by |
|---|---|---|---|---|
| **#76** | `wt-58-macos-voiceover-chords` | **Evacuates** `Ctrl+Alt` | −10 | `git diff accessibility-improvements-2026-06-25...wt-58-macos-voiceover-chords -- res/keyboard/en_US.kbd.cfg` |
| **#77** | `wt-56-kbd-mixer-fx` | **Fills** `Ctrl+Alt` | +36 | `…...wt-56-kbd-mixer-fx --` (`14 files changed, 697 insertions(+)`) |
| **#71** | `wt-50-keylock-binding` | **Fills** `Ctrl+Alt` | +10 | `…...wt-50-keylock-binding --` |

Net effect if all three merge: `Ctrl+Alt` goes from 10 fork bindings (+2
upstream `vinylcontrol_cueing`) to **46 fork bindings**. PR #76's stated
purpose — get the fork's chords off `Ctrl+Alt` because a screen reader owns it
— is not merely undone, it is overrun 4.6×.

The three PRs do **not** key-collide with each other (verified: #77's 18 deck-1
letters `n s a F9 l i j x e F11 w o f F10 b v g t` and #71's `k Up Down m r` are
disjoint from each other and from the base's `h d p c q` and upstream's `Y`
`U`). The conflict is entirely one of **policy**, which is why it cannot be
resolved by a merge tool.

### 5.2 What each PR does, exactly

**PR #76 (`wt-58-macos-voiceover-chords`)** — moves the fork's 10 `Ctrl+Alt`
chords out. Commit message rationale: *"macOS VoiceOver claims Ctrl+Option
(Ctrl+Alt in Qt) as its own command modifier, so this fork's BPM halve/double,
quick-add-to-crate/playlist, and quantize-toggle chords never reached Mixxx
while VoiceOver was running."*

| Control | Deck 1: from → to | Deck 2: from → to |
|---|---|---|
| `beats_set_halve` | `Ctrl+Alt+H` → `Alt+H` | `Ctrl+Alt+Shift+H` → `Alt+Shift+H` |
| `beats_set_double` | `Ctrl+Alt+D` → `Alt+D` | `Ctrl+Alt+Shift+D` → `Alt+Shift+D` |
| `quick_add_to_playlist` | `Ctrl+Alt+P` → `Alt+P` | `Ctrl+Alt+Shift+P` → **`Ctrl+Shift+P`** |
| `quick_add_to_crate` | `Ctrl+Alt+C` → `Alt+C` | `Ctrl+Alt+Shift+C` → **`Ctrl+Shift+C`** |
| `quantize` | `Ctrl+Alt+Q` → `Alt+Q` | `Ctrl+Alt+Shift+Q` → `Alt+Shift+Q` |

Deck 2's quick-add pair breaks the `Alt+Shift` pattern of its four siblings
because `Alt+Shift+P`/`C` are already taken by `[Library],AddToCrate`/
`AddToPlaylist` (§2.2 category C). PR #76 leaves upstream's
`vinylcontrol_cueing Ctrl+Alt+Y`/`Ctrl+Alt+U` alone as out of scope — correctly,
since those predate the fork.

**PR #77 (`wt-56-kbd-mixer-fx`)** — adds 36 bindings, all in `Ctrl+Alt`
(18 deck 1, 18 deck 2 with `+Shift`):

| Area | Deck-1 chords |
|---|---|
| Filter (QuickEffect super knob) | `Ctrl+Alt+N` down, `Ctrl+Alt+S` up |
| Channel volume | `Ctrl+Alt+B` down, `Ctrl+Alt+V` up |
| Channel trim (pregain) | `Ctrl+Alt+G` down, `Ctrl+Alt+T` up |
| EQ low | `Ctrl+Alt+E` down, `Ctrl+Alt+F11` up |
| EQ mid | `Ctrl+Alt+W` down, `Ctrl+Alt+O` up |
| EQ high | `Ctrl+Alt+F` down, `Ctrl+Alt+F10` up |
| Effect unit power / chain preset / slot focus | `Ctrl+Alt+A`, `Ctrl+Alt+F9`, `Ctrl+Alt+L` |
| Effect slot 1 enable / next / prev | `Ctrl+Alt+I`, `Ctrl+Alt+J`, `Ctrl+Alt+X` |

**PR #71 (`wt-50-keylock-binding`)** — adds 10 bindings, all in `Ctrl+Alt`:

| Control | Deck 1 | Deck 2 |
|---|---|---|
| `keylock` | `Ctrl+Alt+K` | `Ctrl+Alt+Shift+K` |
| `pitch_up` | `Ctrl+Alt+Up` | `Ctrl+Alt+Shift+Up` |
| `pitch_down` | `Ctrl+Alt+Down` | `Ctrl+Alt+Shift+Down` |
| `sync_key` | `Ctrl+Alt+M` | `Ctrl+Alt+Shift+M` |
| `reset_key` | `Ctrl+Alt+R` | `Ctrl+Alt+Shift+R` |

PR #71 also touches `res/controllers/Pioneer-DDJ-400-script.js` (+20) and the
emulator, so it is not a pure keyboard change.

### 5.3 Confirmed screen-reader collisions

macOS VoiceOver's default command modifier is **Control+Option** (`VO`).
Confirmed collisions:

| Chord | Fork use | PR | VoiceOver command | Severity |
|---|---|---|---|---|
| `Ctrl+Alt+K` | `keylock` | #71 | `VO+K` — Keyboard Help on/off | High: swallows *all* subsequent keys until dismissed |
| `Ctrl+Alt+Up` | `pitch_up` | #71 | `VO+↑` — core navigation | Critical: the single most-used VO key |
| `Ctrl+Alt+Down` | `pitch_down` | #71 | `VO+↓` — core navigation | Critical |
| `Ctrl+Alt+M` | `sync_key` | #71 | `VO+M` — move to menu bar | High |
| `Ctrl+Alt+R` | `reset_key` | #71 | `VO+R` — read row | High |
| `Ctrl+Alt+A` | Effect unit 1 power | #77 | `VO+A` — read all from cursor | High |

**All five of PR #71's deck-1 chords collide with a VoiceOver command.** That
is the whole PR, not a corner of it — which is why it is held.

Further `Ctrl+Alt+<letter>` chords in PR #77 land on letters VoiceOver also
uses (`F` find, `I` item chooser, `J` jump to linked item, `W` read word,
`S` read sentence, `B` read from top, `T` text attributes, `L` read line).
These are **probable, not verified** — they should be spot-checked against the
maintainer's actual VoiceOver build before being cited as fact. The five in the
table above are confirmed.

`Ctrl+Alt+F9`/`F10`/`F11` (PR #77) are additionally exposed to macOS Mission
Control / volume defaults and to Linux virtual-terminal switching.

### 5.4 A technical wrinkle the maintainer should know about

Two facts about how Qt delivers these key events, both verified on this branch:

1. `KeyboardEventFilter::getKeySeq()`
   (`src/controllers/keyboard/keyboardeventfilter.cpp:163-178`) builds the
   sequence string from `Qt::ControlModifier` → `"Ctrl+"` and
   `Qt::MetaModifier` → `"Meta+"`.
2. `Qt::AA_MacDontSwapCtrlAndMeta` is **never set** anywhere in `src/`
   (`grep -rn "AA_MacDontSwapCtrlAndMeta" src/` → no hits; the only
   `setAttribute(Qt::AA…)` calls are `AA_EnableHighDpiScaling`,
   `AA_UseHighDpiPixmaps`, `AA_ShareOpenGLContexts` at `src/main.cpp:184-188`
   and `AA_DontUseNativeMenuBar` at `src/mixxxmainwindow.cpp:1743`).

Under Qt's default on macOS, `Qt::ControlModifier` maps to the **Command** key
and `Qt::MetaModifier` maps to the **Control** key. Taken at face value that
implies:

- `Ctrl+Alt+H` in a `.kbd.cfg` fires on **Cmd+Option+H** on macOS — which is
  the system "Hide Others" shortcut that Qt's Cocoa plugin installs into the
  application menu automatically. Pressing it hides Mixxx.
- The physical **Control+Option+H** (VoiceOver's `VO+H`) would arrive as
  `Meta+Alt+H`, and **no `.kbd.cfg` in this repo binds any `Meta+` sequence**
  (`grep -rn "Meta+" res/keyboard/` → no hits), so it is unbound.

This does not change the conclusion — the chords are broken on macOS either
way — but it changes the *mechanism*, and therefore possibly the right fix.
PR #76's commit message asserts "Ctrl+Option (Ctrl+Alt in Qt)", which appears
to be the opposite of Qt's default mapping.

**Recommended before deciding:** settle this empirically rather than by
argument. `KeyboardEventFilter` already logs every key press when Mixxx runs
with `--developer` (`keyboardeventfilter.cpp:180-186`):

```
keyboard press:  Ctrl+Alt+H
```

Running `mixxx --developer`, pressing the chord with VoiceOver on and again
with it off, and reading the log line settles in one minute what the sequence
actually is on that machine. That log line is the ground truth; everything in
§5.3 and §5.4 is inference until it is run.

### 5.5 An independent, platform-crossing argument against `Ctrl+Alt`

Separate from any screen reader: on **Windows and Linux**, `Ctrl+Alt` *is*
**AltGr** on every non-US keyboard layout. `Ctrl+Alt+Q` is `@` on German
layouts; `Ctrl+Alt+E` is `€` on several; `Ctrl+Alt+<letter>` produces real
characters across the board.

Because of Invariant K1 the fork's block is byte-identical in all 12 locale
files — **including `de_DE`, `de_CH`, `fr_FR`, `fr_CH`, `es_ES`, `it_IT`,
`cs_CZ`, `el_GR`, `ru_RU`, `da_DK`, `fi_FI`**. Every `Ctrl+Alt+<letter>`
binding therefore shadows a character-entry combination for the majority of
shipped layouts, most visibly in the library search box.

On Linux, `Ctrl+Alt+↑`/`↓` (PR #71) are GNOME and KDE **workspace switching**
shortcuts, taken by the window manager before any application sees them.

So `Ctrl+Alt` is a poor namespace on **all three** platforms, for **three
different reasons**, only one of which is VoiceOver.

### 5.6 Recommended modifier policy (recommendation only)

| # | Rule | Reasoning |
|---|---|---|
| **R1** | **Freeze `Ctrl+Alt`.** Add nothing new to it. Leave upstream's `vinylcontrol_cueing Ctrl+Alt+Y/U` alone (pre-fork, not ours to move). | §5.3, §5.4, §5.5 — burned on macOS, Windows and Linux for three independent reasons |
| **R2** | **`Alt+<key>` = deck 1, `Alt+Shift+<key>` = deck 2** is the fork's canonical namespace. | It already ships and works: `Alt+1`–`Alt+0`, `Alt+B/J/S/X`, `Alt+Shift+A/R/T/Y/C/P` are in production on this maintainer's macOS build today. That is empirical proof that Option+key reaches `KeyboardEventFilter` as `Alt+` on macOS, which no amount of reasoning gives us for a new namespace. |
| **R3** | Adopt PR #76's direction, but **finish the pattern**: `Alt+Shift+P`/`C` are taken by `[Library],AddToCrate/AddToPlaylist`, so either rename those to free the slot, or accept the deck-2 exception explicitly and document it. Do not leave two of ten siblings on a different modifier by accident. | Consistency is the whole value of the odd/even and deck-1/deck-2 conventions |
| **R4** | **Do not solve PR #77 with chords at all.** 36 mixer/EQ/FX chords will not fit in `Alt` and will not be memorised. Route them through the existing **`[AccessMenu]` value editor** (Spec 01 addendum), which already has a `control + min + max + step + format` model and already drives `ControlProxy`-backed values. `[EqualizerRack1_[Channel1]_Effect1],parameter1` fits that model with no new machinery, and gets spoken feedback for free. | The value editor is reachable from the DDJ-400 with **no script change** (Spec 01 addendum), so this also gets the mixer to controller users |
| **R5** | Bind at most a **small, memorable** subset by key — the ones needed mid-mix under time pressure (channel volume, filter, EQ high kill). Everything else lives in the menu. | A binding a blind DJ cannot recall is worse than no binding: it occupies a chord and does something unexpected |
| **R6** | **Never** bind bare `Meta` (Super/Win/macOS-Control), `Insert` (NVDA + JAWS modifier), `CapsLock` (Orca/NVDA laptop layouts), or `Ctrl+Alt+<arrow>` (GNOME/KDE workspaces). | Guaranteed conflict on at least one platform |
| **R7** | Anything that must survive `[Keyboard],Enabled = 0` or dialog focus goes in `wmainmenubar.cpp` as a `QAction` + `Qt::ApplicationShortcut` (Invariant K2), not in `.kbd.cfg`. | §2.5 |
| **R8** | Settle §5.4 with `mixxx --developer` before finalising. If the Ctrl↔Cmd swap is real, add a short "modifier mapping by platform" table to `ACCESSIBILITY.md` so this argument does not have to be had again. | One measurement replaces an unbounded debate |

#### Namespace safety, by platform

| Namespace | macOS + VoiceOver | Windows + NVDA/JAWS | Linux + Orca | Verdict |
|---|---|---|---|---|
| `Alt+<key>` | **Safe** — in production on this fork today | Safe; `Alt` alone opens the menu bar only on key-*up* with no other key | Safe (same caveat) | **Preferred** |
| `Alt+Shift+<key>` | **Safe** — in production today | Safe | Safe | **Preferred (deck 2)** |
| `Ctrl+<key>` | = Cmd; heavily used by the app menu and macOS | Safe-ish; collides with app conventions | Safe-ish | Use sparingly, menu actions only |
| `Ctrl+Shift+<key>` | = Cmd+Shift | Safe | Terminal copy/paste convention | Acceptable fallback |
| `Ctrl+Alt+<key>` | Cmd+Opt system shortcuts **and/or** VoiceOver `VO+key` (§5.4) | **= AltGr** on all non-US layouts | **= AltGr**; `Ctrl+Alt+arrow` = workspaces | **Avoid** |
| `Ctrl+Alt+Shift+<key>` | Same, plus unreachable one-handed | Same AltGr problem | Same | **Avoid** |
| `Meta+<key>` | = physical Control; `Ctrl+Opt` is the VO modifier | Win key → Start menu | Super → WM/overview | **Never** |
| `Insert+<key>` | n/a (no Insert on Apple keyboards) | **NVDA/JAWS modifier** | **Orca modifier (desktop)** | **Never** |
| `CapsLock+<key>` | n/a | NVDA laptop modifier | Orca laptop modifier | **Never** |
| Bare `F1`–`F12` | Needs `fn` unless "use F keys as standard" | Mostly free | Mostly free | Large but inconsistent; last resort |
| `Ctrl+Alt+F9`–`F12` | Mission Control / volume | AltGr + function | VT switching on some setups | **Avoid** |

---

## 6. Recommended lint (does not exist yet)

Invariant K1 is currently enforced by nothing. There is no test anywhere that
binds a specific key sequence to a control — PR #76's own commit message says
so: *"No test coverage binds specific key sequences for these controls, so this
is verified by inspection (grep) rather than a test update."*

A `tools/check_kbd_bindings.py` should assert:

1. The fork's a11y block is byte-identical across all 12 `res/keyboard/*.kbd.cfg`.
2. No duplicate key sequence within a single file (across all groups).
3. No fork binding in a namespace on the policy deny-list (whatever R1/R6
   resolves to).

**Wire it into `.gitea/workflows/`, not only `.pre-commit-config.yaml`.** As
documented in Spec 09 §4.3, `tools/check_ui_buddies.py` is pre-commit-only and
is therefore **not run by any of the four `.gitea` workflows** — which are what
actually run on this fork. A lint that only runs in a hook the maintainer might
bypass with `--no-verify` does not protect a rebase.

`--controller-navigation-without-focus` (Invariant C4) is similarly
undiscoverable as a CLI-only flag. Promoting it to a checkbox on the
Accessibility preferences page would make it findable by the users who need it;
that page is documented in Spec 09 §5.

---

## 7. Invariant summary (rebase checklist)

| ID | Invariant | Failure mode if lost | Loud? |
|---|---|---|---|
| **K1** | The 34 fork a11y bindings are byte-identical in all 12 `.kbd.cfg` files | Users on 11 locales silently lose bindings | **Silent** |
| **K2** | Must-always-work actions are `QAction` + `Qt::ApplicationShortcut` in `wmainmenubar.cpp`, reading their key from `m_pKbdConfig` | Speech toggle becomes unreachable once speech or shortcuts are off | **Silent** |
| **K3** | `[Keyboard],Enabled` gates all `.kbd.cfg` bindings; `Custom.kbd.cfg` shadows the shipped file entirely | "The accessibility keys just don't work", with no UI explanation | **Silent** |
| **C1** | `[Tts],pad_mode` speaks only on `valueChanged`; same-value writes are silent | Double speech on hardware that fires per-deck | Audible |
| **C2** | The DDJ-400 accessibility pad layer is opt-in (`accessibilityPads`, default `false`) and disables hot cues while active | Hot cues silently stop working for sighted users if defaulted on | **Silent** |
| **C3** | A relative encoder on a `<Script-Binding/>` decodes its own byte convention and emits exactly ±1 per detent | Runaway scroll through the track list / spoken menu; unrecoverable without sight | Audible but confusing |
| **C3b** | The emulator sends the hardware's real byte convention (two's-complement browse deltas, 14-bit MSB+LSB tempo pairs) | Tests pass against a broken decoder | **Silent** |
| **C4** | The three `getControllerNavigationWithoutFocus()` guards in `librarycontrol.cpp` (`:997`, `:1035`, `:1091`) and the CLI flag survive together | Controller browsing dies whenever Mixxx loses focus, logging only `qInfo` | **Silent** |
| **C5** | `[Tts],shift` / `[Tts],pad_mode` are a shared cross-controller vocabulary (9 values), not DDJ-400-specific | Third controllers invent conflicting values | Audible |

Six of nine fail **silently**. That is the argument for §6.

---

## 8. Open questions for review

1. **The `Ctrl+Alt` policy** (§5.6). Maintainer decision. PR #71 is held on it.
2. **Is the Qt Ctrl↔Cmd swap real on this build?** (§5.4). One `--developer`
   run settles it and determines whether PR #76's rationale needs correcting in
   its commit message and in `ACCESSIBILITY.md`.
3. **Should PR #77's 36 mixer/EQ/FX controls be chords at all**, or value-editor
   entries (R4)? This is the largest single input-design decision outstanding.
4. **Should `--controller-navigation-without-focus` become a preference?**
   It is currently CLI-only and undiscoverable, yet it is the difference between
   the browse knob working and not working for a screen-reader user (§3.6).
5. **Should `Custom.kbd.cfg` merge with, rather than replace, the shipped file?**
   Today a pre-fork custom mapping silently removes every accessibility binding
   (§2.4). A merge — or at minimum a spoken warning at startup — would close a
   trap that has no visible symptom.

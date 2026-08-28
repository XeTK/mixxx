# Spec 08 — Input layer: keyboard bindings and controller mappings

**Status:** Verified current state — refreshed against the merged codebase
**Branch:** `spec-controller`, verified against `accessibility-improvements-2026-06-25`
@ `1cb23d38e7` (the real branch tip, not a hypothetical merge of open PRs — see
note below)
**Owner:** accessibility fork
**Related:** Spec 01 (DDJ-400 spoken menu), Spec 03 (DDJ-400 emulator), Spec 04
(E2E testing), Spec 05 (speech engine), Spec 06 (announcement layer), Spec 07
(spoken menu controller), Spec 09 (library / UI accessibility surface)

## Goal

Document the two ways a blind DJ physically drives Mixxx on this fork — the
**keyboard mappings** (`res/keyboard/*.kbd.cfg`) and the **controller
mappings** (`res/controllers/`) — as a contract that the upstream-2.6 rebase
must preserve. This is a specification of *what is true on this branch today*,
plus the invariants that make it work.

**A note on what "current" means here.** Specs 05-07 were verified against
`scratch-target-state`, a throwaway branch that merged the
`accessibility-improvements-2026-06-25` tip together with every other
then-open accessibility PR, to see what the *fully landed* fork would look
like. This spec does not do that. It describes only what is actually reachable
from `1cb23d38e7` — the real tip of `accessibility-improvements-2026-06-25` —
via `tea pr list`. Several PRs that touch this spec's territory are **still
open** as of this writing (§5, §6) and are described as proposals, not shipped
behaviour.

---

## 1. Background / current state (verified on this branch)

### 1.1 Keyboard

| Fact | Evidence |
|---|---|
| 12 locale files ship | `res/keyboard/{cs_CZ,da_DK,de_CH,de_DE,el_GR,en_US,es_ES,fi_FI,fr_CH,fr_FR,it_IT,ru_RU}.kbd.cfg` |
| The fork's accessibility set is a **minimum of 30 controls**, present and byte-identical (as *controls*, not raw diff line counts) in every locale | `src/test/keyboardbindings_test.cpp`, `forkAccessibilityBindings()` (comment: *"30 entries, replicated verbatim into all 12 locale files"*) |
| Raw diff size per locale file has grown well past that 30-control set | `git diff 0e0589c751..HEAD -- res/keyboard/<locale>.kbd.cfg` adds 86 content lines in 9 of the 12 locales (the mixer/EQ/filter/effects bindings from issue #56, §1.2, are not part of the tested 30-control minimum but ship in the same commits) |
| Three locale files differ from the other nine by 1-2 lines, **on purpose** | `el_GR` (+88), `fr_FR` (+87), `ru_RU` (+87) carry small upstream-collision fixes (issue #95, PR #130, merged) that are locale-specific by necessity — see §6 |

### 1.2 Controllers

| File | Fork delta vs. fork point (`0e0589c751`) | Role |
|---|---|---|
| `res/controllers/Pioneer-DDJ-400-script.js` | +258 / −rewrites | Primary accessibility controller: spoken menu entry, accessibility pad layer, jog safety, layer feedback |
| `res/controllers/Pioneer-DDJ-400.midi.xml` | +387 / −(rewrites) | Script-Binding conversions, `<settings>` block, pad-mode announce bindings, MASTER LEVEL knob binding (§3.2, §3.6) |
| `res/controllers/Numark-Scratch-scripts.js` | +69 | Accessibility shift layer (secondary controller) |
| `res/controllers/Numark-Scratch.midi.xml` | +2/−2 | `<description>` documenting the shift layer |

`git diff --stat 0e0589c751..HEAD -- res/controllers/` also shows Traktor
Kontrol S2 MK3 (+185/−, HID script), Traktor MX2 (+9), Yaeltex MiniMixxx
(+15/+6) and `mixxx-controls.d.ts` (+80) changes; those are **upstream churn
carried across the fork point**, not accessibility work, and remain out of
scope for this spec.

---

## 2. Keyboard binding layer

### 2.1 How bindings are loaded

`CoreServices::initializeKeyboard()` — `src/coreservices.cpp:860-907`.

| Step | Behaviour |
|---|---|
| Resolve `Custom.kbd.cfg` in the settings dir (`:871`) | User override wins over every shipped locale file |
| Otherwise pick `<locale>.kbd.cfg` from `inputLocale()` (`:881`) | Locale comes from the **input method / keyboard layout**, not the UI language |
| Build a second, **empty** `ConfigObject<ConfigValueKbd>` (`:874`) | `m_pKbdConfigEmpty` — the "shortcuts off" config |
| Construct `KeyboardEventFilter` with whichever config `[Keyboard],Enabled` selects (`:906`) | The gate |

Toggling at runtime is `CoreServices::slotOptionsKeyboard()`
(`src/coreservices.cpp:909-...`): it swaps the filter's config pointer between
`m_pKbdConfig` and `m_pKbdConfigEmpty` (`:917`) and writes `[Keyboard],Enabled`.

**Consequence:** when a user hits `Ctrl+`` ` (`OptionsMenu_EnableShortcuts`),
**every accessibility binding in the tables below stops working at once** —
readouts, quick-add, BPM halve/double, quantize. The only exception is the
`Qt::ApplicationShortcut` escape hatch in §2.5.

Key sequences are matched by `KeyboardEventFilter::getKeySeq()`
(`src/controllers/keyboard/keyboardeventfilter.cpp:150-178`), which composes a
`QKeySequence` string from the individual Qt modifier flags
(`Shift+`/`Ctrl+`/`Alt+`/`Meta+`) rather than from `e->modifiers()` wholesale.
This matters for §5.

### 2.2 The fork's accessibility bindings (en_US, minimum set identical in all 12)

#### Category A — global quick toggles

| Binding | Group, control | Effect |
|---|---|---|
| `Alt+X` | `[Master],crossfader_lock` | Lock the crossfader so a stray knock can't kill the mix |
| `Alt+S` | `[Master],headSplitDecks` | Split headphone cue |
| `Alt+J` | `[Master],disable_touch_scratch` | Jog touch lock |
| `Alt+B` | `[BeatClick],enabled` | Beat-click metronome |
| `Alt+Shift+A` | `[Tts],enabled` | Speech on/off — **also a `QAction`, see §2.5** |
| `Alt+Shift+R` | `[Tts],repeat` | Repeat last announcement |

`crossfader_lock` and `disable_touch_scratch` are fork-added engine controls,
announced by `src/util/announcementmanager.cpp:552` and `:770`.

#### Category B — per-deck on-demand readouts (the odd/even convention)

The digit row is split **odd digit = deck 1, even digit = deck 2**, so the two
decks' readouts sit next to each other under the same finger:

| Readout | Deck 1 | Deck 2 |
|---|---|---|
| Status (playing/stopped, time, BPM, pitch) | `Alt+1` | `Alt+2` |
| Time remaining | `Alt+3` | `Alt+4` |
| BPM | `Alt+5` | `Alt+6` |
| Musical key | `Alt+7` | `Alt+8` |
| Bar position | `Alt+9` | `Alt+0` |
| Track name | `Alt+Shift+T` | `Alt+Shift+Y` |

Controls are `[ChannelN],tts_status` / `tts_time` / `tts_bpm` / `tts_key` /
`tts_bar` / `tts_track`. Semantics are Spec 06's territory; this spec owns
only the binding.

Note the convention breaks at the wrap: `Alt+9` is deck 1's bar position but
`Alt+0` is deck 2's, because `0` sits to the right of `9` on the number row —
deliberate, positional rather than arithmetic. Track name escapes the digit
row entirely (`Alt+Shift+T`/`Y`) because the digits were exhausted at five
readouts per deck.

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

| Control | Deck 1 | Deck 2 | Purpose |
|---|---|---|---|
| `beats_set_halve` | `Ctrl+Alt+H` | `Ctrl+Alt+Shift+H` | Fix a double-tempo analysis by ear |
| `beats_set_double` | `Ctrl+Alt+D` | `Ctrl+Alt+Shift+D` | Fix a half-tempo analysis by ear |
| `quantize` | `Ctrl+Alt+Q` | `Ctrl+Alt+Shift+Q` | First keyboard binding quantize has ever had in a shipped layout |

Categories C and D are the **10 original-fork bindings that still sit in the
`Ctrl+Alt` namespace** (`res/keyboard/en_US.kbd.cfg:104-108, 173-177`) and are
the subject of §5.

#### Category E — mixer, EQ, filter and effects (issue #56, merged since the last verification)

Landed as PR #77 (issue #56). Unlike the original three-way `Ctrl+Alt`
proposal described in earlier drafts of this spec, the version that actually
merged puts almost everything on plain `Alt`/`Alt+Shift`, dodging every
existing chord — this is worth calling out because it is exactly the R2-style
policy this spec used to only *recommend*:

| Area | Deck 1 | Deck 2 |
|---|---|---|
| Filter (QuickEffect super knob) | `Alt+F` down, `Alt+W` up | `Alt+Shift+F` down, `Alt+Shift+W` up |
| Channel volume | `Alt+U` down, `Alt+G` up | `Alt+Shift+U` down, `Alt+Shift+G` up |
| Channel trim (pregain) | `Alt+H` down, `Alt+D` up | `Alt+Shift+H` down, `Alt+Shift+D` up |
| EQ low | `Alt+L` down, `Alt+F11` up | `Alt+Shift+L` down, `Alt+Shift+F11` up |
| EQ mid | `Alt+E` down, `Alt+I` up | `Alt+Shift+E` down, `Alt+Shift+I` up |
| EQ high (kill) | `Alt+Q` down, `Alt+F10` up, `B` (bare) kills EQ low | `Alt+Shift+Q` down, `Alt+Shift+F10` up, `N` (bare) kills EQ low |
| Effect unit enable / next chain preset | `Alt+N`, `Alt+F9` | `Alt+Shift+N`, `Alt+Shift+F9` |
| Effect slot 1 enable | `Alt+Z` | `Alt+Shift+Z` |

Three controls did **not** fit the `Alt` namespace without a further
collision and are still on `Ctrl+Alt`, each carrying its own
`// TODO(issue #56 follow-up)` comment pointing at the ongoing `Ctrl+Alt`
discussion (§5):

| Control | Deck 1 | Deck 2 |
|---|---|---|
| `focused_effect` | `Ctrl+Alt+L` | `Ctrl+Alt+Shift+L` |
| `next_effect` | `Ctrl+Alt+J` | `Ctrl+Alt+Shift+J` |
| `prev_effect` | `Ctrl+Alt+X` | `Ctrl+Alt+Shift+X` |

(`res/keyboard/en_US.kbd.cfg:32-90`.) These 6, plus category D's original 10,
plus upstream's 2 `vinylcontrol_cueing` bindings, are the full current
`Ctrl+Alt` inventory — see §5.

### 2.3 Why the a11y block is deliberately NOT localised

The 30-control minimum set is bytewise/structurally identical across all 12
locales (enforced by `AccessibilityChordsAreIdenticalAcrossLocales` in
`src/test/keyboardbindings_test.cpp`). The upstream bindings around them *are*
localised (deck play is `d`/`l` on `en_US`, and moves on AZERTY/QWERTZ
layouts). The fork's accessibility set does not move.

| Reason | Detail |
|---|---|
| **Muscle memory is the interface** | A blind DJ cannot look at a cheat sheet mid-set. `Alt+5` must be "deck 1 BPM" on every machine they touch, including a borrowed one with a different system locale. |
| **The locale is chosen by keyboard layout, not by user intent** | `initializeKeyboard()` derives the file from `inputLocale()` (`coreservices.cpp:881`). Plugging in a different keyboard, or a locale-misdetecting OS, silently swaps the whole mapping file. Identical a11y bindings make that swap invisible for accessibility features. |
| **The documentation is single-sourced** | `ACCESSIBILITY_GUIDE.md`, `ACCESSIBILITY_QUICK_REFERENCE.md` and the DDJ-400 mapping `<description>` all quote one set of key names. Twelve variants would need twelve doc sets. |
| **It makes drift mechanically detectable** | `keyboardbindings_test.cpp` (§6) hashes/diffs the set on every test run — that guard did not exist when this invariant was first written, and now it does. |

**Invariant K1 — locale-invariance of the tested minimum set.** The fork's
30-control accessibility minimum is present and identically bound across all
12 `res/keyboard/*.kbd.cfg` files, enforced by
`KeyboardBindingsTest.AccessibilityChordsAreIdenticalAcrossLocales` and
`.AccessibilityBindingsPresentInEveryLocale`. Any change must be applied to
all 12 in the same commit; the test now catches a rebase that drops one
locale, which the original version of this invariant (written when no such
test existed) could not.

**Caveat, resolved but worth keeping in mind:** three locale files
(`el_GR`, `fr_FR`, `ru_RU`) legitimately differ from the other nine by a
line or two, because issue #95 (PR #130, merged) fixed pre-existing
**upstream** collisions that were locale-specific by nature (an AZERTY-only
Shift+a/A clash, a Greek-alphabet collision, a Cyrillic collision). Those
fixes touch upstream controls, not the fork's accessibility set, so K1 still
holds for the set it actually governs.

**Cost accepted:** the block is chosen for US-QWERTY ergonomics and takes no
account of what those physical positions are on AZERTY/QWERTZ, nor of what
characters `Ctrl+Alt+<letter>` produces on layouts where `Ctrl+Alt` is
**AltGr** (§5). This is a known, deliberate trade, and is the entire reason
§5's `Ctrl+Alt` debate exists at all.

### 2.4 What gates the bindings

| Gate | Effect |
|---|---|
| `[Keyboard],Enabled = 0` (`Ctrl+`` `) | Every accessibility binding is dead |
| A modal dialog has focus | `KeyboardEventFilter` is installed application-wide, but Qt delivers to the focused window; dialog-local handling wins |
| `Custom.kbd.cfg` exists in the settings dir | The shipped file is **not read at all** — a user with an old `Custom.kbd.cfg` from before the fork gets **zero** accessibility bindings (`coreservices.cpp:871`) |

That last row is a real trap: it is silent, and the user's symptom is "the
accessibility keys just don't work", with nothing in the UI to explain it.

### 2.5 The `Qt::ApplicationShortcut` escape hatch

`Alt+Shift+A` (speech on/off) is bound **twice**, on purpose:

1. As a normal `.kbd.cfg` line: `[Tts] enabled Alt+Shift+a`.
2. As a `QAction` in `WMainMenuBar::createOptionsMenu()` —
   `src/widget/wmainmenubar.cpp:520-552`:

```cpp
pOptionsTts->setShortcut(
        safeKeySequence(m_pKbdConfig->getValue(
                ConfigKey("[Tts]", "enabled"),
                QStringLiteral("Alt+Shift+A"))));
pOptionsTts->setShortcutContext(Qt::ApplicationShortcut);
```

The `QAction` reads its key **from the same kbd config**, so the two can never
disagree, and the in-source default `"Alt+Shift+A"` is the last-resort
fallback if the config has no entry.

| Property | Why it matters |
|---|---|
| Qt's shortcut map consumes the key press **before** `KeyboardEventFilter` sees it | No double-toggle; the two bindings are mutually exclusive at runtime, not additive |
| `Qt::ApplicationShortcut` scope | Fires regardless of which window/widget in the application has focus — including modal dialogs |
| It is a `QAction`, not a kbd.cfg line | Survives `[Keyboard],Enabled = 0` |
| It is in the menu | Discoverable by screen-reader menu traversal; the menu item advertises its own shortcut |

`WMainMenuBar` bridges to the control **via a signal**
(`WMainMenuBar::toggleTts`, connected at `wmainmenubar.cpp:546`) rather than a
`ControlProxy`, because the menu bar is constructed before `CoreServices`
creates the engine; the connection to `[Tts],enabled` is made later in
`MixxxMainWindow::connectMenuBar()`.

**Invariant K2 — the always-works pattern.** Anything that must work when
shortcuts are disabled, when a modal dialog has focus, or when the skin is in
a broken state, must be a `QAction` with `Qt::ApplicationShortcut` in
`wmainmenubar.cpp`, reading its key from `m_pKbdConfig` with an in-source
default. A `.kbd.cfg` line alone is not sufficient. Speech-toggle is the
canonical case: if speech is off *and* the toggle is gated behind the thing
that turned it off, the user is locked out with no feedback channel.

16 other menu actions already use `setShortcutContext(Qt::ApplicationShortcut)`
(`wmainmenubar.cpp:137, 160, 210, 223, 360, 405, 441, 469, 489, 513, 563, 586,
599, 619, 639, 661`), so this is an established pattern, not a one-off.

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
| `[Tts],shift` | `ControlPushButton` | `PioneerDDJ400.shiftPressed`, `NumarkScratch.shift/unshift` | "Shift" — **on the press only**; release is silent (`announcementmanager.cpp:878-891`) |
| `[Tts],pad_mode` | `ControlObject` | `PioneerDDJ400.padModePressed`, `NumarkScratch.setMode` | "Pads, `<mode>`" — or "Pads, `<mode>` (not yet supported)" if the mode has no working pad layer yet (`announcementmanager.cpp:893-950`) |
| `[Tts],repeat` | trigger | Accessibility pad 7, Numark Shift+Echo | Repeats last announcement |

`[Tts],pad_mode` is a **fixed cross-controller vocabulary of 9 values**:

| Value | Spoken | DDJ-400 status | Numark Scratch |
|---|---|---|---|
| 1 | "hot cues" | Implemented | HOTCUE |
| 2 | "beat loop" | Implemented | — |
| 3 | "beat jump" | Implemented | — |
| 4 | "sampler" | Implemented | SAMPLER |
| 5 | "keyboard" | Mode button lights, **pads do nothing** — announced "(not yet supported)" | — |
| 6 | "pad effects 1" | Same as 5 | — |
| 7 | "pad effects 2" | Same as 5 | — |
| 8 | "key shift" | Same as 5 | — |
| 9 | "loop roll" | Implemented | ROLL |

Values 5-8 not being wired to real pad behaviour, and the announcement now
saying so explicitly, is issue #65 (PR #82, merged) — before that fix a blind
DJ pressing a pad in one of those modes got silence, indistinguishable from
"nothing happened" versus "this mode has no pads yet". `announcementmanager.cpp`
tracks this with an `implemented` bool per case (`:915-949`) and picks between
`speak(tr("Pads, %1").arg(mode))` and
`speak(tr("Pads, %1 (not yet supported)").arg(mode))`.

Any value outside 1-9 is a silent no-op (`default: return;` at
`announcementmanager.cpp:944-945`).

**Invariant C1 — same-value writes are silent.** `[Tts],pad_mode` only speaks
on a `valueChanged`, so re-pressing the current mode button says nothing, and
hardware that fires one mode press per deck (the Numark Scratch's single mode
button reaches both `[Channel1]` and `[Channel2]` `PadSection` instances) is
deduplicated for free. This CO deliberately keeps `bIgnoreNops` at its default
(`true`) for this reason — a mapping that wants a same-mode re-press to
re-announce (e.g. a future "what mode am I in" query) must bounce the value
through 0 first rather than changing this construction.

### 3.2 Mapping settings (`<settings>` block, `Pioneer-DDJ-400.midi.xml`)

`<settings><group label="Accessibility">` (`Pioneer-DDJ-400.midi.xml:10-40`).
All three options are read once at script load
(`Pioneer-DDJ-400-script.js:859`, `:864-869`).

| Option | Type | Default | Effect |
|---|---|---|---|
| `accessibilityPads` | boolean | `false` | Opt-in accessibility pad layer (§3.3) |
| `disableJogScratch` | boolean | `false` | Sets `PioneerDDJ400.vinylMode = false` — platter touch becomes a no-op so a stray hand can't stop or scratch playback. Rotation still nudges pitch; Shift+jog still seeks. |
| `jogSensitivity` | real, 0.1–5.0, step 0.1 | `1.0` | Scales pitch-bend nudge (`script.js:622`), Shift+jog seek rate (`:634`) and scratch response (`:622`) |

The last two are the **jog-safety** settings. They exist because the DDJ-400's
capacitive platter top is the single largest accidental-input hazard for a DJ
who cannot see where their hands are.

### 3.3 The accessibility pad layer (opt-in)

When `accessibilityPads` is on, the eight **Hot Cue mode** pads are
re-purposed (`PioneerDDJ400.hotcuePad`, `script.js:875-891`;
`hotcuePadShift`, `:895-914`):

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

Shift + pads 3-6 are intentionally inert so a stray press cannot clear hot
cues the DJ cannot see (`script.js:912-913`).

**Invariant C2 — the pad layer is opt-in and destructive of hot cues.** While
`accessibilityPads` is on, hot cues cannot be set, triggered, or cleared from
the pads at all — the early-return in `hotcuePad`/`hotcuePadShift` when
`accessibilityPads` is true (`script.js:876-878`, `:896-898`) branches away
from the stock hotcue behaviour entirely. This is a real functional
sacrifice and is why the default is `false` and the setting carries a
`<description>` saying so. It must not be flipped to default-on during a
rebase.

Every action confirms itself out loud, so no LED feedback is needed
(comment at `script.js:857`).

### 3.4 Browse knob and spoken-menu entry

The mapping converts browse/load bindings from declarative MIDI options to
`<Script-Binding/>` so the script can branch on whether the spoken menu is
open:

| Control | Before (upstream) | After (fork) |
|---|---|---|
| BROWSE rotate | `<SelectKnob/>` → `[Library],MoveVertical` | `PioneerDDJ400.browseRotate` (`script.js:238`) |
| BROWSE press | `[Library],MoveFocusForward` | `PioneerDDJ400.browsePress` (`script.js:265`) |
| BROWSE + SHIFT | `[Library],MoveFocusBackward` | `PioneerDDJ400.browseShiftPress` (`script.js:312`) |
| LOAD Deck 1 | `[Channel1],LoadSelectedTrack` | `PioneerDDJ400.loadDeck1` (`script.js:322`) |
| LOAD Deck 2 | `[Channel2],LoadSelectedTrack` | `PioneerDDJ400.loadDeck2` (`script.js:332`) |

Each script handler checks `browseMenuActive()`
(`[AccessMenu],active == 1`, `script.js:234-236`) and routes to
`[AccessMenu]` when open, or the original control when closed. The
press/hold gesture toggles the menu open/closed rather than only opening it
(issue #106, see below); the hold timer is started on press-down and
cancelled on release if it fires before the threshold.

The mapping also auto-raises deck count on init:
`if (engine.getValue("[App]", "num_decks") < deckCount) engine.setValue(...)`
(`script.js:187-188`).

### 3.5 Relative-encoder decoding — Invariant C3

**This was a real, shipped bug.** Converting the browse knob from
`<SelectKnob/>` to `<Script-Binding/>` (§3.4) silently discarded the relative
decode that `midicontroller.cpp` performs for `<SelectKnob/>`. Two successive
fixes were needed:

| Stage | Result |
|---|---|
| Original fork conversion | `engine.setValue("[Library]", "MoveVertical", value)` — raw MIDI byte; one detent could scroll the track list by up to 127 rows |
| First fix | Assumed an offset-64 encoding (`value - 0x40`). Wrong convention: the hardware sends 7-bit two's complement, so `0x01` (one detent up) decoded as `-63` |
| **Shipped fix (issue #47, PR #68, merged)** | Two's-complement decode **and** clamp to ±1 (`script.js:238-262`) |

Current, correct implementation (`Pioneer-DDJ-400-script.js:246-256`):

```js
let delta = value;
if (delta >= 64) { delta -= 128; }
if (delta === 0) { return; }
delta = delta > 0 ? 1 : -1;   // clamp defensively
```

`0x01`-`0x3F` are positive detents (1..63), `0x7F`-`0x40` are negative
(-1..-64) — mirroring exactly what `midicontroller.cpp` does for
`<SelectKnob/>`.

**Invariant C3 — one detent, one step.** A relative encoder bound through a
`<Script-Binding/>` must decode the device's byte convention **in the script**
and emit exactly `±1` per detent, regardless of convention. The clamp is not
belt-and-braces: for a blind user a runaway jump through the track list or the
spoken menu is unrecoverable, because there is no visual anchor to jump back
to.

Regression coverage: `src/test/controllerscriptenginelegacy_test.cpp:135-169`
(`ddj400BrowseRotateDecodesTwosComplementToSingleStep`), which asserts both
the menu-open (`[AccessMenu],navigate`) and menu-closed
(`[Library],MoveVertical`) paths for `0x01` and `0x7F`.

The emulator matches: `tools/ddj400-emulator/ddj400_emulator.py:69-70`
(`BROWSE_UP = (0xB6, 0x40, 0x01)`, `BROWSE_DOWN = (0xB6, 0x40, 0x7F)` — real
two's-complement bytes) and gives the emulator 14-bit tempo pairs
(`tempo()`, `:322`): the real hardware sends the TEMPO fader as an MSB CC
followed by an LSB CC, and `PioneerDDJ400.tempoSliderLSB` only applies the
rate once the LSB arrives, so sending the MSB alone is a silent no-op and both
messages must always be sent, MSB first.

**Invariant C3b — the emulator must be byte-faithful.** Spec 03/04 use the
emulator as the Layer 1 test driver. An emulator that sends a *convenient*
byte convention rather than the *hardware's* convention will pass tests
against a broken decoder — which is precisely how the offset-64 bug survived
a fix.

### 3.6 Recent hardware fixes — issues #106, #109, #110 (PR #129, merged)

Real-hardware testing (not the emulator) surfaced three bugs, all fixed in the
same PR:

**#106 — AccessMenu "locked into the menu, no way out" and dead multi-step
navigation.** Two independent root causes:

1. `engine.beginTimer(...)` for the browse-hold-to-open gesture was missing
   `oneShot=true` (`engine.beginTimer`'s default is repeating, not one-shot).
   The timer never stopped firing after the first hold, silently reopening
   the menu forever. Fixed in `Pioneer-DDJ-400-script.js:285-292`, with the
   fix comment explaining the symptom directly at the call site.
2. `[AccessMenu],navigate`'s `ControlEncoder` had `bIgnoreNops` at its default
   (`true`). The real hardware sends a flat +1/-1 per detent in the same
   direction, so repeated same-direction ticks were silently dropped as
   no-ops after the first — breaking multi-step navigation and wrap-around.
   Fixed in `src/util/accessmenucontroller.cpp:90-105`
   (`m_pNavigate` now explicitly constructed with `bIgnoreNops=false`, with a
   comment explaining why the encoder default is wrong for this control).

The same fix also made the hold gesture **toggle**: hold-to-open when closed,
hold-to-exit when already open, instead of always sending "open" (a no-op
while the menu is already open, per `openMenu()`'s early return) — see the
comment block at `script.js:292-300`.

**#109 — no spoken master-volume readout.** `AnnouncementManager` already
generically announces `[Master],gain` as "Main volume"; the physical MASTER
LEVEL knob was simply never bound. Added in
`Pioneer-DDJ-400.midi.xml:927-953` at `0xB6`/`0x08` (MSB) + `0xB6`/`0x28`
(LSB), inferred from the file's own established convention for the other
shared mixer knobs (headMix `0x0C`/`0x2C`, headGain `0x0D`/`0x2D`). **This
binding has not been confirmed against real hardware** — the PR's own comment
flags it as inferred from convention, not measured. Treat it as needing
hardware validation before relying on it.

**#110 — effects toggle mostly correct, one init gap.** `focused_effect`
(`[EffectRack1_EffectUnit1],focused_effect`) defaulted to `0` (invalid) on a
fresh profile until BEAT LEFT/RIGHT was pressed once. Fixed by initializing it
to `1` in `init()` if it is currently `< 1`
(`script.js:161-170`), leaving a persisted value from a previous session
alone.

**Investigated and NOT bugs / NOT fixed:**

- **#108 (CUE not a toggle)** — determined to be intentional, not a defect.
  `cue_default` → `CueControl::cueCDJ()` is genuine stock Pioneer/CDJ
  hold-to-preview behaviour by design. If this control comes up elsewhere in
  the fork's docs, describe it as intentional stock-matching behaviour.
- **#107 (spontaneous play/reverse)** — **still open, unfixed.** No root
  cause found for the DDJ-400. A parallel look at the Numark Scratch mapping
  found it has no jog/rate/reverse code at all (pure mixer mapping), which
  casts doubt on a "same mechanism as the DDJ-400" framing — if a real
  mechanism exists it is more likely shared VinylControl/DVS decoding than
  either controller's script. Do not describe this as fixed.

---

## 4. Controller mapping layer — Numark Scratch (secondary)

The Numark Scratch has **no dedicated accessibility pad layer** — instead the
fork overloads the existing **shift layer** on a few physical buttons where
that overload costs least (`Numark-Scratch-scripts.js:85-131`).

| Physical control | Fork behaviour | Rationale |
|---|---|---|
| SHIFT (either) | `[Tts],shift` = 1 on press, 0 on release (`:85-96`) | Same layer-feedback contract as the DDJ-400 |
| Shift + CUE (either channel) | `[ChannelN],tts_status` (`:239-249`) | Speaks deck status **without touching PFL state** |
| Pad mode button | `[Tts],pad_mode` ∈ {1, 4, 9} (`:382-387`) | The mode button cycles blind through three states with no tactile or LED cue |
| Shift + Echo (FX unit 1 only) | `[Tts],repeat` (`:129`) | |
| Shift + Delay (FX unit 1 only) | `[BeatClick],enabled` toggle (`:130`) | |
| Shift + Flanger (FX unit 1 only) | `[Master],headSplitDecks` toggle (`:131`) | |

FX **unit 2**'s Shift+button behaviour (toggle that effect) is deliberately
**unchanged** — unit 1's three buttons are individually addressable physical
buttons, unlike the pad-mode selector which cycles blind, so unit 1 is the
cheap place to spend three shift chords, and unit 2 stays a normal effects
unit.

The mapping also fixes a genuine double-fire in `modeButtonPress`
(`Numark-Scratch-scripts.js:348-357`): the single physical mode button fires
once per deck (MIDI channels 5 and 6), and both press and release used to
route to `setMode()`. The fix filters on `(status & 0xF0) === 0x80 ||
value === 0` before calling `setMode`. The `[Tts],pad_mode` announce then
relies on Invariant C1 to deduplicate the remaining both-decks-at-once firing.

**Invariant C5 — layer feedback is a cross-controller contract, not a DDJ-400
feature.** `[Tts],shift` and `[Tts],pad_mode` are set by two independent
mappings and consumed by one handler in `AnnouncementManager`. Their value
vocabulary (§3.1) is shared. Adding a third controller means picking values
from the existing 1-9 table, not extending it ad hoc.

---

## 5. The `Ctrl+Alt` namespace — where things actually stand

**Status: partially resolved by what merged, still contested for what
remains.** Earlier drafts of this spec described a three-way, entirely
unmerged conflict between PRs #71, #76 and #77. That framing is now stale:
**PR #77 merged** (§2.2 Category E) and, in merging, revised its own approach
to mostly avoid `Ctrl+Alt` — leaving a much smaller live surface than any of
the three PRs originally proposed. PR #76 and PR #71 remain open.

### 5.1 What is actually on `Ctrl+Alt` today

18 bindings, verified directly in `res/keyboard/en_US.kbd.cfg`:

| Source | Bindings | Status |
|---|---|---|
| Original fork (Categories C+D) | `beats_set_halve`, `beats_set_double`, `quick_add_to_playlist`, `quick_add_to_crate`, `quantize` × 2 decks = 10 | Shipped, unchanged since the fork's early history |
| Issue #56 leftovers (Category E) | `focused_effect`, `next_effect`, `prev_effect` × 2 decks = 6 | Shipped, each flagged `// TODO(issue #56 follow-up)` pointing at this discussion |
| Upstream, pre-fork | `vinylcontrol_cueing` × 2 decks (`Ctrl+Alt+Y`/`Ctrl+Alt+U`) | Unrelated to accessibility work, not in scope for any of the PRs below |

### 5.2 The two open proposals

**PR #76 (`wt-58-macos-voiceover-chords`, issue #58, open)** — evacuates the
original 10 (not the 6 issue #56 leftovers, which it does not touch). Its
current diff against `HEAD` (re-checked directly, not from stale notes):

| Control | Deck 1: from → to | Deck 2: from → to |
|---|---|---|
| `beats_set_halve` | `Ctrl+Alt+h` → `Alt+F5` | `Ctrl+Alt+Shift+h` → `Alt+Shift+F5` |
| `beats_set_double` | `Ctrl+Alt+d` → `Alt+F6` | `Ctrl+Alt+Shift+d` → `Alt+Shift+F6` |
| `quick_add_to_playlist` | `Ctrl+Alt+p` → `Alt+p` | `Ctrl+Alt+Shift+p` → `Ctrl+Shift+p` |
| `quick_add_to_crate` | `Ctrl+Alt+c` → `Alt+c` | `Ctrl+Alt+Shift+c` → `Ctrl+Shift+c` |
| `quantize` | `Ctrl+Alt+q` → `Alt+F7` | `Ctrl+Alt+Shift+q` → `Alt+Shift+F7` |

Notably, this diff does **not** land on `Alt+h`/`Alt+d`/`Alt+q` even though
those look free at first glance — because issue #56 (§2.2 Category E, already
merged) has since claimed `Alt+h`/`Alt+d` for `pregain_down`/`pregain_up` and
`Alt+q` for EQ-low-down on deck 1. This is a live example of the exact
cross-PR interaction §5's older drafts warned about in the abstract: an
unmerged PR aimed at an old target state needs rework once a sibling PR
lands first. Deck 2's `quick_add` pair also breaks the `Alt+Shift` pattern of
its four siblings, landing on `Ctrl+Shift` instead, because `Alt+Shift+P`/`C`
are already taken by `[Library],AddToCrate`/`AddToPlaylist` (§2.2 Category C).

**PR #71 (`wt-50-keylock-binding`, issue #50, open, green CI)** — adds
keylock/pitch controls. Its current diff, re-checked directly, is **not**
what earlier notes described (it no longer puts everything on `Ctrl+Alt`):

| Control | Deck 1 | Deck 2 |
|---|---|---|
| `keylock` | `Alt+K` | `Alt+Shift+K` |
| `pitch_up` | `Alt+Up` | `Alt+Shift+F1` |
| `pitch_down` | `Alt+Down` | `Alt+Shift+F2` |
| `sync_key` | `Alt+M` | `Alt+Shift+F8` |
| `reset_key` | **`Ctrl+Alt+R`** | **`Ctrl+Alt+Shift+R`** |

Four of the five controls were moved off `Ctrl+Alt` at some point during this
PR's life (matching the direction PR #76 argues for); `reset_key` is the one
holdout still on `Ctrl+Alt`, and it is exactly the chord flagged as a
confirmed VoiceOver collision in §5.3 below. The PR is unapproved but green in
CI as of this writing — it is not blocked on test failures, only on review.

### 5.3 Confirmed screen-reader collisions

macOS VoiceOver's default command modifier is **Control+Option** (`VO`).
Confirmed collisions against what would land if the open PRs merged as-is,
plus what is already live:

| Chord | Use | Source | VoiceOver command | Severity |
|---|---|---|---|---|
| `Ctrl+Alt+R` | `reset_key` (deck 1) | PR #71 (open) | `VO+R` — read row | High |
| `Ctrl+Alt+L` | `focused_effect` (deck 1) | **Already shipped** (§5.1) | `VO+L` — read line | High |
| `Ctrl+Alt+J` | `next_effect` (deck 1) | **Already shipped** (§5.1) | `VO+J` — jump to linked item | High |
| `Ctrl+Alt+H` | `beats_set_halve` (deck 1) | **Already shipped** (§5.1) | `VO+H` — keyboard help on/off | High: swallows all subsequent keys until dismissed |
| `Ctrl+Alt+Y`/`U` | `vinylcontrol_cueing` | Upstream, pre-fork | Not a standard VO command as far as this fork has verified | Low |

Unlike earlier drafts of this section, the most severe collisions
(`Ctrl+Alt+H` = VoiceOver's keyboard-help toggle) are **already shipped and
live today**, not hypothetical. This raises the urgency of §5.2's decision
relative to earlier framings, where the whole `Ctrl+Alt` question was still
abstract.

`X` and `C` chords (`prev_effect`, `quick_add_to_crate`) are not on any
VoiceOver command this fork has confirmed; spot-check before relying on that.

### 5.4 A technical wrinkle worth knowing

Two facts about how Qt delivers these key events, both verified on this
branch:

1. `KeyboardEventFilter::getKeySeq()`
   (`src/controllers/keyboard/keyboardeventfilter.cpp:163-178`) builds the
   sequence string from `Qt::ControlModifier` → `"Ctrl+"` and
   `Qt::MetaModifier` → `"Meta+"`.
2. `Qt::AA_MacDontSwapCtrlAndMeta` is **never set** anywhere in `src/`
   (`grep -rn "AA_MacDontSwapCtrlAndMeta" src/` → no hits; the only
   `setAttribute(Qt::AA…)` calls are `AA_EnableHighDpiScaling`,
   `AA_UseHighDpiPixmaps`, `AA_ShareOpenGLContexts` at `src/main.cpp:184-188`
   and `AA_DontUseNativeMenuBar` at `src/mixxxmainwindow.cpp:206, 1755`).

Under Qt's default on macOS, `Qt::ControlModifier` maps to the **Command** key
and `Qt::MetaModifier` maps to the **Control** key. Taken at face value that
implies `Ctrl+Alt+H` in a `.kbd.cfg` fires on physical **Cmd+Option+H**, which
is also the system "Hide Others" shortcut Qt's Cocoa plugin installs
automatically — pressing it hides Mixxx. The physical **Control+Option+H**
(VoiceOver's `VO+H`) would arrive as `Meta+Alt+H`, and **no `.kbd.cfg` in this
repo binds any `Meta+` sequence** (`grep -rn "Meta+" res/keyboard/` → no
hits), so it is unbound. This does not change the conclusion that the chords
are broken on macOS, but it changes the *mechanism*. Settling this
empirically (running `mixxx --developer` and reading the
`keyboard press: Ctrl+Alt+H`-style log line with VoiceOver on vs. off) still
has not been done, as far as this spec's verification could establish.

Separately: on Windows and Linux, `Ctrl+Alt` *is* AltGr on non-US layouts, and
`Ctrl+Alt+↑`/`↓` (relevant if PR #71 lands as originally proposed) is a GNOME/
KDE workspace-switching shortcut. Both remain true regardless of the
VoiceOver question, and neither has changed since earlier verification.

### 5.5 What this means for the rebase

- The `Ctrl+Alt` question is **narrower** than earlier drafts suggested (18
  live bindings, not a hypothetical 46), but the worst single collision
  (`Ctrl+Alt+H`) is now **live in production**, not proposed.
- Any rebase must preserve the 6 issue-#56-leftover `Ctrl+Alt` bindings
  (`focused_effect`/`next_effect`/`prev_effect` × 2) exactly as they are
  until whatever resolves their TODO comments lands — they are shipped
  behaviour, not a draft.
- PR #76 and PR #71 both target the *current* merged state (their diffs were
  re-verified against `HEAD`, not against a stale base), so either can land
  independently without conflicting with the other's edits — but note PR #71
  still leaves `reset_key` on the exact chord (`Ctrl+Alt+R`) that collides
  with VoiceOver's read-row command, so merging it as-is would not fully
  close this section.

---

## 6. Testing and lint coverage that now exists

Earlier drafts of this spec recommended a `tools/check_kbd_bindings.py` that
did not exist. Some of that gap has since been closed by
`src/test/keyboardbindings_test.cpp` (merged as part of the rebase-guard PR
wave), which runs as a normal ctest target — meaning, unlike a pre-commit
hook, it runs in every `.gitea/workflows/*.yml` build that runs the test
suite, not just at commit time.

| Test | What it checks |
|---|---|
| `EveryShippedLocaleFileLoads` | All 12 files parse via the production `ConfigObject<ConfigValueKbd>` loader |
| `EveryBindingIsWellFormed` | Every chord in every file parses to a non-null `QKeySequence`, modulo `kKnownUnparseableChords` (currently **empty** — issue #95/PR #130 fixed the last known unparseable chord, an el_GR stray-space binding) |
| `AccessibilityBindingsPresentInEveryLocale` | Every control in the 30-entry `forkAccessibilityBindings()` list is bound in all 12 locales |
| `AccessibilityChordsAreIdenticalAcrossLocales` | Those 30 controls are bound to the identical chord in all 12 |
| `EveryReferenceControlIsBoundInEveryLocale` | Every control bound in `en_US` is bound somewhere in every other locale |
| `NoUnexpectedChordCollisions` | Whole-file collision scan, allowlisted against `kKnownUpstreamCollisions` — now down to a **single entry**: the pre-existing fr_FR AZERTY `beatjump_backward`/`beatloop_activate` collision on the plain `a` key |
| `NoAccessibilityChordCollides` | Same scan, but strict — no allowlist, applies only to the 30-entry accessibility set |
| `EngineBackedAccessibilityBindingsResolveToControls` | Spot-checks that the engine-backed subset of accessibility controls (crossfader_lock, headSplitDecks, quantize, beats_set_halve/double on both decks) actually resolves against real `ControlObject`s on a signal-path fixture |
| `ParserAssumptionsStillHold` | Guards the test's own parsing assumptions (comment-artifact handling, `[KeyboardShortcuts]` exclusion) against upstream format changes |

This closes most of what §6 in the earlier draft asked for, with one
remaining gap: this is a **structural** guard (controls present, chords
match across locales, no *new* collisions), not a **policy** guard. It would
not, by itself, catch a new binding landing on `Ctrl+Alt`, `Meta+`, `Insert+`
or `CapsLock+` — nothing currently encodes the modifier-namespace policy this
spec has argued for since its first draft. That remains open work.

`--controller-navigation-without-focus` (Invariant C4, §7) is **no longer**
CLI-only. Issue #64 (PR #83, merged) added a real preference:
`AccessibilitySettings::ControllerNavigationWithoutFocus`
(`src/preferences/accessibilitysettings.h:231-234`, default off), a checkbox
in `dlgprefaccessibilitydlg.ui` ("Allow controller navigation when Mixxx
isn't focused", `:395-412`), and
`LibraryControl::controllerNavigationWithoutFocusAllowed()`
(`src/library/librarycontrol.cpp:996-999`), which ORs the preference with the
CLI flag. A latent null-pointer bug was fixed in the same change:
`LibraryControl::getFocusedWidget()` (`:1038-1051`) used to dereference
`focusWindow` without a null check when nav-without-focus was allowed and no
window had focus; it now checks `focusWindow` before dereferencing at every
call site (`:1002-1003`, `:1040`, `:1104-1105`).

---

## 7. Invariant summary (rebase checklist)

| ID | Invariant | Failure mode if lost | Loud? |
|---|---|---|---|
| **K1** | The 30-control accessibility minimum is identically bound in all 12 `.kbd.cfg` files | Caught by `keyboardbindings_test.cpp` now — was silent before this test existed | **Loud (as of the test wave)** |
| **K2** | Must-always-work actions are `QAction` + `Qt::ApplicationShortcut` in `wmainmenubar.cpp`, reading their key from `m_pKbdConfig` | Speech toggle becomes unreachable once speech or shortcuts are off | **Silent** |
| **K3** | `[Keyboard],Enabled` gates all `.kbd.cfg` bindings; `Custom.kbd.cfg` shadows the shipped file entirely | "The accessibility keys just don't work", with no UI explanation | **Silent** |
| **C1** | `[Tts],pad_mode` speaks only on `valueChanged`; same-value writes are silent | Double speech on hardware that fires per-deck | Audible |
| **C2** | The DDJ-400 accessibility pad layer is opt-in (`accessibilityPads`, default `false`) and disables hot cues while active | Hot cues silently stop working for sighted users if defaulted on | **Silent** |
| **C3** | A relative encoder on a `<Script-Binding/>` decodes its own byte convention and emits exactly ±1 per detent | Runaway scroll through the track list / spoken menu; unrecoverable without sight | Audible but confusing |
| **C3b** | The emulator sends the hardware's real byte convention (two's-complement browse deltas, 14-bit MSB+LSB tempo pairs) | Tests pass against a broken decoder | **Silent** |
| **C4** | `controllerNavigationWithoutFocusAllowed()` (preference OR CLI flag) and its three call sites in `librarycontrol.cpp` (`:1002`, `:1040`(indirect via `getFocusedWidget`), `:1104`) survive together | Controller browsing dies whenever Mixxx loses focus, logging only `qInfo` | **Silent** |
| **C5** | `[Tts],shift` / `[Tts],pad_mode` are a shared cross-controller vocabulary (9 values), not DDJ-400-specific | Third controllers invent conflicting values | Audible |

Six of nine still fail silently, though K1 and C4 are meaningfully better
protected than when this spec was first written — K1 now has a real test, and
C4 now has a discoverable preference instead of a CLI-only flag.

---

## 8. Open questions for review

1. **The `Ctrl+Alt` policy** (§5). Narrower than before, but the worst
   collision is now shipped, not proposed. PR #76 (evacuate) and PR #71 (add
   `reset_key` on the collision chord) are both open and both need a
   maintainer call. Recommend landing PR #76 first, since it only removes
   collision surface, then deciding whether PR #71's `reset_key` needs one
   more chord change before merge.
2. **The 6 issue-#56-leftover `Ctrl+Alt` bindings** (`focused_effect`,
   `next_effect`, `prev_effect`) are marked with follow-up TODOs referencing
   "AccessMenu routing" — i.e. moving them into the spoken value-editor menu
   (Spec 01 addendum, Spec 07) instead of a keyboard chord at all. That
   redesign has not started.
3. **Is the Qt Ctrl↔Cmd swap real on this build?** (§5.4). Still unresolved;
   one `--developer` run would settle it.
4. **The MASTER LEVEL knob binding** (§3.6, issue #109) needs hardware
   validation — it was added by convention, not measured against a real
   DDJ-400.
5. **Issue #107 (spontaneous play/reverse)** remains open with no confirmed
   root cause. Worth tracking whether it recurs on the Numark Scratch despite
   that mapping having no jog/rate code, which would point at
   VinylControl/DVS rather than either script.

# Spec 07 — Spoken menu controller (`AccessMenuController`)

**Status:** Verified current state — rebase contract
**Branch:** `spec-speech` (based on `accessibility-improvements-2026-06-25` @ `2390edf423`)
**Owner:** accessibility fork
**Related:** Spec 01 (DDJ-400 interaction design + value-editor addendum), Spec 03 (DDJ-400 emulator), Spec 05 (speech engine), Spec 06 (announcement layer)

## Purpose

Spec 01 designed the DDJ-400 spoken menu and its interaction model (hold-to-open,
browse-to-scroll, load-to-confirm), and its addendum designed the value editor.
**This spec does not repeat that.** It documents the **controller / state-machine
contract as implemented** — the class, its control objects, its state
transitions, its action dispatch, and what a rebase must not break.

For "why the browse knob opens the menu on a long press", read Spec 01.
For "what happens inside the controller when `[AccessMenu],navigate` fires",
read this.

## Background / current state (verified on this branch)

| Aspect | Value |
|---|---|
| Implementation | `src/util/accessmenucontroller.{h,cpp}` — 215 + 583 lines |
| Instantiated | `src/mixxxmainwindow.cpp:447–459`, inside `initialize()`, immediately after `connectMenuBar()` |
| Action dispatch | `MixxxMainWindow::slotAccessMenuAction()`, `mixxxmainwindow.cpp:1495–1601` |
| Speech callback | injected lambda → `Library::announceText()` (`mixxxmainwindow.cpp:448–452`) |
| Control objects | 7, group `[AccessMenu]` |
| Inactivity timeout | 30 000 ms (`kDefaultTimeoutMs`, `accessmenucontroller.cpp:13`) |
| Menu tree | `buildMenuTree()`, `accessmenucontroller.cpp:146–254` |
| Tests | `src/test/accessmenucontroller_test.cpp`, 456 lines |
| Controller mapping | `res/controllers/Pioneer-DDJ-400-script.js:215–300` |

The class is deliberately decoupled from both `Library` and `DlgPreferences`:

- **Speech** goes through an injected `std::function<void(const QString&)>`, so
  tests install a spy (`accessmenucontroller.h:23–26`).
- **Actions** are emitted as a `actionTriggered(QString actionId)` signal with
  stable string ids; `MixxxMainWindow` maps ids to real handlers. This is
  option (a) from Spec 01 — reuse the `WMainMenuBar` signals — and it is what
  shipped.
- **Preferences** are opened by title through `DlgPreferences::switchToPageByTitle()`
  (`src/preferences/dialog/dlgpreferences.h:67`), except Sound Hardware which
  uses the dedicated `showSoundHardwarePage()`.

Only `UserSettingsPointer` is a hard dependency, and only so config-backed
value items (`[Accessibility],TtsRate`) can be read and written. Passing
`nullptr` degrades those items to read-only at their minimum rather than
crashing (`accessmenucontroller.cpp:519–523`).

## The 7 `[AccessMenu]` control objects

Created in the constructor (`accessmenucontroller.cpp:62–138`). All are owned
`ControlPushButton`s except `navigate`, which is a `ControlEncoder`.

| Control | Type | Mode | Handler | Behaviour |
|---|---|---|---|---|
| `[AccessMenu],open` | `ControlPushButton` | Trigger | `slotOpen()` `:286` | Opens the menu, speaks "Main menu" then the first item |
| `[AccessMenu],close` | `ControlPushButton` | Trigger | `slotClose()` `:290` | Closes, speaks "Menu closed" |
| `[AccessMenu],navigate` | `ControlEncoder` | — | `slotNavigate(double)` `:294` | `> 0` → +1, `< 0` → −1, `== 0` → ignored. **Wraps** modulo the item count |
| `[AccessMenu],activate` | `ControlPushButton` | Trigger | `slotActivate()` `:319` | Descend / fire / enter value edit |
| `[AccessMenu],back` | `ControlPushButton` | Trigger | `slotBack()` `:333` | Up one level, or close at top |
| `[AccessMenu],confirm` | `ControlPushButton` | Trigger | `slotConfirm()` `:347` | Identical to `activate` outside edit mode (redundant, easy-to-hit confirm on the LOAD buttons) |
| `[AccessMenu],active` | `ControlPushButton` | default | — | Read-only state: `1.0` while open. Written only by `openMenu()`/`closeMenu()` |

**Trigger mode is required** on the six input controls: the value is written
repeatedly with the same `1.0`, and a non-Trigger control would emit no change.

`[AccessMenu],active` is the **only** channel a controller mapping has for
knowing whether the menu is open, and the DDJ-400 script polls it on every
browse event (`Pioneer-DDJ-400-script.js:223–225`) to decide whether the knob
drives the menu or the library. Removing or renaming it silently reverts the
menu to unreachable while leaving all seven controls apparently present.

## State machine

```
                    ┌──────────────────────────────┐
                    │        CLOSED                │
                    │  m_open = false              │
                    │  all slots no-op             │
                    └──────────────────────────────┘
                        │ open              ▲
                        ▼                   │ close / back-at-top /
                    ┌──────────────────────┴───┐  30 s timeout /
                    │        BROWSING          │  Action leaf fired
                    │  m_stack = [{menu,idx}]  │
                    │  navigate → move + speak │
                    └──────────────────────────┘
                        │ activate on Value    ▲ confirm/activate ("Set")
                        ▼                      │ back ("Cancelled")
                    ┌──────────────────────────┴─┐
                    │        VALUE EDIT          │
                    │  m_editing = true          │
                    │  navigate → step + speak   │
                    └────────────────────────────┘
```

Every state is guarded by `if (!m_open) return;` at the top of all five input
slots. The controls are inert when the menu is closed, so normal library
browsing is unaffected — this is Spec 01 acceptance criterion 6, enforced in
one place per slot.

### Navigation state

`m_stack` is a `std::vector<MenuState>` where `MenuState = { const
std::vector<Item>* menu; int index; }` (`accessmenucontroller.h:154–157`). The
current menu and highlighted index are **always** `m_stack.back()`. Descending
pushes; `goBack()` pops, or closes when the stack has one entry
(`:413–420`).

`m_stack` holds **raw pointers into `m_root`**, which is a member `std::vector<Item>`
built once in the constructor and never mutated afterwards. Any rebase that makes
the menu tree dynamic (rebuilt on preference change, populated from a plugin
list, …) must also invalidate `m_stack` — otherwise a reallocation of `m_root`
leaves dangling menu pointers.

### Timeout

`m_timeout` is single-shot at 30 s and is restarted by **every** input slot
(`restartTimeout()`, `:361–363`), including navigation inside value-edit mode.
It fires `slotClose()`, which speaks "Menu closed". `setTimeoutMs()`
(`:142–144`) exists solely so tests can shorten it.

### Spoken feedback per transition

| Transition | Spoken |
|---|---|
| Open | "Main menu", then the first item |
| Highlight a `Submenu` | "<label>, submenu" |
| Highlight a `Value` | "<label>, <current value>" |
| Highlight anything else | "<label>" |
| Descend into a submenu | the submenu's first item (its "Back") |
| Enter value edit | "<label>. Turn to change, confirm to set, back to cancel", then the current value |
| Step a value | the new value (or the unchanged boundary value, so the knob never feels dead) |
| Commit | "Set", then the item line again |
| Cancel | "Cancelled", then the item line again |
| Close | "Menu closed" |

Note that several transitions speak **twice in the same call stack**
(`openMenu()` at `:270–271`, `enterValueEdit()` at `:431–433`, `commitValue()`
at `:484–485`, `exitValueEdit()` at `:444–445`). Under the barge-in rule
documented in Spec 05, Invariant 3, **the first of each pair is discarded** —
the user hears only the second.

> **Status: pending, branch `wt-48-tts-queue`.** PR #73 combines each of these
> paired announcements into a single utterance (and adds
> `beginSpeechBatch()`/`endSpeechBatch()` in `AnnouncementManager`). It is
> **not merged**; on this branch the first half of every pair above is
> inaudible. Verified: `grep -rn "beginSpeechBatch" src/` returns nothing.

## Menu tree

Built in `buildMenuTree()` (`accessmenucontroller.cpp:146–254`). Four item
types (`accessmenucontroller.h:31–36`):

| `ItemType` | On activate |
|---|---|
| `Submenu` | Push child menu, speak its first item |
| `Toggle` | `emit actionTriggered(actionId)`, **menu stays open** |
| `Action` | `emit actionTriggered(actionId)`, then `closeMenu()`. `actionId == "back"` is reserved and instead calls `goBack()` |
| `Value` | Enter value-edit mode |

### Root menu (12 items)

| Index | Label | Type | `actionId` |
|---|---|---|---|
| 0 | Back | Action | `back` |
| 1 | Preferences | Submenu | — |
| 2 | Values | Submenu | — |
| 3 | Recording | Toggle | `toggleRecording` |
| 4 | Broadcasting | Toggle | `toggleBroadcasting` |
| 5 | Speech on/off | Toggle | `toggleTts` |
| 6 | Fullscreen | Toggle | `toggleFullScreen` |
| 7 | Keyboard shortcuts | Toggle | `toggleKeyboardShortcuts` |
| 8 | Reload skin | Action | `reloadSkin` |
| 9 | Rescan library | Action | `rescanLibrary` |
| 10 | About | Action | `showAbout` |
| 11 | Quit | Action | `quit` |

### Preferences submenu (13 items)

| Index | Label | `actionId` | Opens |
|---|---|---|---|
| 0 | Back | `back` | — |
| 1 | Sound Hardware | `pref_sound_hardware` | `showSoundHardwarePage()` + `show`/`raise`/`activateWindow` |
| 2 | MIDI Controllers | `pref_midi_controllers` | page title `tr("Controllers")` |
| 3 | Accessibility | `pref_accessibility` | `tr("Accessibility")` |
| 4 | Interface | `pref_interface` | `tr("Interface")` |
| 5 | Decks | `pref_decks` | `tr("Decks")` |
| 6 | Effects | `pref_effects` | `tr("Effects")` |
| 7 | Library | `pref_library` | `tr("Library")` |
| 8 | Recording | `pref_recording` | `tr("Recording")` |
| 9 | Broadcasting | `pref_broadcasting` | `tr("Live Broadcasting")` |
| 10 | Mixer | `pref_mixer` | `tr("Mixer")` |
| 11 | Waveform | `pref_waveform` | `tr("Waveforms")` |
| 12 | Vinyl Control | `pref_vinyl_control` | `tr("Vinyl Control")` |

**Note the label/title mismatches**, which are deliberate and fragile: the
spoken label is "MIDI Controllers" but the page title is "Controllers";
"Broadcasting" → "Live Broadcasting"; "Waveform" → "Waveforms". `switchToPageByTitle()`
matches on the **translated** page title, so an upstream rename of any
preference page — or a change to its translation — silently makes that menu
item do nothing. There is no fallback and no spoken error.

### Values submenu (5 items) — see Spec 01 addendum for the design rationale

Driven by the `kValueControls` table (`accessmenucontroller.cpp:29–46`):

| Index | Label | Backing | Range / step | Format |
|---|---|---|---|---|
| 0 | Back | — | — | — |
| 1 | Speech on/off | `[Tts],enabled` (control) | 0..1, toggle | Boolean → "on"/"off" |
| 2 | Speech rate | `[Accessibility],TtsRate` (**config**) | −10..10, 1 | Integer |
| 3 | Ducking strength | `[Tts],duckStrength` (control) | 0..1, 0.05 | Percent |
| 4 | Beat click volume | `[BeatClick],volume` (control) | 0..1, 0.05 | Percent |

The labels are assigned *positionally* after the loop
(`accessmenucontroller.cpp:216–219`):

```cpp
values[1].label = tr("Speech on/off");
values[2].label = tr("Speech rate");
values[3].label = tr("Ducking strength");
values[4].label = tr("Beat click volume");
```

Adding an entry to `kValueControls` without updating these four lines produces
an item with an **empty spoken label**. There is no compile-time or run-time
check.

### Value-edit semantics

| Input | Effect |
|---|---|
| `navigate` | Booleans **toggle** on every tick regardless of direction (`:455–459`); numerics step by `step`, clamped to `[min, max]`. At a boundary the value is re-spoken so the knob never feels dead (`:468–473`) |
| `activate` / `confirm` | `commitValue()` — exits edit mode, speaks "Set" |
| `back` | `exitValueEdit()` — **restores `m_editStartValue`** captured on entry, speaks "Cancelled" |

`m_editingIndex` is the index within `m_stack.back().menu`, valid only while
`m_editing`. `currentEditingItem()` (`:488–499`) re-validates bounds and type
on every access rather than caching a pointer.

Control-backed values are read and written through a throwaway `ControlProxy`
each time (`readItemValue()` `:517–534`, `writeItemValue()` `:536–550`) — this
is deliberate, because these controls may not exist yet when the menu tree is
built, and `ControlProxy` never re-binds (see Spec 06, Invariant A).

---

# Invariants

## Invariant A — `NoWarnIfMissing` is strictly worse than `AllowMissingOrInvalid`

The two `ControlFlag::NoWarnIfMissing` sites in this class are
**`accessmenucontroller.cpp:530` and `:546`** — inside `readItemValue()` and
`writeItemValue()`, the generic value-editor accessors.

> **Correction to the brief.** The brief stated these two sites reach
> `[Recording],status` and `[Shoutcast],enabled`. **They do not.** Verified:
> `grep -rn "Shoutcast" src/util/` returns nothing, and `[Recording],status`
> appears in `announcementmanager.cpp:492` (with `AllowMissingOrInvalid`), not
> in `accessmenucontroller.cpp`. The code wins; the real state is documented
> below, and the `[Shoutcast]` risk is real but arrives by a different route.

`ControlFlag::NoWarnIfMissing` is defined at `src/control/control.h:24`:

```cpp
NoWarnIfMissing = (1 << 2) | NoAssertIfMissing,
```

It implies `NoAssertIfMissing`, so it inherits the entire silent-failure mode
documented in Spec 06 Invariant A — the proxy binds to a shared dummy control
that never changes — **and additionally suppresses the warning log line**. A
missing control under `AllowMissingOrInvalid` at least leaves a trace in the
Mixxx log a developer can grep for. Under `NoWarnIfMissing` there is
**literally no diagnostic output anywhere**.

The class does hedge: both accessors check `proxy.valid()` before use
(`:531`, `:547`), so a missing control degrades to "reads as `min`, writes are
dropped" rather than reading garbage. That is a real mitigation and should be
preserved. But it is silent: the user turns the knob, hears the minimum value
read back, and has no way to learn the control is gone.

Keys reached this way today, and their owners:

| Key | Owned by | Risk if renamed |
|---|---|---|
| `[Tts],enabled` | `EngineTts` (fork) | Speech on/off item reads/writes nothing |
| `[Tts],duckStrength` | `EngineTts` (fork) | Ducking item pinned at 0 percent |
| `[BeatClick],volume` | `EngineBeatClick` (fork) | Beat-click volume item pinned at 0 percent |

All three are **fork-owned**, so upstream cannot rename them — but a rebase
that drops or renames the engine-side creation (Spec 05) while keeping the menu
would produce exactly this failure, with no output.

**Recommendation:** the guard test proposed in Spec 06
(`src/test/a11ycontrols_test.cpp`, still unwritten on branch
`guard-co-existence`) should cover the `kValueControls` table too, since it is
a compact, machine-readable list of required control keys.

## Invariant B — `[Shoutcast]` and the Broadcasting menu item

The `[Shoutcast]` legacy group name is real and upstream is mid-migration away
from it. `src/broadcast/defs_broadcast.h:3–5`:

```c
// NOTE(rryan): Do not change this from [Shoutcast] unless you also put upgrade
// logic in src/preferences/upgrade.h.
#define BROADCAST_PREF_KEY "[Shoutcast]"
```

`BroadcastManager` creates `[Shoutcast],enabled` and `[Shoutcast],status`
(`src/broadcast/broadcastmanager.cpp:24`, `:31`).

**How the menu actually reaches it (verified):** not by control-object name.
`slotAccessMenuAction()` calls the C++ API
(`mixxxmainwindow.cpp:1561–1568`):

```cpp
#ifdef __BROADCAST__
    if (actionId == QStringLiteral("toggleBroadcasting")) {
        if (m_pCoreServices->getBroadcastManager()) {
            emit m_pMenuBar->toggleBroadcasting(
                    !m_pCoreServices->getBroadcastManager()->isEnabled());
        }
        return;
    }
#endif
```

`BroadcastManager::isEnabled()` (`broadcastmanager.cpp:80–82`) reads its own
owned control, so a `[Shoutcast]` → `[Broadcast]` rename inside `BroadcastManager`
would rename creator and reader together and the menu would keep working.

**The Broadcasting menu item is therefore not vulnerable to the CO rename.
It is vulnerable to two other things, and both are silent:**

1. **`#ifdef __BROADCAST__`.** On a build without broadcasting compiled in, the
   menu item still exists in the tree (`accessmenucontroller.cpp:234–236` is
   unconditional), the `Toggle` fires, dispatch falls through every branch, and
   execution reaches `qWarning() << "Unknown access menu action:" << actionId;`
   (`mixxxmainwindow.cpp:1596`). The user hears **nothing at all** — no error,
   no state change, and because it is a `Toggle` the menu does not even close.
   Same failure for a build *with* `__BROADCAST__` but a null `BroadcastManager`,
   which returns silently at `:1566`.
2. **Consumers that *do* hardcode the name.** `src/controllers/controlpickermenu.cpp:1586`
   registers `[Shoutcast]` for controller mappings, and any user mapping that
   binds `[Shoutcast],enabled` breaks on the migration. That is outside this
   class but inside the fork's blast radius.

**Rebase action:** if upstream completes the `[Shoutcast]` migration, grep the
whole tree (`grep -rn "Shoutcast" src/ res/`) rather than trusting that the
menu still works, and add a spoken fallback for the unknown-action path at
`mixxxmainwindow.cpp:1596` — a `qWarning()` is invisible to the only user this
feature exists for. The same argument applies to `toggleRecording`, which
returns silently when `getRecordingManager()` is null (`:1553–1559`).

## Invariant C — menu indices are positional and hard-coded in tests

`src/test/accessmenucontroller_test.cpp` navigates by **counting `navigate`
ticks**, with the target item identified only by a comment. Examples:

| Test line | Code | Assumes |
|---|---|---|
| `:92–95` | four `navigate(1.0)` calls commented `// Preferences`, `// Values`, `// Recording`, `// Broadcasting` | root order, indices 1–4 |
| `:210–213` | `// Navigate to Quit (last item, index 11 of 12).` then `for (int i = 0; i < 11; ++i)` | root has **exactly 12** items and Quit is last |
| `:297–303` | two ticks to reach Values, then a counted descent | root indices 1–2 |
| `:388` | `navigate(1.0); // next item: Ducking strength` | Values submenu order |

Consequences:

- **Reordering the tree breaks the tests** — which is the good case: it is
  loud, and it is the only automated guard on menu structure that exists.
- **Adding an item at the end of the root menu** breaks the Quit test at `:210`
  in an obviously-diagnosable way.
- **Adding an item in the middle** makes several tests silently assert against
  the *wrong item*, since they only check that *something* was triggered or
  spoken. Read the test comments, not just the tick counts.

The tests are the specification of the menu order. Treat any change to
`buildMenuTree()` as an API change.

## Invariant D — action ids are a stable public contract

`actionTriggered(QString)` ids are matched by string literal in
`slotAccessMenuAction()`. They are not enum values, there is no shared header,
and the compiler cannot catch a typo on either side. The complete set is the
`actionId` column of the three tables above, plus the reserved `"back"`.

A mismatch produces `qWarning() << "Unknown access menu action:"` and, again,
**silence for the user**.

Where the ids overlap `WMainMenuBar` signal names (`toggleRecording`,
`toggleBroadcasting`, `toggleTts`, `toggleFullScreen`,
`toggleKeyboardShortcuts`, `reloadSkin`, `rescanLibrary`, `showAbout`) that
is deliberate — Spec 01's option (a) — so the same handler serves the mouse
menu and the spoken menu. Keep them aligned.

## Invariant E — the speak callback must stay injected

`MixxxMainWindow` passes a lambda that calls `Library::announceText()` after a
null check (`mixxxmainwindow.cpp:448–452`). Because `announceText()` routes
through `slotQuickPickerItemHighlighted()`, which is **ungated** (Spec 06),
menu speech is never suppressed by an `[Accessibility]` preference — correct,
since opening the menu is a deliberate act.

It does, however, mean menu speech is subject to everything in Spec 05: it is
silent before a sound device opens, it is discarded by barge-in, and it is
recorded in `--tts-log` whether audible or not.

Do not replace the injected callback with a direct `Library*` dependency during
a rebase; the entire 456-line test suite depends on the spy.

## Cross-reference: the DDJ-400 mapping side

Owned by the mapping, not this class. Documented here only as the contract
boundary — see Spec 01 for the interaction design.

| Gesture | Script function | Writes |
|---|---|---|
| Browse rotate | `browseRotate` `:227` | `[AccessMenu],navigate` when open, else `[Library],MoveVertical` |
| Browse press, short (< 400 ms) | `browsePress` `:253` | `[AccessMenu],activate` when open, else `[Library],MoveFocusForward` |
| Browse press, hold (≥ 400 ms) | `browsePress` timer `:275` | `[AccessMenu],open` |
| Browse + shift | `browseShiftPress` `:285` | `[AccessMenu],back` when open, else `[Library],MoveFocusBackward` |
| LOAD deck 1 / 2 | `loadDeck1` / `loadDeck2` `:292` | `[AccessMenu],confirm` when open, else `LoadSelectedTrack` |

`browseHoldThreshold = 0.4` seconds (`Pioneer-DDJ-400-script.js:215`) — the
value Spec 01 proposed, unchanged after prototyping.

The script decodes the browse encoder as a **7-bit two's-complement delta**
(`:228–241`, issue #47): `0x01–0x3F` = +1..+63, `0x7F–0x40` = −1..−64. It must
do this by hand because the control is bound through a Script-Binding (needed
to branch on `[AccessMenu],active`) rather than the `<SelectKnob/>` MIDI option
that `midicontroller.cpp` would otherwise decode. It then clamps to ±1
defensively, so a stray multi-step message can never send a runaway jump
through the track list or the spoken menu.

## Rebase checklist for this spec

1. All 7 `[AccessMenu]` controls present; the six input controls still
   `ButtonMode::Trigger`; `navigate` still a `ControlEncoder`.
2. `[AccessMenu],active` still written by `openMenu()`/`closeMenu()` — the DDJ
   script's only way to know the menu is open.
3. `if (!m_open) return;` guard present in all five input slots.
4. Root menu still 12 items in the documented order; Preferences 13; Values 5.
   Run `accessmenucontroller_test.cpp` and **read the failures**, don't just
   renumber the ticks.
5. `values[1..4].label` assignments still match the `kValueControls` order.
6. Every `switchToPageByTitle()` string still matches a real `DlgPreferences`
   page title after the rebase — including the three deliberate mismatches
   (Controllers / Live Broadcasting / Waveforms).
7. `m_stack` still holds pointers into an immutable `m_root`.
8. 30 s timeout restarted by every input, including inside value-edit mode.
9. Value-edit cancel still restores `m_editStartValue`.
10. Consider (new work, not preservation) adding a spoken fallback on the
    unknown-action path at `mixxxmainwindow.cpp:1596` and on the null-manager
    early returns at `:1553` and `:1562`. A `qWarning()` is invisible to the
    only user this feature exists for.

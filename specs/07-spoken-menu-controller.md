# Spec 07 — Spoken menu controller (`AccessMenuController`)

**Status:** Verified current state — rebase contract (refreshed against the current PR wave)
**Branch:** `spec-speech` — content re-verified against the merged target-state tree
(`scratch-target-state` @ `4a9641692c`: the `accessibility-improvements-2026-06-25`
tip plus all 19 other currently-open accessibility PRs merged together, with the
cross-PR bugs that merge surfaced found and fixed). 141 commits landed on top of
the previous verification point, `2390edf423`.
**Owner:** accessibility fork
**Related:** Spec 01 (DDJ-400 interaction design + value-editor addendum), Spec 03 (DDJ-400 emulator), Spec 05 (speech engine), Spec 06 (announcement layer)

## Purpose

Spec 01 designed the DDJ-400 spoken menu and its interaction model (hold-to-open,
browse-to-scroll, load-to-confirm), and its addendum designed the value editor.
**This spec does not repeat that.** It documents the **controller / state-machine
contract as implemented** — the class, its control objects, its state
transitions, its action dispatch, and what a rebase must not break.

The previous verification described a controller reachable only from the
DDJ-400's browse-knob hold gesture. **That is no longer true.** The single
largest change since then (issue #57) gives the menu a second, always-on
keyboard entry point and teaches every toggle item to speak its own current
state — both are now core to what this class does, not an addendum.

For "why the browse knob opens the menu on a long press", read Spec 01.
For "what happens inside the controller when `[AccessMenu],navigate` fires, or
when Alt+Shift+M is pressed instead," read this.

## Background / current state

| Aspect | Value |
|---|---|
| Implementation | `src/util/accessmenucontroller.{h,cpp}` — 250 + 677 lines (was 215 + 583) |
| Instantiated | `src/mixxxmainwindow.cpp:467–474`, inside `initialize()`, immediately after `connectMenuBar()` (`:462`) |
| Action dispatch | `MixxxMainWindow::slotAccessMenuAction()`, `mixxxmainwindow.cpp:1583–1689` |
| Speech callback | injected lambda → `Library::announceText()` (`mixxxmainwindow.cpp:468–471`) |
| Control objects | still 7, group `[AccessMenu]` — **plus 6 application-wide Qt shortcuts that bypass the CO layer entirely (new, issue #57)** |
| Inactivity timeout | 30 000 ms (`kDefaultTimeoutMs`, `accessmenucontroller.cpp:13`) |
| Menu tree | `buildMenuTree()`, `accessmenucontroller.cpp:146–290` |
| Tests | `src/test/accessmenucontroller_test.cpp`, 590 lines (was 456) |
| Controller mapping | `res/controllers/Pioneer-DDJ-400-script.js:211–312` |
| Guard test | `src/test/a11ycontrols_test.cpp:536–561` (new — the seven `[AccessMenu]` controls are now existence-checked in CI) |

The class is still deliberately decoupled from both `Library` and
`DlgPreferences`:

- **Speech** goes through an injected `std::function<void(const QString&)>`,
  unchanged.
- **Actions** are still emitted as `actionTriggered(QString actionId)` with
  stable string ids, mapped by `MixxxMainWindow`.
- **Preferences** are still opened by title through
  `DlgPreferences::switchToPageByTitle()`, except Sound Hardware.
- **New:** fullscreen state is now pushed *in* from `MixxxMainWindow` via
  `setFullScreenState(bool)` (`accessmenucontroller.h:167`,
  `accessmenucontroller.cpp:412–414`), wired to `MixxxMainWindow::fullScreenChanged`
  (`mixxxmainwindow.cpp:484–488`) — the first case of the controller tracking
  live external state rather than only reading a `ControlProxy` or config key
  on demand.

Only `UserSettingsPointer` is still a hard dependency, for the same reason as
before (config-backed value items). Unchanged.

## Two independent ways in: hardware controls and always-on keyboard chords (issue #57)

This is the architectural headline of this verification pass. Before issue
#57, the **only** way to reach the menu was the DDJ-400's hold-to-open
gesture driving the seven `[AccessMenu]` control objects below. That path is
completely unchanged — but it is no longer the only path.

### The 7 `[AccessMenu]` control objects (unchanged)

Created in the constructor (`accessmenucontroller.cpp:62–138`). All are owned
`ControlPushButton`s except `navigate`, which is a `ControlEncoder`.

| Control | Type | Mode | Handler | Behaviour |
|---|---|---|---|---|
| `[AccessMenu],open` | `ControlPushButton` | Trigger | `slotOpen()` `:325–327` | Opens the menu, speaks "Main menu" then the first item |
| `[AccessMenu],close` | `ControlPushButton` | Trigger | `slotClose()` `:329–331` | Closes, speaks "Menu closed" |
| `[AccessMenu],navigate` | `ControlEncoder` | — | `slotNavigate(double)` `:333–356` | `> 0` → +1, `< 0` → −1, `== 0` → ignored. **Wraps** modulo the item count |
| `[AccessMenu],activate` | `ControlPushButton` | Trigger | `slotActivate()` `:358–370` | Descend / fire / enter value edit |
| `[AccessMenu],back` | `ControlPushButton` | Trigger | `slotBack()` `:372–384` | Up one level, or close at top |
| `[AccessMenu],confirm` | `ControlPushButton` | Trigger | `slotConfirm()` `:386–398` | Identical to `activate` outside edit mode |
| `[AccessMenu],active` | `ControlPushButton` | default | — | Read-only state: `1.0` while open. Written only by `openMenu()`/`closeMenu()` |

Unreachable-without-COs risk is unchanged: `[AccessMenu],active` is still the
**only** channel a controller mapping has for knowing whether the menu is
open, and the DDJ-400 script still polls it on every browse event
(`Pioneer-DDJ-400-script.js:223–225`).

### New — 6 application-wide keyboard shortcuts that call the controller's slots directly (issue #57)

`MixxxMainWindow::initialize()` (`mixxxmainwindow.cpp:490–539`) builds six
`QAction`s with `Qt::ApplicationShortcut` context and connects them **straight
to `AccessMenuController`'s public slots** — not through a `ControlObject`,
not through `KeyboardEventFilter`, not gated by "Enable Keyboard Shortcuts" at
all:

```cpp
// mixxxmainwindow.cpp:506-515 (abbreviated)
auto makeAccessMenuAction = [this, pKbdConfig](const QString& item, const QString& defaultKeySequence) {
    auto* pAction = new QAction(this);
    pAction->setShortcut(QKeySequence(pKbdConfig->getValue(
            ConfigKey(QStringLiteral("[AccessMenu]"), item), defaultKeySequence)));
    pAction->setShortcutContext(Qt::ApplicationShortcut);
    addAction(pAction);
    return pAction;
};
```

| Action | Default chord | Connects to | Default read from |
|---|---|---|---|
| `toggle` | `Alt+Shift+M` | `AccessMenuController::slotToggle` (new — `accessmenucontroller.cpp:404–410`) | `mixxxmainwindow.cpp:516–519` |
| `navigateUp` | `Alt+Shift+Up` | `slotNavigate(-1.0)` (lambda) | `:520–523` |
| `navigateDown` | `Alt+Shift+Down` | `slotNavigate(1.0)` (lambda) | `:524–527` |
| `activate` | `Alt+Shift+Return` | `slotActivate` | `:528–531` |
| `back` | `Alt+Shift+Backspace` | `slotBack` | `:532–535` |
| `confirm` | `Alt+Shift+Space` | `slotConfirm` | `:536–539` |

`slotToggle()` (`accessmenucontroller.cpp:404–410`) is genuinely new — it
opens the menu if closed, closes it if open, because an always-on keyboard
chord has no notion of "menu focus" to decide open-vs-close the way the
hardware hold-gesture does (which only ever means "open").

**Why this bypasses the normal kbd.cfg → ControlObject path deliberately**
(comment `mixxxmainwindow.cpp:490–504`): these six chords must keep working
even when "Enable Keyboard Shortcuts" is turned off — including by this very
menu's own Keyboard-shortcuts Toggle item. Qt's application-wide shortcut map
consumes the key press before `KeyboardEventFilter` (and its kbd.cfg-driven
`ControlObject` bindings) ever sees it, so a keyboard-only DJ who has just
disabled shortcuts from inside this menu can still reach it afterwards to turn
them back on. `AccessMenuController::activateCurrentItem()` reinforces this
at the moment of danger: it speaks a warning **before** actually firing the
`toggleKeyboardShortcuts` action, specifically because that's the one toggle
in the menu that could otherwise strand a keyboard-only user
(`accessmenucontroller.cpp:470–481`, see "New — spoken toggle state" below).

**Default key sequences are still overridable** the same way as any other
kbd.cfg entry — `pKbdConfig->getValue(ConfigKey("[AccessMenu]", item), default)`
reads a per-user override if present, falling back to the hardcoded default
above. `res/keyboard/en_US.kbd.cfg:274–289` ships the defaults (and the same
comment explaining why they live outside the normal binding path), replicated
across all 12 locale files.

**Net effect:** the six input slots (`slotNavigate`, `slotActivate`,
`slotBack`, `slotConfirm`, plus open/close via `slotToggle`) each now have
**two independent trigger paths** — a hardware-driven `ControlObject` (for the
DDJ-400 mapping) and an always-on Qt shortcut calling the C++ slot directly
(for the keyboard). Both funnel into the same state machine below; neither
knows the other exists.

### A discovered bug: four unrelated shortcuts are misfiled into `[AccessMenu]`'s kbd.cfg section

Reading `res/keyboard/en_US.kbd.cfg:274–293` closely turns up a real, verified
defect that is **not** part of this class but sits inside the section this
spec documents, so it's worth flagging rather than silently working around:

```
[AccessMenu]
...
confirm Alt+Shift+Space
sort_column_next Alt+Shift+s
sort_column_prev Alt+Shift+j
sort_order Alt+Shift+o
show_column_menu Alt+Shift+v
```

The last four lines bind `[AccessMenu],sort_column_next` etc. — but the
actual `ControlObject`s these are meant to drive are `[Library],sort_column_next`,
`[Library],sort_column_prev`, `[Library],sort_order`, and
`[Library],show_column_menu`, created by `LibraryControl`
(`src/library/librarycontrol.cpp:387–399, 555`). `[Library]`'s own kbd.cfg
section (`:269–272`) does **not** contain these four bindings. Nothing in
`src/` reads `ConfigKey("[AccessMenu]", "sort_column_next")` as an
application-wide shortcut the way the six real `[AccessMenu]` chords above
are read — these four lines currently bind to nothing and are dead at
runtime, most plausibly a merge artifact from library-sort-shortcut work
(issue #59 territory) landing its kbd.cfg lines under the wrong, adjacent
section header.

This is **not caught** by any existing guard test:
`keyboardbindings_test.cpp`'s `forkAccessibilityBindings()` list (the table
`AccessibilityBindingsPresentInEveryLocale`/`AccessibilityChordsAreIdenticalAcrossLocales`
check against) does not include these four keys at all, and the generic
chord-collision test only checks for two controls sharing one chord, not for
a chord binding to a group with no real consumer. This is genuinely Spec
08/09's territory (library keyboard input, not the spoken menu), so it is
noted here only as a pointer: **fix by moving the four lines under `[Library]`
in all 12 locale files**, and add them to (or otherwise cover them in) the
locale-parity guard.

## State machine

Unchanged in shape and in every transition rule from the previous
verification:

```
                    ┌──────────────────────────────┐
                    │        CLOSED                │
                    │  m_open = false              │
                    │  all slots no-op             │
                    └──────────────────────────────┘
                        │ open / toggle     ▲
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

Every state is still guarded by `if (!m_open) return;` at the top of all five
input slots (`slotNavigate`, `slotActivate`, `slotBack`, `slotConfirm`; and
implicitly `slotOpen`/`slotClose`/`slotToggle`, which manage `m_open` itself).
The always-on keyboard chords reach the same guarded slots, so the controls
remain inert when the menu is closed regardless of entry path — Spec 01
acceptance criterion 6 still holds, enforced in one place per slot.

### Navigation state (unchanged)

`m_stack` is still a `std::vector<MenuState>` of raw pointers into an
immutable `m_root`, built once in the constructor. The dangling-pointer hazard
if a rebase ever made the tree dynamic is unchanged — see the Rebase
checklist.

### Timeout (unchanged)

Still single-shot at 30 s, restarted by every input slot including inside
value-edit mode, `setTimeoutMs()` exists for tests.

### The double-speak bug from the previous verification is now fixed, in this class specifically

The previous verification's Invariant listed several transitions that spoke
**twice in the same call stack** — `openMenu()`, `enterValueEdit()`,
`commitValue()`, `exitValueEdit()` — and noted the general fix
(`beginSpeechBatch()`/`endSpeechBatch()`, issue #48) was "pending, branch
`wt-48-tts-queue`, not merged." That general mechanism has since landed in
`AnnouncementManager` (see Spec 05, Invariant 3) — but `AccessMenuController`
fixes its own four cases independently, by combining what used to be two
`speak()` calls into **one** formatted string per transition, rather than
calling the general batching API:

| Transition | Now a single `speak()` call at |
|---|---|
| Open | `"Main menu"` or `"Main menu. %1"` — `accessmenucontroller.cpp:309–310` |
| Enter value edit | `"%1. Turn to change, confirm to set, back to cancel. %2"` — `:521–522` |
| Cancel (back out of edit) | `"Cancelled"` or `"Cancelled. %1"` — `:536–537` |
| Commit (confirm/activate in edit) | `"Set"` or `"Set. %1"` — `:578–579` |

Both fixes for issue #48 — this class's local string-combining, and
`AnnouncementManager`'s general `beginSpeechBatch()`/`endSpeechBatch()` — are
independent and both must be preserved; see Spec 05.

### Spoken feedback per transition (updated)

| Transition | Spoken |
|---|---|
| Open (via `open` CO **or** the `toggle` keyboard chord) | "Main menu", then the first item, in one utterance |
| Highlight a `Submenu` | "<label>, submenu" |
| Highlight a `Value` | "<label>, <current value>" |
| Highlight a `Toggle` **(new, issue #57)** | "<label>, <current on/off state>" — see below; falls back to bare "<label>" if there is no known state source |
| Highlight anything else | "<label>" |
| Descend into a submenu | the submenu's first item (its "Back") |
| Enter value edit | instructions + current value, one utterance |
| Step a value | the new value (or the unchanged boundary value, so the knob never feels dead) |
| Commit | "Set" + the item line, one utterance |
| Cancel | "Cancelled" + the item line, one utterance |
| Close (via `close` CO **or** the `toggle` keyboard chord) | "Menu closed" |

## New — spoken toggle state (issue #57)

Previously, `Toggle` items in the menu (Recording, Broadcasting, Speech
on/off, Fullscreen, Keyboard shortcuts) fired an action but said nothing
about the state they were toggling *into* or *out of* — the DJ had to already
know the state, or trigger it and listen for a side effect elsewhere. Now,
four of the five root-menu toggles carry a state descriptor and speak it every
time they're highlighted, using the same `ValueItem` read/format plumbing the
Values submenu already used for numeric settings:

```cpp
// accessmenucontroller.h:117-129 (abbreviated)
// A Toggle item that also carries a state descriptor (issue #57): reusing
// the ValueItem's control/config plumbing lets the menu speak the toggle's
// current on/off state the same way Value items speak their current value,
// without a second read/format code path.
Item(ItemType type, const QString& label, const QString& actionId, const ValueItem& stateValue);
```

`toggleStateText()` (`accessmenucontroller.cpp:448–457`) special-cases
Fullscreen (reads `m_fullscreenState`, pushed in from `MixxxMainWindow` — no
CO or config key exists for it) and otherwise reads the item's `ValueItem`
through the same `readItemValue()`/`formatItemValue()` path Values-submenu
items use. An item with no known state source (`value.group.isEmpty() &&
!value.configBacked`) speaks its bare label — this is the fallback a rebase
gets automatically if a new Toggle is added without a state source, so it
fails soft, not silently-wrong.

### Root menu (12 items, order unchanged) — now with state sources

| Index | Label | Type | `actionId` | State source (new) |
|---|---|---|---|---|
| 0 | Back | Action | `back` | — |
| 1 | Preferences | Submenu | — | — |
| 2 | Values | Submenu | — | — |
| 3 | Recording | Toggle | `toggleRecording` | `[Recording],status`, 0..2, Boolean (`v > 0` → "on") |
| 4 | Broadcasting | Toggle | `toggleBroadcasting` | `[Shoutcast],enabled`, 0..1, Boolean — **see Invariant B** |
| 5 | Speech on/off | Toggle | `toggleTts` | `[Tts],enabled`, 0..1, Boolean |
| 6 | Fullscreen | Toggle | `toggleFullScreen` | `m_fullscreenState` (no CO/config key — pushed in) |
| 7 | Keyboard shortcuts | Toggle | `toggleKeyboardShortcuts` | `[Keyboard],Enabled` (**config-backed**) |
| 8 | Reload skin | Action | `reloadSkin` | — |
| 9 | Rescan library | Action | `rescanLibrary` | — |
| 10 | About | Action | `showAbout` | — |
| 11 | Quit | Action | `quit` | — |

Built at `accessmenucontroller.cpp:222–289`; still confirmed exactly 12 items
by `src/test/accessmenucontroller_test.cpp:213` ("Navigate to Quit (last item,
index 11 of 12)").

**A new safety behaviour worth calling out specifically:** before firing
`toggleKeyboardShortcuts` while it's currently on, `activateCurrentItem()`
speaks a warning first (`accessmenucontroller.cpp:474–481`):

> "Keyboard shortcuts now off. Use this menu or the mouse to re-enable them."

This is the concrete payoff of the always-on keyboard chords documented
above: this is the one toggle in the entire menu that could otherwise strand
a keyboard-only DJ, and the warning fires **before** the toggle actually
takes effect, using whatever input path (CO or keyboard chord) triggered it.

### Preferences submenu (13 items) — unchanged

Same 13 items, same order, same three deliberate label/title mismatches
(MIDI Controllers→Controllers, Broadcasting→Live Broadcasting,
Waveform→Waveforms) as the previous verification. Built at
`accessmenucontroller.cpp:149–186`.

### Values submenu (5 items) — unchanged

Same `kValueControls` table (`accessmenucontroller.cpp:29–46`), same four
entries (Speech on/off, Speech rate, Ducking strength, Beat click volume),
same positional label assignment (`:216–219`) with the same "adding an entry
without updating the four label lines produces an empty spoken label, with no
compile-time or run-time check" risk.

### Value-edit semantics — unchanged

Same rules: booleans toggle on every tick regardless of direction, numerics
step and clamp, boundary re-speaks so the knob never feels dead, `back`
restores `m_editStartValue`. Read/write still goes through a throwaway
`ControlProxy` each time (`readItemValue()`/`writeItemValue()`,
`accessmenucontroller.cpp:611–628`/`:630–644`) for the same reason as before
— these controls may not exist yet when the menu tree is built, and
`ControlProxy` never re-binds.

---

# Invariants

## Invariant A — `NoWarnIfMissing` is strictly worse than `AllowMissingOrInvalid`, and it now reaches more keys

The two `ControlFlag::NoWarnIfMissing` sites are now **`accessmenucontroller.cpp:623`
and `:640`**, inside `readItemValue()` and `writeItemValue()` — the same two
generic accessors as before, just at new line numbers. The flag's definition
and behaviour (`src/control/control.h:24`) are unchanged: it inherits the
entire silent-failure mode from Spec 06's Invariant A **and** additionally
suppresses the warning log line, so a missing control leaves **no diagnostic
trace anywhere**. The `proxy.valid()` hedge before every read/write is
unchanged and still the only mitigation: a missing control degrades to "reads
as `min`, writes are dropped," not garbage — but silently.

**Keys reached this way have grown**, because the new Toggle state feature
(above) routes through this exact same code path — `toggleStateText()` calls
`readItemValue()` for every non-Fullscreen toggle, in addition to the
pre-existing Values submenu:

| Key | Reached from | Owned by | Risk if renamed |
|---|---|---|---|
| `[Tts],enabled` | Values item 1 **and** root Toggle item 5 (same key, two menu locations) | `EngineTts` (fork) | Speech on/off item and toggle state both read/write nothing |
| `[Tts],duckStrength` | Values item 3 | `EngineTts` (fork) | Ducking item pinned at 0 percent |
| `[BeatClick],volume` | Values item 4 | `EngineBeatClick` (fork) | Beat-click volume item pinned at 0 percent |
| `[Recording],status` | Root Toggle item 3 (Recording state) | **upstream** (`RecordingManager`) | Recording toggle always speaks "off" regardless of actual state |
| `[Shoutcast],enabled` | Root Toggle item 4 (Broadcasting state) | **upstream**, and only compiled in with `__BROADCAST__` (`coreservices.cpp:560–561`) | See Invariant B below — this one is new and has a compounding risk |

The first three are fork-owned, as before. The last two are **new to this
verification and upstream-owned** — a real change in the risk profile from
the previous verification's "all three are fork-owned, so upstream cannot
rename them." `[Recording],status` is also independently observed by
`AnnouncementManager` with the louder `AllowMissingOrInvalid` flag (Spec 06)
— two different silent-failure modes now hang off the same upstream-owned key
from two different classes.

**Recommendation, updated:** `src/test/a11ycontrols_test.cpp` (Spec 06,
Invariant A) now exists and does cover the seven `[AccessMenu]` control
objects, but its comment at `:625–636` still claims nothing reads
`[Shoutcast],enabled` — see Invariant B immediately below, this is now
concretely wrong and should be fixed alongside adding the key to the guard
table. The `kValueControls` table and the root-menu Toggle state sources are
both compact, machine-readable lists a guard test could walk directly instead
of hand-duplicating.

## Invariant B — `[Shoutcast]` and the Broadcasting menu item (updated — the guard test is now stale, not the code)

The `[Shoutcast]` legacy group name situation is unchanged from the previous
verification: `src/broadcast/defs_broadcast.h:3–5` still forbids renaming it
without upgrade logic, `BroadcastManager` still creates
`[Shoutcast],enabled`/`status` (`src/broadcast/broadcastmanager.cpp:23–24,31`),
`BroadcastManager::isEnabled()` (`:80–82`) still reads its own control, and
`slotAccessMenuAction()` still dispatches Broadcasting by calling the C++ API
directly (`mixxxmainwindow.cpp:1652–1660`), not by control-object name — so
the *action* path (toggling broadcasting on/off) is exactly as robust to a
`[Shoutcast]` rename as before.

**What changed: the item now also reads state, and that read path is
different from the action path.** `toggleStateText()` formats the Broadcasting
Toggle's spoken on/off suffix by reading `[Shoutcast],enabled` through
`readItemValue()` → a throwaway `ControlProxy` with `NoWarnIfMissing`
(Invariant A). This is genuinely new since the previous verification (the
Broadcasting item didn't speak state at all before issue #57), and it is
**not covered by any guard test**:

- `a11ycontrols_test.cpp`'s own trailing comment (`:629–636`) explicitly
  reasons "no accessibility code reads it" and therefore deliberately omits
  `[Shoutcast],enabled` from its table. **That reasoning no longer holds** —
  `AccessMenuController::toggleStateText()` reads it every time the
  Broadcasting item is highlighted, via exactly the `NoWarnIfMissing` path
  the same test file's own header comment (`:31–33`) calls out as the worst
  of the fork's silent-failure modes.
- Compounding this: `BroadcastManager` — and therefore `[Shoutcast],enabled`
  itself — only exists when Mixxx is compiled with `__BROADCAST__`
  (`coreservices.cpp:560–561`). `AccessMenuController::buildMenuTree()` adds
  the Broadcasting Toggle's state `ValueItem` **unconditionally**, with no
  `#ifdef` guard. On a non-broadcast build the read degrades gracefully (the
  proxy is invalid, so it reads as `value.min`, i.e. "off") — not a crash,
  but a build where the feature doesn't exist at all will still confidently
  announce "Broadcasting, off" rather than something like "unavailable."

**This is exactly the kind of gap this spec-verification pass exists to
catch**, so it's recorded here as a concrete action item rather than folded
silently into the "unchanged" Invariant B from the previous verification:
add `[Shoutcast],enabled` to `a11ycontrols_test.cpp`'s table (conditionally on
`__BROADCAST__`, matching how the control itself is conditionally created),
and update the stale comment.

The rest of the previous Invariant B is unchanged: the menu item is not
vulnerable to a `[Shoutcast]` rename for its *action*, only for its *state
readout* (new) and for the two pre-existing risks — the unconditional menu
item on a non-`__BROADCAST__` build combined with the unknown-action path
still just `qWarning()`s (`mixxxmainwindow.cpp:1688`, unchanged), and
`src/controllers/controlpickermenu.cpp:1586` still hardcodes `[Shoutcast]` for
controller mappings. `toggleRecording` similarly still returns silently when
`getRecordingManager()` is null (`mixxxmainwindow.cpp:1645–1650`) — this
recommendation from the previous verification (add a spoken fallback on the
unknown-action path and the null-manager early returns) has **not** been
acted on; it remains open work, not a regression.

## Invariant C — menu indices are positional and hard-coded in tests

Unchanged in kind, and the test file has grown substantially (456 → 590
lines) with new tests covering the same positional-navigation pattern for the
new `ToggleStateTest` fixture (e.g. `Recording_SpeaksOnState`,
`Tts_SpeaksOffThenOnState`, `Fullscreen_SpeaksStatePushedFromMainWindow`,
`KeyboardShortcuts_SpeaksConfigBackedState`,
`DisablingKeyboardShortcuts_SpeaksWarningBeforeToggling`,
`EnablingKeyboardShortcuts_NoWarning`) and the `ValueEditorTest` fixture
(`EnterValueEditMode_SpeaksLabelAndStartValue`,
`Navigate_ChangesValueAndSpeaksIt`, `Navigate_ClampsAtMinAndMax`,
`Confirm_CommitsAndExitsEditMode`, `Back_CancelsEditAndRestoresValue`,
`Boolean_StepsOnOff`, `Percent_FormatsAsPercent`). The Quit-is-last-of-12 test
is unchanged in substance: `press(open); for (i<11) navigate(1.0); press(confirm);`
still expects `"quit"` (`accessmenucontroller_test.cpp:203–224`, comment at
`:213`). Treat any change to `buildMenuTree()` as an API change, exactly as
before.

## Invariant D — action ids are a stable public contract

Unchanged. The complete id set is still the `actionId` column of the three
menu tables above, plus the reserved `"back"`. A mismatch still produces
`qWarning() << "Unknown access menu action:"` (`mixxxmainwindow.cpp:1688`)
and silence for the user. Ids still deliberately overlap `WMainMenuBar` signal
names where applicable — keep them aligned.

## Invariant E — the speak callback must stay injected

Unchanged. `MixxxMainWindow` still passes a lambda calling
`Library::announceText()` after a null check (`mixxxmainwindow.cpp:468–471`),
still routes through the ungated `slotQuickPickerItemHighlighted()` (Spec 06),
and is still subject to everything in Spec 05 (silent before a sound device
opens, discarded by barge-in — though see Spec 05/07's note on this class's
own double-speak fix — and recorded in `--tts-log`, now with real
audibility outcomes rather than a flat transcript, per Spec 05 Invariant 6).
Do not replace the injected callback with a direct `Library*` dependency; the
590-line test suite depends on the spy.

## Invariant F — the always-on keyboard chords are a second, parallel control surface (new)

Stated as its own invariant because it's new architecture, not a variant of
an existing one. The six chords documented above:

1. Are read from `res/keyboard/*.kbd.cfg` under the `[AccessMenu]` section,
   but **not** via the normal kbd.cfg → `ControlObject` binding mechanism —
   `MixxxMainWindow` reads the raw config values itself
   (`mixxxmainwindow.cpp:505–515`) and builds `QAction`s with
   `Qt::ApplicationShortcut` context.
2. Therefore do **not** appear as `ControlObject`s at all, and are
   consequently **not** covered by `a11ycontrols_test.cpp` (which checks CO
   existence) — their only current guard is
   `src/test/keyboardbindings_test.cpp`'s locale-parity checks (chord text
   identical across all 12 locales) and the generic collision detector (no
   two bound chords in one file share a key sequence, modulo the allowlisted
   pre-existing upstream collisions). Neither test proves the `QAction`
   wiring in `mixxxmainwindow.cpp` is intact — only that the config file's
   text is consistent.
3. Deliberately survive "Enable Keyboard Shortcuts" being turned off — this
   is the entire reason they exist as application-wide Qt shortcuts instead
   of ordinary kbd.cfg bindings, and the `toggleKeyboardShortcuts` warning
   (above) exists specifically to protect the escape hatch these chords
   provide.
4. Must be kept in sync with the hardware CO path's semantics. A rebase that
   changes `slotNavigate()`'s sign convention, for example, must update both
   the DDJ-400 mapping's `engine.setValue("[AccessMenu]", "navigate", delta)`
   call **and** the two keyboard lambdas at `mixxxmainwindow.cpp:520–527`
   (`slotNavigate(-1.0)` for Up, `slotNavigate(1.0)` for Down) — there is no
   single source of truth for "which direction is which" across the two entry
   paths.

## Cross-reference: the DDJ-400 mapping side

Owned by the mapping, not this class. Unchanged from the previous
verification except line numbers:

| Gesture | Script function | Writes |
|---|---|---|
| Browse rotate | `browseRotate` `:227–252` | `[AccessMenu],navigate` when open, else `[Library],MoveVertical` |
| Browse press, short (< 400 ms) | `browsePress` `:254–282` | `[AccessMenu],activate` when open, else `[Library],MoveFocusForward` |
| Browse press, hold (≥ 400 ms) | same, timer callback `:274–281` | `[AccessMenu],open` |
| Browse + shift | `browseShiftPress` `:284–292` | `[AccessMenu],back` when open, else `[Library],MoveFocusBackward` |
| LOAD deck 1 / 2 | `loadDeck1`/`loadDeck2` `:294–312` | `[AccessMenu],confirm` when open, else `LoadSelectedTrack` |

`browseHoldThreshold = 0.4` seconds (`Pioneer-DDJ-400-script.js:215`) —
unchanged. The 7-bit two's-complement browse-encoder decode (`:227–245`,
issue #47) and its defensive ±1 clamp are unchanged.

Note: the DDJ-400 mapping gained substantial **unrelated** new material in
this window — Shift+pad bindings for keylock/pitch/reset-key on the hotcue
pad-shift layer (issue #50) and the pad-mode "(not yet supported)" narration
tweak (Spec 06) — none of which touches the `[AccessMenu]` section
(`:211–312`) or this class. That's Spec 01/06/08 territory; noted here only
so a reader diffing the whole script file isn't surprised by unrelated churn
around the section this spec documents.

## Rebase checklist for this spec

1. All 7 `[AccessMenu]` controls present; the six input controls still
   `ButtonMode::Trigger`; `navigate` still a `ControlEncoder`.
   `a11ycontrols_test.cpp:536–561` now checks this in CI.
2. `[AccessMenu],active` still written by `openMenu()`/`closeMenu()` — the DDJ
   script's only way to know the menu is open.
3. `if (!m_open) return;` guard present in all five input slots, reachable
   from **both** the CO path and the six always-on keyboard `QAction`s.
4. Root menu still 12 items in the documented order, four of them (Recording,
   Broadcasting, Speech on/off, Keyboard shortcuts) still carrying a state
   `ValueItem` and Fullscreen still reading `m_fullscreenState`; Preferences
   still 13; Values still 5. Run `accessmenucontroller_test.cpp` and **read
   the failures**, don't just renumber the ticks.
5. `values[1..4].label` assignments still match the `kValueControls` order.
6. Every `switchToPageByTitle()` string still matches a real `DlgPreferences`
   page title after the rebase — including the three deliberate mismatches.
7. `m_stack` still holds pointers into an immutable `m_root`.
8. 30 s timeout restarted by every input, including inside value-edit mode
   and regardless of which entry path triggered it.
9. Value-edit cancel still restores `m_editStartValue`.
10. The six application-wide `QAction`s (`mixxxmainwindow.cpp:505–539`) still
    exist, still read their defaults from `[AccessMenu]` config keys with the
    documented chords, still use `Qt::ApplicationShortcut` context, and still
    keep working when "Enable Keyboard Shortcuts" is off. This has **no**
    ControlObject-existence guard test — verify manually or extend
    `keyboardbindings_test.cpp`.
11. The `toggleKeyboardShortcuts` pre-toggle warning
    (`accessmenucontroller.cpp:474–481`) still fires before the toggle takes
    effect, from every entry path.
12. Fix, or at minimum do not further entrench, the misfiled
    `sort_column_next`/`sort_column_prev`/`sort_order`/`show_column_menu`
    lines under `[AccessMenu]` in `res/keyboard/*.kbd.cfg` — they belong under
    `[Library]` and are currently dead (see the discovered-bug callout above).
13. Add `[Recording],status` and (conditionally on `__BROADCAST__`)
    `[Shoutcast],enabled` to `a11ycontrols_test.cpp`'s guarded-key table, and
    fix its stale "nothing reads `[Shoutcast],enabled`" comment (Invariant B).
14. Consider (still new work, not preservation — carried over from the
    previous verification, still not done) adding a spoken fallback on the
    unknown-action path at `mixxxmainwindow.cpp:1688` and on the null-manager
    early returns for Recording/Broadcasting. A `qWarning()` is invisible to
    the only user this feature exists for.

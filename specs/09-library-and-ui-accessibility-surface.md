# Spec 09 — Library and UI accessibility surface

**Status:** Draft for review
**Branch:** `spec-controller`, based on `accessibility-improvements-2026-06-25` @ `2390edf423`
**Owner:** accessibility fork
**Related:** Spec 04 (E2E QAccessible testing), Spec 06 (speech engine),
Spec 07 (announcement layer), Spec 08 (input layer)

## Goal

Document how the library and the Qt UI expose themselves — to the fork's
**self-voicing announcement layer** on one side, and to **OS screen readers**
(VoiceOver / NVDA / JAWS / Orca) on the other. These are two different
surfaces with two very different levels of investment, and the gap between them
is the fork's central architectural decision.

This is written as a contract the upstream-2.6 rebase must preserve.

---

## 1. The headline fact

```
$ git diff --stat 0e0589c751..HEAD -- res/skins/
$                                          # (no output)
```

**Zero lines of skin have been changed by this fork.**

```
$ git diff 0e0589c751..HEAD -- src/ | grep setFocusPolicy
$                                          # (no output)
```

**The fork has changed no widget's focus policy either.**

`WWidget`, the base class every skin widget derives from, sets
`Qt::ClickFocus` (`src/widget/wwidget.cpp:27`) — focusable by mouse, **never by
Tab**. Fifteen concrete widget classes then downgrade to `Qt::NoFocus`
outright: `wpushbutton`, `wknob`, `wknobcomposed`, `wslidercomposed`,
`whotcuebutton`, `weffectpushbutton`, `weffectchainpresetbutton`, `woverview`,
`wwaveformviewer`, `wvumeterbase`, `wvumeterlegacy`, `wstatuslight`,
`wstarrating` and others. All of that is upstream; the fork inherited it and
left it alone.

Combined with zero `setAccessibleName()` calls anywhere in the skin path, the
consequence is precise and total:

> **Every deck, mixer, EQ, effects and waveform widget in every shipped skin is
> unreachable by keyboard and anonymous to a screen reader. The skin is a
> screen-reader dead zone.**

This is **by design**. §3 explains the strategy and its trade-offs.

---

## 2. Library signals and control objects (the self-voicing surface)

### 2.1 Fork-added `Library` methods — the announcement entry points

`src/library/library.h`. Three public methods:

| Method | Line | Implementation | Purpose |
|---|---|---|---|
| `announceQuickPickerItem(const QString& text, int row = -1, int siblingCount = 0)` | `:116` | `library.cpp:481-483` | Speak a transient picker item with position ("3 of 12") |
| `announceText(const QString& text)` | `:120` | `library.cpp:485-487` | Speak a one-off event with no natural position |
| `announceSearchResultCount(int count)` | `:125` | `library.cpp:489-491` | Report how many tracks a search matched |

`announceText()` has **no signal of its own**:

```cpp
void Library::announceText(const QString& text) {
    emit quickPickerItemHighlighted(text, -1, 0);
}
```

It reuses the picker signal with a sentinel `row = -1`, which
`AnnouncementManager` reads as "no position information, just say the text".
That is worth knowing before anyone "tidies up" the signal list.

### 2.2 Fork-added `Library` signals

`src/library/library.h:169-188`. Five signals:

| Signal | Line | Emitted for |
|---|---|---|
| `sidebarItemActivated(QString title, int row, int siblingCount, int childCount, bool expanded)` | `:172-177` | Sidebar navigation; `childCount`/`expanded` describe containers; `row = -1` when position is unavailable |
| `playlistTracksEdited(QString name, int added, int removed)` | `:180` | Per-signal deltas, for spoken confirmation |
| `crateTracksEdited(QString name, int added, int removed)` | `:181` | As above |
| `quickPickerItemHighlighted(QString text, int row, int siblingCount)` | `:185` | Transient picker moved to a new item. **Always spoken** — the picker was invoked on purpose, so hearing it is not optional |
| `searchResultCountChanged(int count)` | `:188` | Folded into the spoken search announcement |

### 2.3 How the announcement layer consumes them — Invariant L1

`AnnouncementManager` connects to these with **Qt pointer-to-member-function
(PMF) syntax** (`src/util/announcementmanager.cpp:406-430`):

| Line | Signal |
|---|---|
| `:406` | `&Library::trackSelected` *(upstream)* |
| `:410` | `&Library::sidebarItemActivated` |
| `:414` | `&Library::playlistTracksEdited` |
| `:418` | `&Library::crateTracksEdited` |
| `:422` | `&Library::quickPickerItemHighlighted` |
| `:426` | `&Library::search` *(upstream)* |
| `:430` | `&Library::searchResultCountChanged` |

**Invariant L1 — the PMF connects are a compile-time contract, and that is a
feature.** Renaming or re-signaturing any of the five fork-added signals
**breaks the build**. This is the *safe* failure mode. It is the only part of
the announcement wiring that a rebase cannot silently break, and it is why the
`Library` signal path — rather than a string-keyed control — is the right
carrier for these events.

Contrast with the two unsafe modes documented below (L2, L4).

### 2.4 Emit sites — Invariant L2

The three `announce*` methods have **68 call sites** outside `src/test/`,
spread across eleven files:

| File | Sites |
|---|---|
| `src/library/trackset/baseplaylistfeature.cpp` | 16 |
| `src/mixxxmainwindow.cpp` | 14 |
| `src/library/trackset/crate/cratefeaturehelper.cpp` | 9 |
| `src/widget/wtracktableview.cpp` | 9 |
| `src/library/trackset/crate/cratefeature.cpp` | 7 |
| `src/library/library.cpp` | 5 (2 self-calls + 3 declarations) |
| `src/library/library.h` | 3 (declarations) |
| `src/mixxxmainwindow.h` | 2 |
| `src/coreservices.cpp` | 1 |
| `src/library/librarycontrol.cpp` | 1 |
| `src/errordialoghandler.h` | 1 |

**Invariant L2 — dropped emit sites fail silently.** An `announceText()` call
lost in a merge conflict compiles, links, passes every test that does not
specifically assert on that string, and produces **silence** at runtime. For a
sighted developer this is invisible; for the user it is the feature simply not
existing. Sixty-eight scattered call sites is a large silent-failure surface,
and the rebase must diff them explicitly rather than trusting the build.

Recommended rebase check:

```
git diff 0e0589c751..HEAD -- src/ | grep -c '^+.*announce\(Text\|QuickPickerItem\|SearchResultCount\)('
```

Run before and after; the counts must match.

### 2.5 Fork-added control objects in `LibraryControl`

`src/library/librarycontrol.cpp`. Exactly **10** fork-added COs:

| Control | Line | Handler | Behaviour |
|---|---|---|---|
| `[Library],AddToCrate` | `:329` | `slotAddToCrate` (`:767-776`) | Opens the spoken crate picker for the **selected** library track |
| `[Library],AddToPlaylist` | `:340` | `slotAddToPlaylist` (`:778-787`) | Opens the spoken playlist picker for the **selected** track |
| `[Channel1..4],quick_add_to_playlist` | loop at `:359-366` | `deckQuickAdd(deck, true)` (`:789-808`) | Same picker, for the track **loaded in that deck** |
| `[Channel1..4],quick_add_to_crate` | loop at `:359-366` | `deckQuickAdd(deck, false)` | As above |

The per-deck controls are `ControlPushButton` with
`ButtonMode::Trigger`, created in a loop **hardcoded to decks 1–4**
(`for (int deck = 1; deck <= 4; ++deck)`), matching the keyboard layouts
(Spec 08 §2.2 category C, which only binds decks 1 and 2).

All four creation blocks are guarded by `#ifdef MIXXX_USE_QML` /
`if (!CmdlineArgs::Instance().isQml())` — under the QML skin the controls are
still **created** but **not connected**, so they exist and do nothing.

`deckQuickAdd` speaks its own failure:

```cpp
if (!pTrack || !pTrack->getId().isValid()) {
    m_pLibrary->announceText(tr("Deck %1, no track loaded").arg(deck));
    return;
}
```

But it returns **silently** if there is no visible track table view
(`:791-796`) — the picker is implemented on `WTrackTableView`
(`quickAddTracksToPlaylist` / `quickAddTracksToCrate`), so it needs one to
exist. That is an unspoken failure path worth closing.

### 2.6 What is NOT fork-added — sort and focus controls

**Correction to earlier internal notes.** These controls are **upstream**, byte
-identical to `0e0589c751`:

`[Library],sort_column`, `sort_order`, `sort_column_toggle`,
`sort_focused_column`, `focused_widget`, `refocus_prev_widget`.

What the fork added is the **announcement** of sort state, in
`src/util/announcementmanager.cpp`:

| Element | Line |
|---|---|
| `[Library],sort_column` `ControlProxy` | `:443` |
| `[Library],sort_order` `ControlProxy` | `:451` |
| `[Library],focused_widget` `ControlProxy` | `:471` |
| Handler reading both proxies | `:1851-1860` |
| `AnnouncementManager::sortColumnName(TrackModel::SortColumnId)` | `:1870` |

This distinction matters for the rebase: conflicts in the *control* definitions
are upstream's to resolve; conflicts in the *proxies and handler* are the
fork's.

### 2.7 The `FocusWidget` router

`FocusWidget` is declared in `src/library/library_decl.h:11-22` and is
**100% upstream** (empty diff against the fork point):

```
None, Searchbar, Sidebar, TracksTable, ContextMenu, Dialog,
SearchRelatedMenu, Unknown, Count
```

`LibraryControl::getFocusedWidget()` (`librarycontrol.cpp:1032-1087`) classifies
the current focus by inspecting `QApplication::focusWindow()->type()` first
(`Qt::Popup` → context menu, `Qt::Dialog` → dialog) and only then the focused
widget.

**It is not a general state machine.** `slotMoveVertical`
(`librarycontrol.cpp:846-902`) is the **only** slot that actually branches on
`FocusWidget`:

| `m_focusedWidget` | `MoveVertical` behaviour |
|---|---|
| `Sidebar` | `slotSelectSidebarItem(i)` |
| `TracksTable` | Falls through to `Up`/`Down` key events (wrapping is handled by `WLibraryTableView::moveCursor()`) |
| `Dialog` | Maps up/down to `Tab`/`Backtab` |
| `ContextMenu`, `SearchRelatedMenu` | Sends `Up`/`Down` to `focusWindow()` — **not** `focusWidget()`, because a freshly-opened `QMenu` has no focus widget yet |
| `Searchbar` | Falls through to plain key events |
| `None`, `Unknown`, default | **Recovers** by calling `setLibraryFocus(FocusWidget::TracksTable)` and returning |

`slotMoveHorizontal` (`:934-938`) and `slotMoveFocus` (`:952-963`) have **no
`FocusWidget` switch at all** — they synthesise `Left`/`Right` and
`Tab`/`Backtab` unconditionally via `emitKeyEvent()`.

That last row of the table is load-bearing: from an unknown or unfocused state,
one browse-knob turn **re-anchors focus to the track table** rather than doing
nothing. For a blind user, "the knob puts me somewhere known" is far better
than "the knob is inert". A rebase that turns that `default:` into a no-op
removes the only recovery path.

`emitKeyEvent()` (`:995-1030`) is the shared delivery mechanism and carries the
focus-loss guard documented as Invariant C4 in Spec 08 §3.6.

---

## 3. Screen-reader surface (the deliberately small one)

### 3.1 `setAccessibleName()` — 6 call sites in the entire codebase

| File:line | Name set | Scope |
|---|---|---|
| `src/widget/wtracktableview.cpp:66` | `tr("Track list")` | Static |
| `src/widget/wsearchlineedit.cpp:106` | `tr("Search library")` | Static (the widget) |
| `src/widget/wsearchlineedit.cpp:107` | `tr("Search library")` | Static (its inner `QLineEdit`) |
| `src/widget/wlibrarysidebar.cpp:21` | `tr("Library sidebar")` | Static |
| `src/preferences/dialog/dlgprefsounditem.cpp:33` | `names.device` | **Runtime**, per sound item |
| `src/preferences/dialog/dlgprefsounditem.cpp:34` | `names.channel` | **Runtime**, per sound item |

Four in `src/widget/`, two in preferences. That is the complete list.

The two runtime ones are generated by
`DlgPrefSoundItem::accessibleNamesFor()` (`dlgprefsounditem.cpp:54-60`):

```cpp
const QString typeString = AudioPath::getTrStringFromType(type, index);
const QString deviceKind = isInput ? tr("input") : tr("output");
return {tr("%1 %2 device").arg(typeString, deviceKind),
        tr("%1 %2 channel").arg(typeString, deviceKind)};
```

— e.g. "Master output device", "Headphones output channel". This is the only
place in the fork where accessible names are *composed* rather than literal,
and it is the pattern the parked skin work (§3.5) would generalise.

### 3.2 `Qt::AccessibleTextRole` — 2 production sites, none in the library

**Correction to earlier internal notes.** `src/library/basetracktablemodel.cpp`
has **zero** `Qt::AccessibleTextRole` handling on this branch.

Complete list of production sites:

| File:line | What |
|---|---|
| `src/controllers/controllermappingtablemodel.cpp:64` | `if ((role == Qt::DisplayRole \|\| role == Qt::AccessibleTextRole) && …)` — mapping table cells |
| `src/controllers/dlgprefcontrollers.cpp:258` | `setData(0, Qt::AccessibleTextRole, pController->getName())` — controller tree items |

(`dlgprefcontrollers.cpp:259` is `Qt::AccessibleDescriptionRole`, not
`AccessibleTextRole` — it sets the constant string `tr("Controller")`. It is the
only `AccessibleDescriptionRole` site in the codebase.)

`BaseTrackTableModel::data()` (`src/library/basetracktablemodel.cpp:402`) ends
with a **role allowlist** at `:463-472`:

```cpp
// Only retrieve a value for supported roles
if (role != Qt::DisplayRole &&
        role != Qt::EditRole &&
        role != Qt::CheckStateRole &&
        role != Qt::ToolTipRole &&
        role != kDataExportRole &&
        role != Qt::TextAlignmentRole &&
        role != Qt::DecorationRole) {
    return QVariant();
}
return roleValue(index, rawValue(index), role);
```

`Qt::AccessibleTextRole` is **not** in that list, so a screen reader querying a
track-table cell for accessible text receives an invalid `QVariant` today.

**Invariant L3 (pending, branch `wt-51-track-row-readout`, PR #72).**
That branch adds `role != Qt::AccessibleTextRole` to the allowlist and builds a
`BaseTrackTableModel::rowAccessibleText()` helper that composes a spoken row
summary, explicitly reusing `data(idx, Qt::AccessibleTextRole)` for the columns
whose *visual* state would otherwise be silent — star rating, colour swatch,
play count:

```cpp
static constexpr ColumnCache::Column kSpokenColumns[] = {
        ColumnCache::COLUMN_LIBRARYTABLE_RATING,
        ColumnCache::COLUMN_LIBRARYTABLE_COLOR,
        ColumnCache::COLUMN_LIBRARYTABLE_TIMESPLAYED,
};
```

Once merged, the invariant is: **`BaseTrackTableModel::data()` must keep
admitting `Qt::AccessibleTextRole` through the allowlist.** Dropping that one
line during a rebase makes `rowAccessibleText()` return empty strings for the
rating/colour/play-count columns and removes accessible cell text from every
screen reader — with **no compile error and no test failure** unless the row
readout tests are also carried across. Mark it as pending until PR #72 merges.

### 3.3 Label / buddy associations in `.ui` files

`<property name="buddy">` associations tie a `QLabel`'s text to the control it
describes, so a screen reader announces "Speech rate, slider, 3" instead of
"slider, 3". They also give the label's `&`-mnemonic a target.

| Metric | Value |
|---|---|
| Total buddy properties in `src/**/*.ui` | **97** |
| Files containing them | **14** |
| Fork-added | **29** |

All 14 files are in `src/preferences/dialog/` or `src/controllers/`:

| File | Count |
|---|---|
| `dlgprefaccessibilitydlg.ui` | 16 |
| `dlgprefdeckdlg.ui` | 16 |
| `dlgprefwaveformdlg.ui` | 15 |
| `dlgprefsounddlg.ui` | 15 |
| `dlgprefbroadcastdlg.ui` | 8 |
| `dlgprefrecorddlg.ui` | 7 |
| `dlgprefinterfacedlg.ui` | 6 |
| `dlgprefvinyldlg.ui` | 5 |
| `dlgprefautodjdlg.ui` | 3 |
| `dlgprefmixerdlg.ui` | 2 |
| `dlgprefreplaygaindlg.ui`, `dlgpreflibrarydlg.ui`, `dlgprefsounditem.ui`, `dlgprefcontrollerdlg.ui` | 1 each |

**Zero buddies exist outside preferences and controllers.** There are no `.ui`
files for the skin (it is XML parsed by `LegacySkinParser`), so there is nothing
to buddy there even in principle.

### 3.4 `tools/check_ui_buddies.py` — and why it does not protect this fork

A 52-line fork-added script. For every `QLabel` in a `.ui` file it collects all
`<widget name=…>` values and flags:

1. A label that is **its own buddy** (`buddy_name == label_name`).
2. A buddy pointing at a widget that **does not exist** in the file.

Both are the classic ways a buddy silently stops working after a widget rename
— the `.ui` file still parses, Qt Designer still opens it, and the screen
reader just stops announcing the label. Exactly the right thing to lint.

**But it is wired to `pre-commit` only** — `.pre-commit-config.yaml:192-198`:

```yaml
- id: check-ui-buddies
  name: check-ui-buddies
  description: "Detect invalid or self-referential QLabel buddies in Qt UI files"
  entry: python tools/check_ui_buddies.py
  language: python
  types: [text]
  files: ^.*\.ui$
```

In CI it runs only via `.github/workflows/pre-commit.yml`. This fork's CI is
`.gitea/workflows/` — `build-linux.yml`, `build-macos.yml`,
`build-windows.yml`, `build-windows-tts.yml` — and **none of the four invoke
pre-commit**.

**Invariant L4 — the buddy lint is not enforced by the CI that actually runs.**
A rebase (or a `git commit --no-verify`) that breaks a buddy will not be caught.
Either add a pre-commit step to the `.gitea` workflows, or run
`tools/check_ui_buddies.py` directly there. This is the same class of gap as the
missing keyboard-binding lint in Spec 08 §6, and the fix is the same shape.

### 3.5 The parked work: `handoff/01-skin-widget-labels-tranche2.md`

Tranche 1 (commit `8e19ed6baf`) is what §3.1 and §3.3 describe: names for the
preferences dialogs and the library — search field, sidebar tree, track table,
category tree, plus the buddy associations. Before it, *"the codebase previously
contained zero `setAccessibleName` calls."*

Tranche 2 is **parked, blocked on tester feedback**. Its scope:

| Item | Detail |
|---|---|
| Goal | Accessible names for skin-level custom widgets — deck play/cue/sync buttons, knobs, faders, spinboxes |
| Where the names would come from | `LegacySkinParser`, at the point each widget is constructed and bound to a `ConfigKey`: derive `[Channel1],play` → "Deck 1 play" via a mapping table for the common controls |
| Bonus | Skin XML `<Tooltip>` text → `setAccessibleDescription` |
| Why it is blocked | *"The JAWS user's next session should identify which skin controls they actually reach with Tab/JAWS navigation; label those first instead of all ~hundreds of skin widgets."* |
| The catch the handoff itself flags | *"many skin widgets are not in the tab order at all; adding focus policies is a separate decision — ask the user before changing tab navigation behavior"* |

That last line is the crux. Naming a `Qt::NoFocus` widget accomplishes very
little on its own: most screen readers will not stop on a control they cannot
focus. Tranche 2 is therefore **two** changes — names *and* focus policies —
and the second is a behavioural change for sighted users too (a skin where Tab
cycles through 200 knobs is a worse skin). That is why it is a maintainer
decision, not a task.

---

## 4. Why the skin is a dead zone: the architectural choice

### 4.1 The choice

The fork does **not** try to make Mixxx's skin traversable by a screen reader.
It makes Mixxx **self-voicing** instead:

```
┌────────────────────────────────────────────────────────────┐
│  What the fork built                                       │
│                                                            │
│   keyboard bindings ─┐                                     │
│   controller mappings├─> ControlObjects ─> AnnouncementMgr │
│   Library signals ───┘                          │          │
│                                                 v          │
│                                            TtsEngine       │
│                                          (AVSpeech / Qt)   │
│                                                            │
├────────────────────────────────────────────────────────────┤
│  What the fork did NOT build                               │
│                                                            │
│   OS screen reader ──X──> QAccessible ──X──> skin widgets  │
│                            (unnamed, Qt::NoFocus)          │
└────────────────────────────────────────────────────────────┘
```

The screen reader is used for the **chrome** — menus, dialogs, preferences,
the library table, the search box — which are stock Qt widgets that expose
themselves for free. The **performance surface** is driven entirely by keys and
hardware, and reports back by speaking.

### 4.2 Why it is defensible

| Argument | Detail |
|---|---|
| **A DJ's hands are on hardware, not on Tab** | The interaction model is a controller and a keyboard. Tabbing to a virtual EQ knob is not how anyone mixes, sighted or not. |
| **The skin is custom-painted** | `WKnob`, `WSliderComposed`, `WPushButton` etc. are not `QAbstractSlider`/`QAbstractButton` subclasses with usable default `QAccessibleInterface`s. Every one would need a hand-written accessible interface *and* value/state reporting, not just a name. |
| **Latency** | The announcement layer speaks the *semantic event* ("Deck 1, playing, 3 minutes 12 remaining"), debounced and duckable. A screen reader narrating a focus walk is slower and noisier for the same information. |
| **It works with a controller alone** | The DDJ-400 path (Spec 01) needs no screen reader running at all. Screen-reader traversal would not. |
| **Bounded and testable** | Announcements can be asserted on as strings (Spec 04's `--tts-log`). Accessible-tree traversal needs an OS-specific AX driver on three platforms. |
| **Cross-platform for free** | One `TtsEngine` abstraction covers macOS/Windows/Linux. Three screen readers with three different behaviours do not. |

### 4.3 What it costs

| Cost | Consequence |
|---|---|
| **A screen-reader user who has never read the docs finds nothing** | Tab does not move through the decks; VoiceOver's cursor finds an anonymous blob. The app looks broken, not self-voicing. |
| **No discoverability** | There is no way to explore the interface. You must already know that `Alt+5` is deck 1's BPM. The `[AccessMenu]` spoken menu (Spec 01) is the only browsable surface, and it is DDJ-400-driven. |
| **Every new feature needs an explicit announcement** | Nothing is accessible by default. Invariant L2's 68 silent-failure call sites are the direct consequence of this model. |
| **Low-vision users get nothing** | Self-voicing helps blind users. A partially sighted user wanting a screen magnifier's focus tracking, or a high-contrast focused-control outline, gets no benefit — there is no focus to track. (`handoff/04-minimal-skin-low-vision.md` is the parked response to this.) |
| **It diverges from upstream** | Upstream will not accept "we voice it ourselves" as accessibility. Anything the fork wants to land upstream will eventually need the `QAccessible` route too. |

**Invariant L5 — the strategy is self-voicing, and it is load-bearing.** No part
of the fork's accessibility depends on the OS accessibility tree reaching the
skin. Conversely, **nothing** in the skin can be assumed reachable. Any future
work that assumes "the screen reader will read it" is wrong for every widget in
`res/skins/`, and any rebase that appears to "restore" skin accessibility has
not — there was never any.

---

## 5. Preferences: `DlgPrefAccessibility`

### 5.1 Size

| File | Lines |
|---|---|
| `src/preferences/dialog/dlgprefaccessibility.cpp` | **691** |
| `src/preferences/dialog/dlgprefaccessibility.h` | 92 |
| `src/preferences/dialog/dlgprefaccessibilitydlg.ui` | **843** |
| `src/preferences/accessibilitysettings.h` | **249** |
| **Total** | **1875** |

`src/preferences/accessibilitysettings.h` holds **35**
`DEFINE_PREFERENCE_HELPERS` macro invocations — 35 distinct accessibility
preferences, each with generated getter/setter/default. It is the single
largest declarative surface the fork added, and it is the schema every
accessibility feature reads from.

The `.ui` also carries 16 buddy associations (§3.3) — the highest count of any
file in the repo, tied with `dlgprefdeckdlg.ui`. The page is, appropriately,
the most screen-reader-correct page in the application.

### 5.2 Test coverage: zero

```
$ grep -rn "DlgPrefAccessibility\|AccessibilitySettings" src/test/
$                                          # (no output)
```

**Nothing** in `src/test/` references either the dialog or its settings header.

For context, the fork's accessibility test suites that *do* exist:

`accessmenucontroller_test.cpp`, `announcementmanager_test.cpp`,
`cmdlineargs_test.cpp`, `enginebeatclick_test.cpp`, `engineearcon_test.cpp`,
`enginetts_test.cpp`, `errordialoghandler_test.cpp`,
`ttsengine_integration_test.cpp`.

So the **behaviour** is well covered and the **configuration of that behaviour**
is not covered at all.

**Invariant L6 — 1875 lines of preferences UI and 35 settings have no test.**
The specific risks a rebase runs:

| Risk | Why it is silent |
|---|---|
| A settings key is renamed on one side of the split (`accessibilitysettings.h` vs. the reader in `announcementmanager.cpp`) | `ConfigKey` lookups are **string-keyed**. A mismatch returns the default, not an error. The feature reverts to its default value with no warning. This is the *unsafe* counterpart to Invariant L1's compile-time PMF connects. |
| A widget is dropped from the 843-line `.ui` during conflict resolution | `dlgprefaccessibility.cpp` would fail to compile *if* it names the widget — but a widget referenced only through a buddy or a layout is lost silently |
| A buddy association breaks | Not caught by CI (Invariant L4) |
| A default flips | `wt-36-readouts-default` already changed mixer readouts to default-on; nothing asserts the shipped defaults |

**Recommended minimum coverage** (does not exist yet):

1. A round-trip test per setting: write via the setter, read via the getter,
   confirm the `ConfigKey` string is stable.
2. A defaults snapshot test — assert the shipped default of all 35 preferences,
   so a flip is a failing test rather than a surprise.
3. A construction smoke test for `DlgPrefAccessibility` (build it against a
   temp `UserSettings`, call `slotUpdate` / `slotApply` / `slotResetToDefaults`,
   assert no crash and no changed values on a no-op apply).

(3) is the highest value per line: it is the only thing that would catch a
`.ui`/`.cpp` divergence, and it exercises the whole page in one test.

---

## 6. Invariant summary (rebase checklist)

| ID | Invariant | Failure mode | Loud? |
|---|---|---|---|
| **L1** | The 5 fork `Library` signals are consumed by PMF connects at `announcementmanager.cpp:406-430` | Rename → **build error**. This is the safe one. | **Loud** |
| **L2** | All 68 `announce*()` emit sites across 11 files survive | Dropped emit compiles, links, passes tests, and produces silence | **Silent** |
| **L3** *(pending, PR #72 / `wt-51-track-row-readout`)* | `BaseTrackTableModel::data()` keeps `Qt::AccessibleTextRole` in its role allowlist (`basetracktablemodel.cpp:463-472`) | Accessible cell text vanishes; `rowAccessibleText()` silently returns empty for rating/colour/play-count | **Silent** |
| **L4** | 29 fork-added buddy associations stay valid | `check_ui_buddies.py` would catch it, but it is **pre-commit-only** and no `.gitea` workflow runs it | **Silent in CI** |
| **L5** | Self-voicing is the strategy; nothing in `res/skins/` is screen-reader reachable and nothing may assume it is | Future work built on a false assumption | n/a (design) |
| **L6** | 35 settings + 1875 lines of preferences UI have zero test coverage; `ConfigKey` lookups are string-keyed | Renamed key silently returns the default | **Silent** |
| **L7** | `slotMoveVertical`'s `default:` case recovers by calling `setLibraryFocus(FocusWidget::TracksTable)` (`librarycontrol.cpp:890-896`) | Browse knob becomes inert from an unknown focus state, with no way back | **Silent** |

Five of seven fail silently, and the two loud ones are loud only because they
happen to be compile-time. The whole of §3.4, §5.2 and Spec 08 §6 point at the
same structural gap: **this fork's accessibility work is protected by tests at
the behaviour layer and by essentially nothing at the wiring layer.**

---

## 7. Open questions for review

1. **Should tranche 2 proceed at all?** (§3.5) It is two decisions: accessible
   names for skin widgets, and focus policies to make them reachable. The second
   changes behaviour for sighted users. Recommend: gather the JAWS tester's list
   of controls they actually reach first, exactly as the handoff says, and treat
   focus policy as a separate, opt-in preference if it happens at all.
2. **Is `--controller-navigation-without-focus` (Spec 08 §3.6) the right place
   for a preference on the Accessibility page?** It is CLI-only today, and the
   page already has 35 settings and 16 buddies, so adding one is cheap.
3. **Minimum test coverage for `DlgPrefAccessibility`** (§5.2). Recommend the
   construction smoke test (3) first — one test, highest coverage per line.
4. **Wire the buddy lint into `.gitea/workflows/`** (§3.4). Small, and it closes
   a silent CI gap that already exists today, independent of the rebase.
5. **Should `announceText()` get its own signal** rather than reusing
   `quickPickerItemHighlighted(text, -1, 0)`? (§2.1) The sentinel works but is
   surprising, and a distinct signal would make the 68 call sites easier to
   audit by category.

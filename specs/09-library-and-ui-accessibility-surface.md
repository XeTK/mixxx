# Spec 09 — Library and UI accessibility surface

**Status:** Verified current state — refreshed against the merged codebase
**Branch:** `spec-controller`, verified against `accessibility-improvements-2026-06-25`
@ `1cb23d38e7` (the real branch tip — see the note in Spec 08 §0 about why this
differs from specs 05-07's hypothetical full-merge verification)
**Owner:** accessibility fork
**Related:** Spec 04 (E2E QAccessible testing), Spec 06 (speech engine),
Spec 07 (announcement layer / spoken menu controller), Spec 08 (input layer)

## Goal

Document how the library and the Qt UI expose themselves — to the fork's
**self-voicing announcement layer** on one side, and to **OS screen readers**
(VoiceOver / NVDA / JAWS / Orca) on the other. These are two different
surfaces with two very different levels of investment, and the gap between
them is the fork's central architectural decision.

This is written as a contract the upstream-2.6 rebase must preserve.

Several accessibility PRs that touch library/UI territory are **still open**
as of `1cb23d38e7` and are described below as pending, not shipped — see the
per-section notes. `tea pr list --repo xetk/mixxx --state all` is the source
of truth for merge state; grep the actual files before trusting any status
claim in this document that looks stale.

---

## 1. The headline fact

```
$ git diff --stat 0e0589c751..HEAD -- res/skins/
$                                          # (no output)
```

**Zero lines of skin have been changed by this fork.** Still true, reconfirmed
directly against the current tip.

`WWidget`, the base class every skin widget derives from, sets
`Qt::ClickFocus` (`src/widget/wwidget.cpp:27`) — focusable by mouse, **never
by Tab**. Fifteen concrete widget classes then downgrade to `Qt::NoFocus`
outright: `wpushbutton`, `wknob`, `wknobcomposed`, `wslidercomposed`,
`whotcuebutton`, `weffectpushbutton`, `weffectchainpresetbutton`, `woverview`,
`wwaveformviewer`, `wvumeterbase`, `wvumeterlegacy`, `wstatuslight`,
`wstarrating` and others. All of that is upstream; the fork inherited it and
left it alone.

**Correction to an earlier draft of this spec:** the fork's non-skin
`setFocusPolicy` additions number **3**, not 6 — `src/library/dlgtrackinfo.cpp:98`,
`src/library/dlgtrackinfomulti.cpp:269`, `src/widget/wcoverartlabel.cpp` (all
from issue #63, PR #88, merged — see §3.1). A previous internal note also
claimed three more sites in `src/library/autodj/dlgautodj.cpp`; that is
**wrong**. `git diff 0e0589c751..HEAD -- src/library/autodj/dlgautodj.cpp` is
**empty** — the fork has made zero changes to that file. Its three existing
`setFocusPolicy(Qt::ClickFocus)` calls are pre-existing upstream code,
unrelated to this fork, and the AutoDJ accessibility work referenced by that
earlier note (issue #61, PR #86) is **still open**, not merged — it is out of
this spec's scope until it lands.

Combined with the fork's very small `setAccessibleName()` surface (§3.1), the
practical consequence remains:

> **Every deck, mixer, EQ, effects and waveform widget in every shipped skin
> is unreachable by keyboard and anonymous to a screen reader. The skin is a
> screen-reader dead zone.**

This is **by design**. §4 explains the strategy and its trade-offs.

---

## 2. Library signals and control objects (the self-voicing surface)

### 2.1 Fork-added `Library` methods — the announcement entry points

`src/library/library.h`. Five public methods (two more than earlier drafts of
this spec counted — `announceMenuHover` and its static helper, added by
issue #60):

| Method | Line | Implementation | Purpose |
|---|---|---|---|
| `announceQuickPickerItem(const QString& text, int row = -1, int siblingCount = 0)` | `:118` | `library.cpp:484-486` | Speak a transient picker item with position ("3 of 12") |
| `announceText(const QString& text)` | `:122` | `library.cpp:488-490` | Speak a one-off event with no natural position |
| `announceSearchResultCount(int count)` | `:127` | `library.cpp:492-494` | Report how many tracks a search matched |
| `announceMenuHover(QMenu* pMenu)` | `:140` | `library.cpp:496-517` | Wire a `QMenu`'s `hovered` signal to speak the hovered item, deduplicated (§2.6) |
| `hoverAnnouncementTextForAction(const QAction* pAction)` (static) | `:150` | `library.cpp:518+` | Extract the spoken text for a menu action, preferring `data()` over `text()` |

`announceText()` has **no signal of its own**:

```cpp
void Library::announceText(const QString& text) {
    emit quickPickerItemHighlighted(text, -1, 0);
}
```

It reuses the picker signal with a sentinel `row = -1`, which
`AnnouncementManager` reads as "no position information, just say the text".
That is worth knowing before anyone "tidies up" the signal list.

`announceMenuHover()` is itself built on top of `announceQuickPickerItem()` —
it does not carry the hovered text on its own signal; it calls
`announceQuickPickerItem(name)` once it has resolved a non-empty name
(`library.cpp:509-516`). So the "self-voicing surface" ultimately narrows to
three real emission paths: the picker signal (used directly, and reused by
both `announceText()` and `announceMenuHover()`), `searchResultCountChanged`,
and the two upstream-adjacent signals in §2.2.

### 2.2 Fork-added `Library` signals

`src/library/library.h:202-218`. Five signals:

| Signal | Line | Emitted for |
|---|---|---|
| `sidebarItemActivated(const QString& title, int row = -1, int siblingCount = 0, int childCount = 0, bool expanded = false)` | `:202-206` | Sidebar navigation; `childCount`/`expanded` describe containers; `row = -1` when position is unavailable |
| `playlistTracksEdited(const QString& name, int added, int removed)` | `:209` | Per-signal deltas, for spoken confirmation |
| `crateTracksEdited(const QString& name, int added, int removed)` | `:210` | As above |
| `quickPickerItemHighlighted(const QString& text, int row, int siblingCount)` | `:215` | Transient picker moved to a new item. **Always spoken** — the picker was invoked on purpose, so hearing it is not optional |
| `searchResultCountChanged(int count)` | `:218` | Folded into the spoken search announcement |

### 2.3 How the announcement layer consumes them — Invariant L1

`AnnouncementManager::init()` connects to these with **Qt pointer-to-member-function
(PMF) syntax**, all inside one `if (pLibrary) { ... }` block
(`src/util/announcementmanager.cpp:405-438`):

| Line | Signal |
|---|---|
| `:406-409` | `&Library::trackSelected` *(upstream)* |
| `:410-413` | `&Library::trackRowSelected` *(upstream, carries the row-readout text from issue #51, §3.2)* |
| `:414-417` | `&Library::sidebarItemActivated` |
| `:418-421` | `&Library::playlistTracksEdited` |
| `:422-425` | `&Library::crateTracksEdited` |
| `:426-429` | `&Library::quickPickerItemHighlighted` |
| `:430-433` | `&Library::search` *(upstream)* |
| `:434-437` | `&Library::searchResultCountChanged` |

**Invariant L1 — the PMF connects are a compile-time contract, and that is a
feature.** Renaming or re-signaturing any of the five fork-added signals
**breaks the build**. This is the *safe* failure mode. It is the only part of
the announcement wiring that a rebase cannot silently break, and it is why the
`Library` signal path — rather than a string-keyed control — is the right
carrier for these events.

Contrast with the two unsafe modes documented below (L2, L4).

### 2.4 Emit sites — Invariant L2

**Correction to an earlier draft:** the three `announce*` methods (not
counting `announceMenuHover`, which is a distinct wiring mechanism, §2.6) have
**61 call sites** outside `src/test/`, across 10 files — not 68 across 11.
The earlier figure appears to have conflated `announceMenuHover`'s ~24
call sites (§2.6) with the `announce*` count; they are separate signal paths
and should not be added together.

| File | Sites |
|---|---|
| `src/library/trackset/baseplaylistfeature.cpp` | 16 |
| `src/mixxxmainwindow.cpp` | 12 |
| `src/widget/wtracktableview.cpp` | 10 |
| `src/library/trackset/crate/cratefeaturehelper.cpp` | 9 |
| `src/library/trackset/crate/cratefeature.cpp` | 7 |
| `src/library/library.cpp` | 3 (self-calls: `announceMenuHover` calling `announceQuickPickerItem`, etc.) |
| `src/widget/wtrackmenu.cpp` | 1 |
| `src/widget/trackconfirmdialogs.cpp` | 1 |
| `src/library/librarycontrol.cpp` | 1 |
| `src/coreservices.cpp` | 1 |

**Invariant L2 — dropped emit sites fail silently.** An `announceText()` call
lost in a merge conflict compiles, links, passes every test that does not
specifically assert on that string, and produces **silence** at runtime. For a
sighted developer this is invisible; for the user it is the feature simply not
existing. Sixty-odd scattered call sites is a large silent-failure surface,
and the rebase must diff them explicitly rather than trusting the build.

Recommended rebase check:

```
git diff 0e0589c751..HEAD -- src/ | grep -c '^+.*announce\(Text\|QuickPickerItem\|SearchResultCount\)('
```

Run before and after; the counts must match.

### 2.5 Fork-added control objects in `LibraryControl`

`src/library/librarycontrol.cpp`. Fork-added COs:

| Control | Line | Handler | Behaviour |
|---|---|---|---|
| `[Library],AddToCrate` | `:330` | `slotAddToCrate` (`:768-776`) | Opens the spoken crate picker for the **selected** library track |
| `[Library],AddToPlaylist` | `:341` | `slotAddToPlaylist` (`:779-787`) | Opens the spoken playlist picker for the **selected** track |
| `[Channel1..4],quick_add_to_playlist` | loop at `:360` | `deckQuickAdd(deck, true)` (`:790-806`) | Same picker, for the track **loaded in that deck** |
| `[Channel1..4],quick_add_to_crate` | loop at `:360` | `deckQuickAdd(deck, false)` | As above |

The per-deck controls are created in a loop **hardcoded to decks 1-4**
(`for (int deck = 1; deck <= 4; ++deck)`, `:360`), matching the keyboard
layouts (Spec 08 §2.2 Category C, which only binds decks 1 and 2).

All four creation blocks are guarded by `#ifdef MIXXX_USE_QML` /
`if (!CmdlineArgs::Instance().isQml())` (`:331-333`, `:342-344`, `:356-358`) —
under the QML skin the controls are still **created** but **not connected**,
so they exist and do nothing.

`deckQuickAdd` speaks its own failure:

```cpp
if (!pTrack || !pTrack->getId().isValid()) {
    m_pLibrary->announceText(tr("Deck %1, no track loaded").arg(deck));
    return;
}
```

But it returns **silently** if there is no visible track table view — the
picker is implemented on `WTrackTableView`
(`quickAddTracksToPlaylist` / `quickAddTracksToCrate`), so it needs one to
exist. That is an unspoken failure path worth closing.

### 2.6 The hover-narration mechanism (issue #60, PR #80, merged) — separate from §2.4

`Library::announceMenuHover(QMenu*)` connects `QMenu::hovered` with per-action
dedup and calls `hoverAnnouncementTextForAction()`, then
`announceQuickPickerItem()` if the resolved name is non-empty
(`library.cpp:496-517`). The static helper prefers `data()` over `text()`
because dynamically-named items store the raw name in `data()` while `text()`
may hold escaped `&&` mnemonics.

Call sites: `src/widget/wtrackmenu.cpp` — **20** (not the ~22 in earlier
notes), plus `src/library/trackset/crate/cratefeature.cpp` — 2 and
`playlistfeature.cpp` — 2. The non-`wtrackmenu.cpp` sites exist because
`QMenu::hovered` does not bubble from a submenu to its parent, so each
context-menu owner needs its own wiring.

### 2.7 What is NOT fork-added — sort and focus controls

These controls are **upstream**, byte-identical to `0e0589c751`, and are
genuinely present today (verified directly, not inferred):

`[Library],sort_column` (`ControlEncoder`, `librarycontrol.cpp:387`),
`sort_order` (`ControlPushButton`, Toggle, `:388`), `sort_column_toggle`
(`ControlEncoder`, `:390`), `sort_focused_column` (`ControlPushButton`,
`:392`), `focused_widget` (`:228`), `refocus_prev_widget` (`:255`).

**Correction to an earlier internal note:** `sort_column_next`,
`sort_column_prev`, and `show_column_menu` — controls an earlier draft of
this spec discussed as if they existed under `[Library]` in `.kbd.cfg` (and
worried about a merge artifact that misplaced them under `[AccessMenu]`) —
**do not exist anywhere in this codebase**, in any group. That entire
discussion was based on work that has not landed: issue #59 (PR #81, keyboard
bindings for library sort/column menu) is **still open**, and
`res/keyboard/en_US.kbd.cfg`'s `[Library]` section today contains exactly
three lines — `EditItem r`, `AddToCrate Alt+Shift+c`, `AddToPlaylist
Alt+Shift+p` — no sort-related binding of any kind. There is no live merge
hazard here to document because the feature it would apply to has not shipped.

What the fork *has* added, independent of any keyboard binding, is the
**announcement** of sort state, in `src/util/announcementmanager.cpp`:

| Element | Line |
|---|---|
| `[Library],sort_column` `ControlProxy` | `:440-450`ish (see `:440-490` block) |
| `[Library],sort_order` `ControlProxy` | same block |
| Handler | `slotAnnounceSort()` |
| `AnnouncementManager::sortColumnName(TrackModel::SortColumnId)` | switch table |

This is live today, even though there is currently no keyboard chord that
drives `sort_column_toggle`/`sort_focused_column` to exercise it — a sighted
mouse click on a column header, or a future keyboard binding once #81 lands,
would trigger it. This distinction matters for the rebase: conflicts in the
*control* definitions are upstream's to resolve; conflicts in the *proxies
and handler* are the fork's.

### 2.8 The `FocusWidget` router

`FocusWidget` is declared in `src/library/library_decl.h` and remains
100% upstream:

```
None, Searchbar, Sidebar, TracksTable, ContextMenu, Dialog,
SearchRelatedMenu, Unknown, Count
```

`LibraryControl::getFocusedWidget()` (`librarycontrol.cpp:1038-1101`)
classifies the current focus by inspecting `QApplication::focusWindow()`'s
type first (`Qt::Popup` → context menu, `Qt::Dialog` → dialog) and only then
the focused widget. It now null-checks `focusWindow` before dereferencing
(`:1040`; this is the issue #64 fix, Spec 08 §6) — a real latent crash that
existed only when controller-navigation-without-focus was allowed and no
window had focus.

**It is not a general state machine.** `slotMoveVertical`
(`librarycontrol.cpp:847-894`) is the **only** slot that actually branches on
`FocusWidget`:

| `m_focusedWidget` | `MoveVertical` behaviour |
|---|---|
| `Sidebar` (`:849-853`) | `slotSelectSidebarItem(i)` |
| `TracksTable` (`:854-858`) | Falls through to `Up`/`Down` key events (wrapping is handled by `WLibraryTableView::moveCursor()`) |
| `Dialog` (`:859-866`) | Maps up/down to `Tab`/`Backtab` |
| `ContextMenu`, `SearchRelatedMenu` (`:867-876`) | Sends `Up`/`Down` to `focusWindow()` — **not** `focusWidget()`, because a freshly-opened `QMenu` has no focus widget yet |
| `Searchbar` (`:877-880`) | Falls through to plain key events |
| `None`, `Unknown`, default (`:881-887`) | **Recovers** by calling `setLibraryFocus(FocusWidget::TracksTable)` and returning |

`slotMoveHorizontal` (`:935-938`) and `slotMoveFocus` (`:953-963`) have **no
`FocusWidget` switch at all** — they synthesise `Left`/`Right` and
`Tab`/`Backtab` unconditionally via `emitKeyEvent()`.

That last row of the table is load-bearing: from an unknown or unfocused
state, one browse-knob turn **re-anchors focus to the track table** rather
than doing nothing. For a blind user, "the knob puts me somewhere known" is
far better than "the knob is inert". A rebase that turns that `default:` case
into a no-op removes the only recovery path.

`emitKeyEvent()` (`:1001-1036`) is the shared delivery mechanism and carries
the focus-loss guard documented as Invariant C4 in Spec 08 §6.

---

## 3. Screen-reader surface (the deliberately small one)

### 3.1 `setAccessibleName()` — 9 call sites in the entire codebase

Up from 6 at the last verification point, all new sites from issue #63
(PR #88, merged):

| File:line | Name set | Scope |
|---|---|---|
| `src/widget/wtracktableview.cpp:67` | `tr("Track list")` | Static |
| `src/widget/wsearchlineedit.cpp:106` | `tr("Search library")` | Static (the widget) |
| `src/widget/wsearchlineedit.cpp:107` | `tr("Search library")` | Static (its inner `QLineEdit`) |
| `src/widget/wlibrarysidebar.cpp:21` | `tr("Library sidebar")` | Static |
| `src/widget/wcoverartlabel.cpp:57` | `tr("Cover art")` | Static (class-level — shared by both track-info dialogs and `DlgTagFetcher`) |
| `src/library/dlgtrackinfo.cpp:99` | `tr("Star rating")` | Static, this dialog's `WStarRating` instance |
| `src/library/dlgtrackinfomulti.cpp:270` | `tr("Star rating")` | Same, multi-track variant |
| `src/preferences/dialog/dlgprefsounditem.cpp:33` | `names.device` | **Runtime**, per sound item |
| `src/preferences/dialog/dlgprefsounditem.cpp:34` | `names.channel` | **Runtime**, per sound item |

**Correction to an earlier draft:** `src/library/dlgtrackinfo.cpp` and
`dlgtrackinfomulti.cpp` are the real paths — not `src/widget/`. Both files
were moved/live in `src/library/` on this branch.

The two `DlgPrefSoundItem` names are generated by
`DlgPrefSoundItem::accessibleNamesFor()`:

```cpp
const QString typeString = AudioPath::getTrStringFromType(type, index);
const QString deviceKind = isInput ? tr("input") : tr("output");
return {tr("%1 %2 device").arg(typeString, deviceKind),
        tr("%1 %2 channel").arg(typeString, deviceKind)};
```

— e.g. "Master output device", "Headphones output channel". This remains the
only place in the fork where accessible names are *composed* rather than
literal.

### 3.2 `Qt::AccessibleTextRole` — now shipped in the library, not pending

**This is the most significant status change since the last verification of
this spec.** Issue #51 (PR #72, "speak full track-row state on selection")
has **merged**. `BaseTrackTableModel::data()`
(`src/library/basetracktablemodel.cpp`) now admits `Qt::AccessibleTextRole`
through its role allowlist:

```cpp
// Only retrieve a value for supported roles
if (role != Qt::DisplayRole &&
        role != Qt::EditRole &&
        role != Qt::CheckStateRole &&
        role != Qt::ToolTipRole &&
        role != kDataExportRole &&
        role != Qt::TextAlignmentRole &&
        role != Qt::DecorationRole &&
        role != Qt::AccessibleTextRole) {
    return QVariant();
}
```

(`basetracktablemodel.cpp:464-473`.) `rowAccessibleText()`
(`basetracktablemodel.cpp:498-548`, declared `basetracktablemodel.h:116`)
composes "Artist, Title" plus "BPM locked" (if set), plus each of a
`kSpokenColumns` set's own `Qt::AccessibleTextRole` text so the two stay in
sync:

```cpp
static constexpr ColumnCache::Column kSpokenColumns[] = {
        ColumnCache::COLUMN_LIBRARYTABLE_RATING,
        ColumnCache::COLUMN_LIBRARYTABLE_COLOR,
        ColumnCache::COLUMN_LIBRARYTABLE_TIMESPLAYED,
};
```

Consumed via `WTrackTableView` → `Library::trackRowSelected` (upstream
signal) → `AnnouncementManager`'s connection at
`announcementmanager.cpp:410-413` (§2.3).

**Invariant L3 (now live, not pending).** `BaseTrackTableModel::data()` must
keep admitting `Qt::AccessibleTextRole` through the allowlist. Dropping that
one line during a rebase makes `rowAccessibleText()` return empty strings for
the rating/colour/play-count columns and removes accessible cell text from
every screen reader — with **no compile error and no test failure** unless
the row readout tests are also carried across.

### 3.3 Other `Qt::AccessibleTextRole` sites — unchanged

Two more production sites outside the library, both re-confirmed at the same
lines as before:

| File:line | What |
|---|---|
| `src/controllers/controllermappingtablemodel.cpp:64` | `if ((role == Qt::DisplayRole \|\| role == Qt::AccessibleTextRole) && …)` — mapping table cells |
| `src/controllers/dlgprefcontrollers.cpp:258` | `setData(0, Qt::AccessibleTextRole, pController->getName())` — controller tree items |

(`dlgprefcontrollers.cpp:259` is `Qt::AccessibleDescriptionRole`, setting the
constant string `tr("Controller")` — the only `AccessibleDescriptionRole` site
in the codebase.)

### 3.4 Label / buddy associations in `.ui` files

`<property name="buddy">` associations tie a `QLabel`'s text to the control it
describes, so a screen reader announces "Speech rate, slider, 3" instead of
"slider, 3". They also give the label's `&`-mnemonic a target.

**These counts have grown substantially since issue #62 (PR #84, merged, "10
more prefs pages get buddies") landed on top of issue #63's work:**

| Metric | Value |
|---|---|
| Total buddy properties in `src/**/*.ui` | **205** |
| Files containing them | **19** |
| Declarative `<property name="accessibleName">` (not buddies) | **58**, across **15** files |

Full per-file buddy breakdown:

| File | Count |
|---|---|
| `dlgprefkeydlg.ui` | 25 |
| `dlgprefbroadcastdlg.ui` | 23 |
| `dlgprefdeckdlg.ui` | 20 |
| `dlgtrackinfo.ui` | 21 |
| `dlgtrackinfomulti.ui` | 18 |
| `dlgprefwaveformdlg.ui` | 19 |
| `dlgprefaccessibilitydlg.ui` | 16 |
| `dlgprefsounddlg.ui` | 15 |
| `dlgpreflibrarydlg.ui` | 8 |
| `dlgprefinterfacedlg.ui` | 8 |
| `dlgprefmodplugdlg.ui` | 7 |
| `dlgprefrecorddlg.ui` | 7 |
| `dlgprefcolorsdlg.ui` | 5 |
| `dlgprefvinyldlg.ui` | 5 |
| `dlgprefautodjdlg.ui` | 3 |
| `dlgprefmixerdlg.ui` | 2 |
| `dlgprefreplaygaindlg.ui`, `dlgprefsounditem.ui`, `dlgprefcontrollerdlg.ui` | 1 each |

(19 files, 205 buddies total, confirmed by direct recount.)

The two big newcomers relative to earlier drafts of this spec are
`dlgtrackinfo.ui` (21, issue #63) and `dlgtrackinfomulti.ui` (18, issue #63) —
neither dialog had any buddy coverage before. **Zero buddies exist outside
preferences, controllers, and the two track-info dialogs.** There are no `.ui`
files for the skin (it is XML parsed by `LegacySkinParser`), so there is
nothing to buddy there even in principle.

### 3.5 `tools/check_ui_buddies.py` — and why it still does not protect this fork

Unchanged in substance from the last verification: a fork-added script that
flags a label that is its own buddy, or a buddy pointing at a nonexistent
widget. Both are the classic ways a buddy silently stops working after a
widget rename.

**But it is wired to `pre-commit` only** — `.pre-commit-config.yaml:195-198`
(`entry: python tools/check_ui_buddies.py`, `files: ^.*\.ui$`).

This fork's CI is `.gitea/workflows/` — confirmed today to contain exactly
four files: `build-linux.yml`, `build-macos.yml`, `build-windows.yml`,
`build-windows-tts.yml` — and **none of the four invoke `check_ui_buddies.py`**
or pre-commit generally (`grep -rl check_ui_buddies .gitea/workflows/` →
no matches).

**Invariant L4 — the buddy lint is not enforced by the CI that actually
runs.** A rebase (or a `git commit --no-verify`) that breaks a buddy will not
be caught. This has gotten more consequential since the last verification,
not less: 205 buddies across 19 files is a much larger unguarded surface than
the 97/14 it was when this gap was first flagged. Either add a pre-commit step
to the `.gitea` workflows, or run `tools/check_ui_buddies.py` directly there.
This is the same class of gap as the (now-partially-closed, see Spec 08 §6)
missing keyboard-binding lint.

### 3.6 The parked work: skin widget labels

The scope described in earlier drafts of this spec (accessible names for
skin-level custom widgets — deck play/cue/sync buttons, knobs, faders — via a
`ConfigKey`-to-name mapping table inside `LegacySkinParser`, plus skin
`<Tooltip>` → `setAccessibleDescription`) remains **parked**, and remains
blocked on the same open question: naming a `Qt::NoFocus` widget accomplishes
little on its own, since most screen readers will not stop on a control they
cannot focus, and adding focus policies to ~hundreds of skin widgets is a
behavioural change for sighted users too (a skin where Tab cycles through 200
knobs is a worse skin for everyone). That remains a maintainer decision, not
a task, and nothing in the merged tree has moved on it.

---

## 4. Why the skin is a dead zone: the architectural choice

### 4.1 The choice

The fork does **not** try to make Mixxx's skin traversable by a screen
reader. It makes Mixxx **self-voicing** instead:

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
themselves for free. The **performance surface** is driven entirely by keys
and hardware, and reports back by speaking.

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
| **No discoverability** | There is no way to explore the interface. You must already know that `Alt+5` is deck 1's BPM. The `[AccessMenu]` spoken menu (Spec 01, Spec 07) is the only browsable surface, and it is DDJ-400/keyboard-driven, not skin-driven. |
| **Every new feature needs an explicit announcement** | Nothing is accessible by default. Invariant L2's ~61 silent-failure call sites (plus the ~24 hover sites in §2.6) are the direct consequence of this model. |
| **Low-vision users get nothing** | Self-voicing helps blind users. A partially sighted user wanting a screen magnifier's focus tracking, or a high-contrast focused-control outline, gets no benefit — there is no focus to track. |
| **It diverges from upstream** | Upstream will not accept "we voice it ourselves" as accessibility. Anything the fork wants to land upstream will eventually need the `QAccessible` route too. |

**Invariant L5 — the strategy is self-voicing, and it is load-bearing.** No
part of the fork's accessibility depends on the OS accessibility tree
reaching the skin. Conversely, **nothing** in the skin can be assumed
reachable. Any future work that assumes "the screen reader will read it" is
wrong for every widget in `res/skins/`, and any rebase that appears to
"restore" skin accessibility has not — there was never any.

---

## 5. Preferences: `DlgPrefAccessibility`

### 5.1 Size

| File | Lines |
|---|---|
| `src/preferences/dialog/dlgprefaccessibility.cpp` | **703** |
| `src/preferences/dialog/dlgprefaccessibility.h` | 93 |
| `src/preferences/dialog/dlgprefaccessibilitydlg.ui` | **853** |
| `src/preferences/accessibilitysettings.h` | **264** |
| **Total** | **1913** |

`src/preferences/accessibilitysettings.h` holds **36**
`DEFINE_PREFERENCE_HELPERS` macro invocations — 36 distinct accessibility
preferences (up from 35 at the last verification point; the new one is
`ControllerNavigationWithoutFocus`, Spec 08 §6, `:231-236`). **Correction to
an earlier draft:** `OrientationPlayed` (the first-run TTS onboarding flag,
issue #105) does **not** exist yet — `grep -rn OrientationPlayed src/` returns
nothing. PR #122 (issue #105) is still open; when it lands, it will be a
37th preference and belongs to spec 06/07's speech-content territory even
though the flag itself lives in this file.

The `.ui` also carries 16 buddy associations (§3.4) — third-highest count of
any file in the repo behind `dlgprefkeydlg.ui` (25) and
`dlgprefbroadcastdlg.ui` (23).

### 5.2 Test coverage: still essentially zero

```
$ grep -rn "DlgPrefAccessibility\|AccessibilitySettings" src/test/
src/test/accessibilitysettings_test.cpp
```

Exactly one file, with exactly 3 `TEST_F` cases, all for
`ControllerNavigationWithoutFocus`: `_DefaultsOff`, `_ReadWrite`,
`_ResetToDefault`. The other 35 settings — including the freshly-added
`ControllerNavigationWithoutFocus` default itself only partially covered, and
whatever `OrientationPlayed` becomes — have zero direct persistence test
coverage, and `DlgPrefAccessibility` the dialog class (37 `.ui` controls at
last count, `slotUpdate`/`slotApply`/`slotResetToDefaults`) has zero test
coverage of any kind.

For context, the fork's accessibility test suites that *do* exist and are
well covered: `accessmenucontroller_test.cpp`, `announcementmanager_test.cpp`,
`cmdlineargs_test.cpp`, `enginebeatclick_test.cpp`, `engineearcon_test.cpp`,
`enginetts_test.cpp`, `errordialoghandler_test.cpp`,
`ttsengine_integration_test.cpp`, `wstarrating_test.cpp`,
`libraryscannerdlg_speech_test.cpp`, `dlgkeywheel_speech_test.cpp`,
`trackconfirmdialogs_test.cpp`, `keyboardbindings_test.cpp`.

**Invariant L6 — 1913 lines of preferences UI and 36 settings have
essentially no test.** The specific risks a rebase runs:

| Risk | Why it is silent |
|---|---|
| A settings key is renamed on one side of the split (`accessibilitysettings.h` vs. the reader in `announcementmanager.cpp`) | `ConfigKey` lookups are **string-keyed**. A mismatch returns the default, not an error. The feature reverts to its default value with no warning. This is the *unsafe* counterpart to Invariant L1's compile-time PMF connects. |
| A widget is dropped from the 853-line `.ui` during conflict resolution | `dlgprefaccessibility.cpp` would fail to compile *if* it names the widget — but a widget referenced only through a buddy or a layout is lost silently |
| A buddy association breaks | Not caught by CI (Invariant L4) |
| A default flips | Nothing asserts the shipped defaults for 35 of the 36 settings |

**Recommended minimum coverage** (does not exist yet):

1. A round-trip test per setting: write via the setter, read via the getter,
   confirm the `ConfigKey` string is stable.
2. A defaults snapshot test — assert the shipped default of all 36
   preferences, so a flip is a failing test rather than a surprise.
3. A construction smoke test for `DlgPrefAccessibility` (build it against a
   temp `UserSettings`, call `slotUpdate` / `slotApply` / `slotResetToDefaults`,
   assert no crash and no changed values on a no-op apply).

(3) is the highest value per line: it is the only thing that would catch a
`.ui`/`.cpp` divergence, and it exercises the whole page in one test.

---

## 6. Dialog-level accessibility fixes (issue #63, PR #88, merged)

Three fixes landed together in a wave since the last verification of this
spec, all in dialog/widget code (not skin code, hence no impact on §1's
skin-dead-zone claim):

**(a) `DlgTrackInfo`/`DlgTrackInfoMulti` keyboard access + buddies.** Real
paths: `src/library/dlgtrackinfo.cpp`/`.h` and
`src/library/dlgtrackinfomulti.cpp`/`.h` (not `src/widget/` — an earlier note
had the wrong directory). 21 and 18 buddy blocks respectively (§3.4).
`WStarRating`, in this dialog's instance, gets `Qt::StrongFocus` and
accessible name "Star rating" (`dlgtrackinfo.cpp:98-99`,
`dlgtrackinfomulti.cpp:269-270`); a new `keyPressEvent()` in
`src/widget/wstarrating.cpp:88-120` handles Left/Down (decrement), Right/Up
(increment), Home/End (jump to min/max), and digit keys 0-9 (jump directly).
`WCoverArtLabel` gets `Qt::StrongFocus` and accessible name "Cover art" at the
class level (`wcoverartlabel.cpp:57`, so it applies wherever the widget is
used, including `DlgTagFetcher`), with a new `keyPressEvent()`
(`:144-158`, Enter/Return/Space trigger `activate()`, factored out of the
existing `mousePressEvent`). Test: `src/test/wstarrating_test.cpp`.

**(b) `LibraryScannerDlg` announced on first show.** A callback chain:
`LibraryScannerDlg::setAnnounceCallback` (inline,
`src/library/scanner/libraryscannerdlg.h:26`) → `LibraryScanner::setAnnounceCallback`
(`libraryscanner.cpp:160-163`) → `TrackCollectionManager::setScanAnnounceCallback`
(`trackcollectionmanager.cpp:182-186`) → wired in `src/coreservices.cpp:640-643`
as a lambda calling `m_pLibrary->announceText(text)`. Speaks "Scanning
library" once per scan via the dialog's `showEvent()`. Test:
`src/test/libraryscannerdlg_speech_test.cpp`.

**(c) `DlgKeywheel` Tab trap fixed.** `src/dialog/dlgkeywheel.cpp`/`.h`.
Tab/Shift+Tab used to be swallowed by the dialog's own `eventFilter()` to
cycle key notations, making the dialog's Close button unreachable without a
mouse. Cycling moved to Up/Down (`eventFilter():57-76`, explicit comment
citing issue #63); Tab now falls through to `QDialog`'s default handling. A
new `notationChanged(const QString&)` signal (`dlgkeywheel.h:32`, emitted
`dlgkeywheel.cpp:139`) is connected in `src/mixxxmainwindow.cpp:1466-1478` to
speak the new notation via `Library::announceText()`. Test:
`src/test/dlgkeywheel_speech_test.cpp`.

---

## 7. Confirmation / narration fixes elsewhere in the library (issues #53, #60)

### 7.1 Purge / hide / remove / delete confirmations (issue #53, PR #74, merged)

`src/widget/trackconfirmdialogs.h`/`.cpp`, namespace `mixxx::trackconfirm`:
`purgeAnnouncement(int trackCount)` (`.cpp:18-27`), `confirmPurge(QWidget*,
Library*, int trackCount)` (`.cpp:29-43`, calls
`pLibrary->announceText(purgeAnnouncement(trackCount))` before the dialog,
`:31`; No is the default button), `hideOrRemoveAnnouncement(TrackModel::Capability,
int trackCount)` (`.cpp:45-90`), `deleteFromDiskAnnouncement(int trackCount)`
(`.cpp:92-109`). Pure text-builder functions, unit-tested without a
widget/Library by design.

A separate DAO-layer bug fixed in the same PR: `src/library/trackcollection.cpp`
`hideTracks()`'s `QMessageBox::question` (`:313-320`) had no explicit default
button, which resolves to `Ok` on Qt — a stray Enter proceeded with a
destructive hide. Fixed by passing `QMessageBox::Cancel` explicitly
(comment at `:307-312` cites issue #53). Tests:
`src/test/trackconfirmdialogs_test.cpp`, `src/test/trackcollection_test.cpp`.

### 7.2 WTrackMenu / sidebar context-menu narration (issue #60, PR #80, merged)

Covered in full in §2.6 — `Library::announceMenuHover()`, 20 call sites in
`wtrackmenu.cpp`, 2 each in `cratefeature.cpp`/`playlistfeature.cpp`.

### 7.3 Exit-dialog speech — still pending, not shipped

**Correction to an earlier draft of this spec, which described this as
merged. It is not.** Issue #115 (PR #118) proposes three new static
speech-builder helpers on `MixxxMainWindow`
(`confirmExitDeckPlayingSpeech()`, `confirmExitSamplerPlayingSpeech()`,
`confirmExitPreferencesOpenSpeech()`), matching the existing `*Speech()`
helper family pattern, each to be spoken via `announceText()` immediately
before the corresponding `QMessageBox::question()` inside `confirmExit()`.
None of this exists yet: `grep -rn "confirmExitDeckPlayingSpeech\|confirmExitSamplerPlayingSpeech\|confirmExitPreferencesOpenSpeech"
src/mixxxmainwindow.h src/mixxxmainwindow.cpp` returns nothing.
`MixxxMainWindow::confirmExit()` (`mixxxmainwindow.cpp:1836-1885`) still calls
`QMessageBox::question()` directly in all three branches (deck playing,
sampler playing, preferences open) with no announcement beforehand. PR #118
is open (not merged) as of `1cb23d38e7`. Until it lands, a blind user hitting
the exit confirmation while a deck is playing gets a dialog they cannot hear
announced — only the (inaccessible, per §1) modal box itself, which a screen
reader may or may not pick up depending on platform.

---

## 8. Work explicitly out of scope for this spec

- **AutoDJ speech/keyboard access (issue #61, PR #86)** — open, not merged.
  Touches `res/keyboard/*.kbd.cfg` `[AutoDJ]` section and
  `src/library/autodj/dlgautodj.{cpp,ui}` — see §1's correction. When it
  lands, the keyboard chords belong in Spec 08, and any AutoDJ dialog
  buddy/focus work belongs here.
- **YouTube (Creative Commons) library source (issue #67, PR #85)** — open,
  not merged. Its accessibility-specific work, when it lands, would live
  under the same `TrackModel::rowAccessibleText()` override mechanism as
  §3.2, plus a `statusChanged(QString)` narration signal.
- **First-run TTS onboarding (issue #105, PR #122)** — open, not merged. Would
  add the `OrientationPlayed` flag to `accessibilitysettings.h` (§5.1); the
  spoken content itself is spec 06/07 territory.
- **Issues #55, #66, #114** — confirmed confined to
  `src/util/announcementmanager.{cpp,h}`/`src/engine/engineearcon.{cpp,h}`
  and similar, no library/UI-surface files touched. Spec 05/06/07 territory.

---

## 9. Verification strategy: Gherkin / e2e

`features/` contains 9 `.feature` files (`audio_path`, `blind_dj_workflow`,
`ddj400_hardware`, `destructive_actions`, `first_run_boot`, `keyboard_only`,
`library_and_dialogs`, `macos_voiceover`, `windows_screenreader`), plus
`COVERAGE.md` (claims 257 scenarios / 363 individual runs, with an explicit
"what this doesn't cover" section: JAWS, Orca/AT-SPI on Linux,
non-English translations, decks 3/4, timecode vinyl, recording/broadcast as
tested features, Windows N editions, long-running stability) and `README.md`.
`library_and_dialogs.feature` and `destructive_actions.feature` are this
spec's two most relevant files.

`tools/e2e/` today contains `README.md`, `ax_driver.py`, `run_e2e.py`,
`m1_boot_speech.py`, `m2_ddj400_menu.py`, `m3_accessible_names.py`,
`m4_keyboard_readout.py` — all from already-merged, unrelated PRs (issue #28
and a follow-up M3 batch). **Correction to an earlier draft:** the `d1`
through `d8` destructive-action automation scripts, and `library_fixture.py`/
`library_actions.py`, described in earlier notes as issue #104/PR #121's
contribution, **do not exist on this branch**. PR #121 is open and has
contributed zero files so far. Do not describe automated destructive-action
e2e coverage as shipped.

A working local macOS build exists at
`/tmp/mixxx-target-state/build/Mixxx.app` (built from a merged combination of
open PRs, includes `--tts-log`) that could be used to start closing
`COVERAGE.md`'s "not yet run against a live Mixxx" caveat once the automation
scripts in PR #121 actually land — noted here as a concrete next step, not as
something already done.

---

## 10. Invariant summary (rebase checklist)

| ID | Invariant | Failure mode | Loud? |
|---|---|---|---|
| **L1** | The 5 fork `Library` signals are consumed by PMF connects at `announcementmanager.cpp:405-438` | Rename → **build error**. This is the safe one. | **Loud** |
| **L2** | All ~61 `announce*()` emit sites across 10 files, plus the ~24 `announceMenuHover()` sites across 3 files, survive | Dropped emit compiles, links, passes tests, and produces silence | **Silent** |
| **L3** | `BaseTrackTableModel::data()` keeps `Qt::AccessibleTextRole` in its role allowlist (`basetracktablemodel.cpp:464-473`) — **now live**, no longer pending | Accessible cell text vanishes; `rowAccessibleText()` silently returns empty for rating/colour/play-count | **Silent** |
| **L4** | 205 buddy associations across 19 `.ui` files stay valid | `check_ui_buddies.py` would catch it, but it is **pre-commit-only** and no `.gitea` workflow runs it | **Silent in CI** |
| **L5** | Self-voicing is the strategy; nothing in `res/skins/` is screen-reader reachable and nothing may assume it is | Future work built on a false assumption | n/a (design) |
| **L6** | 36 settings + 1913 lines of preferences UI have essentially zero test coverage; `ConfigKey` lookups are string-keyed | Renamed key silently returns the default | **Silent** |
| **L7** | `slotMoveVertical`'s `default:` case recovers by calling `setLibraryFocus(FocusWidget::TracksTable)` (`librarycontrol.cpp:881-887`) | Browse knob becomes inert from an unknown focus state, with no way back | **Silent** |

Six of seven fail silently, and the one loud invariant is loud only because it
happens to be compile-time. §3.5, §5.2 and Spec 08 §6 point at the same
structural gap: **this fork's accessibility work is protected by tests at the
behaviour layer and by essentially nothing at the wiring layer.**

---

## 11. Open questions for review

1. **Should skin-widget labelling proceed at all?** (§3.6) It is two
   decisions: accessible names for skin widgets, and focus policies to make
   them reachable. The second changes behaviour for sighted users. Nothing
   has moved on this since the last verification; still recommend gathering
   real screen-reader-tester feedback on which controls they actually try to
   reach before doing either.
2. **Wire the buddy lint into `.gitea/workflows/`** (§3.5). Smaller ask than
   it used to be to justify, given the surface it protects has grown from
   97/14 to 205/19 files since this gap was first identified.
3. **Minimum test coverage for `DlgPrefAccessibility`** (§5.2). Recommend the
   construction smoke test first — one test, highest coverage per line.
4. **Should `announceText()` get its own signal** rather than reusing
   `quickPickerItemHighlighted(text, -1, 0)`? (§2.1) The sentinel works but is
   surprising, and a distinct signal would make the ~61+24 call sites easier
   to audit by category.
5. **Land PR #118 (exit-dialog speech, issue #115).** This is the one
   concrete, small, already-written piece of pending work directly in this
   spec's territory (§7.3) — no architectural questions attached, just
   needs review and merge.

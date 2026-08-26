# Spec 06 — Announcement layer (`AnnouncementManager`)

**Status:** Verified current state — rebase contract (refreshed against the current PR wave)
**Branch:** `spec-speech` — content re-verified against the merged target-state tree
(`scratch-target-state` @ `1ba6bee542`: the `accessibility-improvements-2026-06-25`
tip plus all 19 other currently-open accessibility PRs merged together, with the
cross-PR bugs that merge surfaced found and fixed). 142 commits landed on top of
the previous verification point, `2390edf423`.
**Owner:** accessibility fork
**Related:** Spec 05 (speech engine and audio path), Spec 07 (spoken menu), Spec 04 (E2E testing)

## Purpose

`AnnouncementManager` (`src/util/announcementmanager.{h,cpp}`, 404 + 2855
lines, up from 284 + 2250) is still the largest piece of fork-specific code
and the layer that decides **what gets said**. Spec 05 covers how a string
becomes audible; this spec covers how a `ControlObject` change or a `Library`
signal becomes a string.

It also states, bluntly, the structural weaknesses a rebase is most likely to
trip over. One of the two flagged in the previous verification —
`--tts-log`'s "intent, not audibility" gap — is **now fixed** (Spec 05,
Invariant 6). The other — the `AllowMissingOrInvalid` / `NoWarnIfMissing`
silent-failure mode — is unchanged in shape and has grown in surface area.

## Background / current state

| Aspect | Value |
|---|---|
| Construction | `AnnouncementManager::create()`, `announcementmanager.cpp:323–338` (unchanged) |
| Instantiated at | `coreservices.cpp:657–663`, after `PlayerManager::bindToLibrary()` |
| Destroyed at | `coreservices.cpp:997–1001`, **before** the engine (issue #30) |
| `ControlProxy` construction sites (`make_parented<ControlProxy>(` + `std::make_unique<ControlProxy>(`) | **53** (was "40 observer construction sites") |
| `ControlFlag::AllowMissingOrInvalid` uses | **56** (was 45) |
| Owned `ControlObject`/`ControlProxy` member fields | `m_pSampleRate`, `m_pStatusButtons` (per-deck vector), `m_pRepeatButton`, `m_pShiftControl`, `m_pPadModeControl`, plus the **new** `m_pAutoDJNextButton` (`h:330`, issue #61) |
| `[Accessibility]` preference keys | **37** (`src/preferences/accessibilitysettings.h`, was 35 — see below) |
| `announceText(` call sites across `src/` | **94** (was 49) |
| Unit tests | `src/test/announcementmanager_test.cpp`, **4308 lines** (was 2954) |

The class is still deliberately decoupled: it takes a `Library*`, a
`PlayerManagerInterface*`, a `UserSettingsPointer`, an owned
`std::unique_ptr<TtsEngine>`, and raw `EngineTts*` / `EngineEarcon*` sinks.
Tests inject a spy `TtsEngine` and pass `nullptr` for both sinks. Effect and
QuickEffect **names** are still supplied by two injected `std::function`
resolvers, wired from `CoreServices` where `EffectsManager` lives.

The observed-key and owned-CO counts below are the most defensible
apples-to-apples successors of the previous verification's "48 distinct CO
keys observed" and "9 owned COs" — the exact dedup methodology behind those
two numbers wasn't fully recoverable, but every measurable proxy (construction
sites, `AllowMissingOrInvalid` uses, test file size, `announceText()` sites)
grew by roughly 20–90%, consistent with five new features landing in this
window.

## The two input channels

### 1. ControlObject observation (the bulk)

Unchanged in shape: every observer is a `ControlProxy` parented to the
manager, with a `connectValueChanged()` lambda. Global observers in `init()`;
per-deck observers in `connectGroupControls()`, called from `connectDeck()`
for each deck and again from `slotNumberOfDecksChanged()`. New this window:
the four AutoDJ observers (below) live directly in `init()` alongside the
other global observers, not in a separate module.

### 2. `Library` signals and the `announceText()` free-text channel

`Library::announceText(const QString&)` (`src/library/library.h:124`,
`src/library/library.cpp:496–498`) is unchanged, still a one-line adapter:

```cpp
void Library::announceText(const QString& text) {
    emit quickPickerItemHighlighted(text, -1, 0);
}
```

It still reuses the quick-picker signal with "no position", landing in
`slotQuickPickerItemHighlighted()`, still **unconditional** — not gated by any
`[Accessibility]` preference, because every caller is a deliberate one-off
event rather than ambient chatter.

**Verified count: 94 call sites** (up from 49) across `src/`, roughly double.
The channel has kept growing well beyond the boot dialogs it started as. New
feeders landing in this window, each detailed in its own subsection below:

| Feature | Issue | Where |
|---|---|---|
| Exit confirmation dialogs | #115 | `src/mixxxmainwindow.cpp` |
| Dialog-trap fixes (purge/hide/remove/delete confirmations, library scanner dialog, key-wheel notation) | #63 | `src/widget/trackconfirmdialogs.cpp`, `src/library/scanner/libraryscannerdlg.cpp`, `src/dialog/dlgkeywheel.cpp` |
| YouTube library source | #67 | `src/library/youtube/youtubefeature.cpp` |
| Menu-hover narration (`Library::announceMenuHover()`) | adjacent, likely issue #60 territory | `src/library/library.cpp:508–536` |

The pre-existing boot-dialog helpers and `ErrorDialogHandler` choke point
(issue #52) are unchanged in mechanism; see the previous section of this spec
for their tables if needed for the diff, and see Spec 08/09 for anything that
is really library/UI-layer territory rather than the announcement layer
itself (e.g. the full YouTube feature or the menu-hover feature are library
features first and speech feeders second — documented here only to the extent
they touch `AnnouncementManager`'s or `Library`'s speech surface).

### 2a. Exit confirmation dialogs (issue #115, new)

Lives in `src/mixxxmainwindow.{h,cpp}`, not in `AnnouncementManager` itself —
three pure helper functions alongside the existing boot-dialog helpers:

| Helper | Declared | Defined |
|---|---|---|
| `confirmExitDeckPlayingSpeech()` | `mixxxmainwindow.h:70` | `mixxxmainwindow.cpp:175–…` |
| `confirmExitSamplerPlayingSpeech()` | `mixxxmainwindow.h:71` | `mixxxmainwindow.cpp:…` |
| `confirmExitPreferencesOpenSpeech()` | `mixxxmainwindow.h:72` | `mixxxmainwindow.cpp:…–191` |

Called from `MixxxMainWindow::confirmExit()` (`mixxxmainwindow.cpp:1916–1976`,
itself invoked from the window-close path at `:1873`). Each of the three
branches speaks via `Library::announceText()` **before** the blocking
`QMessageBox::question()` call — deck-playing at `:1942`/`:1943–1946`,
sampler-playing at `:1951`/`:1952–1955`, preferences-open at
`:1961`/`:1962–1966`. Before this landed, **none** of the three exit
confirmation dialogs were wired into TTS at all — a blind user closing the
window mid-mix got a modal dialog they could neither hear nor necessarily see
land. `src/test/exitdialog_speech_test.cpp` (66 lines) calls the three static
helpers directly (no live `MixxxMainWindow`), asserting each string states
the situational fact, the question, and — importantly — which button is the
keyboard default ("No is selected by default" / similar), and that all three
strings are pairwise distinct.

### 2b. Dialog-trap fixes (issue #63, new)

Three independent dialogs that were reachable but silent — the "trap" is a
modal or focus-stealing dialog with no spoken indication it appeared:

**Track confirm dialogs** (`src/widget/trackconfirmdialogs.cpp`/`.h`, namespace
`mixxx::trackconfirm`): `purgeAnnouncement()`, `hideOrRemoveAnnouncement()`,
and `deleteFromDiskAnnouncement()` are pure text helpers; `confirmPurge()`
calls `Library::announceText()` **before** the still-blocking
`QMessageBox::question()`. Every generated string ends by stating the
keyboard default ("No is selected by default…" / "Cancel is selected by
default…"). Before this fix, purge in particular ran with zero spoken
confirmation. Tested in `src/test/trackconfirmdialogs_test.cpp` (130 lines),
including two tests that drive a real modal dialog via a deferred
`QTimer::singleShot(0, …)` to confirm the default button really is `NoRole`.

**Library scanner dialog** (`src/library/scanner/libraryscannerdlg.{h,cpp}`):
an injected `std::function<void(const QString&)>` announce callback
(`libraryscannerdlg.h:26–28`), fired once per scan on `showEvent()`
(`libraryscannerdlg.cpp:67–78`, guarded by `m_announcedThisScan`,
`h:59`) speaking `tr("Scanning library")`. Before this fix the dialog
appeared roughly two seconds into a scan and stole focus with no announcement
at all. Wired `TrackCollectionManager::setScanAnnounceCallback()`
(`trackcollectionmanager.cpp:182–186`) ← `LibraryScanner::setAnnounceCallback()`
(`libraryscanner.cpp:160–163`) ← `coreservices.cpp:640`. Tested in
`src/test/libraryscannerdlg_speech_test.cpp` (72 lines): announces exactly
once per scan, not on every `slotUpdate()` repaint.

**Key wheel notation** (`src/dialog/dlgkeywheel.{h,cpp}`): cycling the key
notation used to be a purely visual SVG redraw with no announcement.
`DlgKeywheel::notationChanged(QString)` now emits
`notationDisplayName(m_notation)` (`dlgkeywheel.cpp:133–139`) after redrawing;
`MixxxMainWindow` connects it to `Library::announceText()`
(`mixxxmainwindow.cpp:1553–1558`). Tested in `src/test/dlgkeywheel_speech_test.cpp`
(44 lines) as a pure lookup-table test of `notationDisplayName()`.

### 2c. YouTube library source (issue #67, new)

`src/library/youtube/` (12 new files, ~1477 lines) is a genuinely new library
source — not a networking "port" in the TCP sense — gated by
`ConfigKey("[Library]","ShowYouTubeLibrary")` (default true, **not** an
`[Accessibility]` key) and wired into `Library` at `library.cpp:269–272`. This
feature is properly library/UI territory (Spec 08/09); documented here only
for its speech surface, which bypasses `AnnouncementManager` entirely and
calls `Library::announceText()` directly, same as the boot dialogs:

| Site | Trigger | Text |
|---|---|---|
| `youtubefeature.cpp:97–103` | opening the feature | `"YouTube search. Type in the search box to find tracks."` |
| `youtubefeature.cpp:49–56` + `youtubesearchmodel.cpp:87/90/102/104` | search status changes | forwards the model's status string (searching / no results / N results) |
| `youtubefeature.cpp:119–135` | loading a search result to a deck | `"Downloading %1"` |
| `youtubefeature.cpp:154–166` | download progress | speaks only every 25% (`:158–160`) — never per-percent |
| `youtubefeature.cpp:210–216` | download failure | `"Download failed: %1"` |

Notably, CC-attribution text was **deliberately removed** from the spoken
strings (kept in the track's comment tag instead) — a real commit exists
purely to walk that back once it was found to be too chatty. Worth knowing if
a rebase reintroduces it from an older branch state.

## Observed ControlObject keys, by subsystem

The tables from the previous verification (global application/library/
recording, mixer/master, effects, per-deck, polled-not-observed) are
**unchanged in content and gate semantics** except for the additions below;
line numbers throughout the file have shifted upward by roughly 600 lines due
to the new material, so treat any specific `:NNNN` citation from the previous
verification as stale and re-grep rather than trust it verbatim.

### New — Auto DJ (issue #61)

| Key / control | Site | Gate | Behaviour |
|---|---|---|---|
| `[AutoDJ],enabled` | `announcementmanager.cpp:781–795` (`AllowMissingOrInvalid`) | always | Off → `"Auto DJ off"`. On → `"Auto DJ on. Next: <artist>, <title>"` via `getNextQueuedTrack()`, or bare `"Auto DJ on"` if the queue is empty |
| `[AutoDJ],fade_now` | `:800–809` | always | `"Fading now"` |
| `[AutoDJ],skip_next` | `:814–823` | always | `"Skipped"` — the newly-current track is announced separately by the normal track-load path |
| `[AutoDJ],tts_next` (**owned**, `ControlPushButton`, Trigger, `m_pAutoDJNextButton`, `h:330`) | `:828–836` | on demand (keyboard `Alt+Shift+N`) | `speak(formatAutoDJNext())` |

None of the four are gated by an `[Accessibility]` toggle — AutoDJ transport
confirmations are always-on, matching the pattern for other transport
confirmations elsewhere in the class. `formatAutoDJNext()`
(`announcementmanager.cpp:2637–2671`) builds "Auto DJ is on/off. Next:
<track>/Queue is empty. About <remaining> on <deck>." — the remaining-time
estimate is explicitly approximate (comment `:2646–2649`): the real crossfade
can start earlier, at the track's outro point.

`AutoDJProcessor::getNextQueuedTrack() const` — declared
`src/library/autodj/autodjprocessor.h:200`, defined
`src/library/autodj/autodjprocessor.cpp:1818`. Exposed to `AnnouncementManager`
via `Library::getAutoDJProcessor()` (`library.cpp:504–506`), set from
`pLibrary->getAutoDJProcessor()` at construction (`announcementmanager.cpp:445`).
`src/test/autodjprocessor_test.cpp` adds two tests directly against
`getNextQueuedTrack()` (empty queue → null; populated queue → head of queue).

## ControlObjects the manager *owns*

Unchanged from the previous verification (`[Tts],repeat`, `[Tts],shift`,
`[Tts],pad_mode`, and the six per-deck `[ChannelN],tts_*` triggers), plus the
new `[AutoDJ],tts_next` documented above. All Trigger-mode buttons still fire
on every press regardless of whether the underlying value changes.

`[Tts],pad_mode`'s spoken vocabulary gained a small but important refinement:
the DDJ-400 mapping now appends **"(not yet supported)"** for the four pad
layers that have no working behaviour behind them (Keyboard, Pad FX1, Pad
FX2, Key Shift — `Pioneer-DDJ-400-script.js:638–655`), so a blind DJ isn't
told a dead layer sounds identical to a working one. The mapping also now
bounces the CO through `0` before setting the real value
(`Pioneer-DDJ-400-script.js:668`) so re-pressing the mode you're already in
re-announces it — previously same-value writes were silently swallowed
(deliberately, for the Numark Scratch dedup case) with no way to ask "which
layer am I on?" without cycling through all eight. This mapping-side detail
belongs to Spec 01/08's territory; noted here only because the spoken text it
triggers is formatted by `AnnouncementManager`.

## Debounce, dedup, and name-once-then-value

The six-rule state machine from the previous verification (name-on-touch,
name-once, no-change suppression, context-clear-on-unrelated-utterance,
repeat-gets-full-text, announce-while-moving) is **unchanged in behaviour**.
What changed is the data structure backing rule 1–3 for the **keyed**
overload, to fix a real bug (issue #114):

### The per-control debounce fix (issue #114)

**The bug:** the keyed debounce path used to share a single pending slot
across *all* controls. Touching a second control (e.g. a volume fader) before
the first control's (e.g. a pitch fader's) debounce timer fired silently
discarded the first control's queued value — the DJ heard the control's name
on touch, but the value never followed.

**The fix:** a small struct and a hash, replacing the single slot:

```cpp
// announcementmanager.h:360-363
struct PendingControlAnnouncement {
    QString name;
    QString value;
};
```

- `QStringList m_pendingControlOrder` (`h:368`) + `QHash<QString, PendingControlAnnouncement> m_pendingControls` (`h:369`) — one pending entry **per control key**, in touch order.
- The *unkeyed* overload's single slot, `QString m_pendingControlText` (`h:350`), is **deliberately kept separate and unchanged** — it represents one conceptual readout (loop size, beat-jump size, effect focus) being stepped through, where "supersede, don't queue" is the correct behaviour (comment `h:344–349`).
- Both paths still share **one** `QTimer m_controlDebounce` (`h:343`, `kControlDebounceMs = 400`, set up `announcementmanager.cpp:398–399`) — the fix is in what gets queued when it fires, not in giving every key its own timer.
- `announceControlDebounced(text)` (unkeyed) — `cpp:2719–2727`.
- `announceControlDebounced(key, name, valueText)` (keyed) — `cpp:2729–2759`: inserts into the hash/order list, does name-on-touch/name-once logic, starts the debounce.
- `slotAnnouncePendingControl()` — `cpp:2776–2845`: flushes **both** the unkeyed text and the *entire* keyed hash in insertion order, wrapped in `beginSpeechBatch()`/`endSpeechBatch()` (`:2787`/`:2840` — see Spec 05, Invariant 3) so multiple controls settling in the same debounce window are joined into one utterance instead of each `speak()` barging in on the last.
- **Follow-on correctness fix bundled with this change:** `m_lastSpoken` (backing the Alt+Shift+R repeat command) is now built from `fullTextsForRepeat.join(...)` — the full wording of *every* flushed announcement — rather than whatever partial text the batched dispatch happened to produce, so repeat no longer returns a clipped or wrong string after a multi-control flush (comment `cpp:2789–2794`, `:2842–2844`).

`kControlContextMs = 8000` still governs "same control still in context, so
speak the value only" (now checked per-key against the hash rather than a
single shared field). Per-key jitter/no-change suppression against
`m_lastValueByKey` (`h:378`) is unchanged in intent.

### Taper traps and readout vocabulary

Unchanged from the previous verification: `getParameter()` vs. `get()` for
`ControlAudioTaperPot`s, plain-travel phrasing for `[Master],gain`/`headGain`,
`fractionText()`/`centerSplitText()`, the "B P M" spacing workaround, and the
NATO phonetic deck letters. No evidence these were touched in this PR wave;
re-verify by grep if a rebase touches `phoneticLetter()` or the fraction
helpers, since their line numbers have also shifted with the file's growth.

## Per-event feedback modes: speech / earcon / both

`emitCue()` remains the single dispatch point for every earcon-capable
transport event, with the same `0`/`1`/`2` semantics (speech only / sounds
only / both, default `2`). One addition: the `Xrun` earcon (Spec 05) does
**not** go through `emitCue()` — it has no speech/earcon feedback-mode split
of its own, since an audio dropout is an engine-level fact rather than a
transport event with a "how should this be phrased" question. If a rebase
wants text feedback for xruns, it needs its own gate, not a slot in the
`emitCue()` table.

## `[Accessibility]` preference keys

**Verified count: 37** (`src/preferences/accessibilitysettings.h`, was 35).
Exactly **two** new keys landed in this window (confirmed by diff, +31 lines
total including comments):

| Key | Type | Default | Line | Purpose |
|---|---|---|---|---|
| `ControllerNavigationWithoutFocus` | bool | `false` | `:231–235` | Lets controller-driven library navigation (e.g. the DDJ-400 browse encoder) keep working without OS keyboard focus on the Mixxx window; OR'd with the `--controller-navigation-without-focus` CLI flag (issue #64) |
| `OrientationPlayed` | bool | `false` | `:272–276` | One-shot flag backing the first-run orientation speech (issue #105, below); deliberately not exposed in the Preferences UI |

All other categories (event enables, feedback modes, style/phrasing, speech
engine) are unchanged in membership and defaults from the previous
verification — specifically, `AnnounceMixer` still defaults **on** (issue
#36); do not let a rebase revert that. Neither AutoDJ speech nor YouTube
speech got a dedicated `[Accessibility]` toggle — both are "always on," per
the comments at their observer sites (see above).

### First-run TTS onboarding orientation (issue #105, new)

`AnnouncementManager::maybeSpeakFirstRunOrientation()` (declared
`announcementmanager.h:182`, doc `:172–181`, defined `cpp:2188–2203`):

```cpp
void AnnouncementManager::maybeSpeakFirstRunOrientation() {
    if (m_settings.getOrientationPlayed()) {
        return;
    }
    m_settings.setOrientationPlayed(true);
    speak(tr("Welcome to Mixxx. "
             "Press Alt plus Shift plus A at any time to turn speech on or off. "
             "Press Alt plus 1 or Alt plus 2 to hear the full status of deck 1 or deck 2. "
             "Press Alt plus Shift plus R to repeat the last thing spoken. "
             "The Accessibility Guide and Quick Reference that shipped with Mixxx list "
             "every shortcut."));
}
```

The flag is marked played **before** speaking (comment `cpp:2192–2196`) —
deliberate, since it's a one-shot "has this install run before" flag, not a
"was the orientation actually heard" flag; a crash mid-utterance must not
repeat it forever. Called from `slotSoundDevicesReady()`
(`cpp:2171–2186`, call at `:2184`), inside the same `beginSpeechBatch()`/
`endSpeechBatch()` pair (`:2179`/`:2185`) that speaks "Mixxx ready," so a
first-ever boot doesn't have one announcement barge in on the other. See
Spec 05 for the boot-sequence plumbing this rides on (no changes needed
there — it reuses the existing `devicesSetup()` signal from issue #49).

---

# Invariants — and the danger

## Invariant A — `AllowMissingOrInvalid` / `NoWarnIfMissing` proxies fail *silently*

Still the fork's single largest structural risk, unchanged in mechanism from
the previous verification (`ControlProxy`'s constructor,
`src/control/controlproxy.cpp:10–18`: a missing control silently binds to a
shared, process-wide dummy that never changes; `AllowMissingOrInvalid`
suppresses the assertion; `NoWarnIfMissing` — used by `AccessMenuController`,
see Spec 07 — additionally suppresses the log warning). What changed is
**scale and, for the first time, coverage**:

- `AllowMissingOrInvalid` uses in `announcementmanager.cpp` alone: **56**
  (was 45).
- `ControlProxy` construction sites: **53** (was ~40).

**This gap is now guarded, not just documented — the biggest structural
change since the previous verification.** `src/test/a11ycontrols_test.cpp`
(637 lines, **new**; the previous verification listed this as "pending,
branch `guard-co-existence`, does not exist yet") now exists and is
substantial:

- Fixture `A11yControlExistenceTest` derives from `BaseSignalPathTest`, so
  every `ControlObject` it checks was created by real production code
  (`EngineMixer`/`Deck`/`EngineBuffer`/`CueControl`/`EffectsManager`), not
  fabricated by the test — closing exactly the gap the previous verification
  flagged (`announcementmanager_test.cpp` fabricates its own
  `[TestChannel1]` group and proves formatting logic, not that the real
  controls still exist).
- Header comment (`:1–40`) documents ~90 distinct control names / **263**
  concrete `(group, item)` pairs once decks and effect units are expanded,
  and states the failure mode in the same terms as this Invariant.
- `AnnouncementManagerOwnedControlsExist` (`:572–623`) constructs a real
  `AnnouncementManager` and asserts `[Tts],repeat`/`shift`/`pad_mode` and all
  six per-deck `tts_*` controls exist.
- `AccessMenuControlsExist` (`:536–561`) covers the seven `[AccessMenu]`
  controls too (see Spec 07) — this single test file now guards both classes
  the previous verification flagged as unguarded (`AllowMissingOrInvalid` in
  this class and `NoWarnIfMissing` in `AccessMenuController`).
- A trailing comment (`:625–636`) records a **deliberate non-coverage
  decision**: `[Shoutcast],enabled` is not checked, with the stated reasoning
  "no accessibility code reads it." **This reasoning is now stale** — see
  Spec 07's Invariant B, which found that `AccessMenuController`'s
  Broadcasting menu item *does* now read `[Shoutcast],enabled` (via
  `NoWarnIfMissing`) to speak its on/off state. This is a real, currently
  live gap: update `a11ycontrols_test.cpp`'s comment and add the key.

Three complementary new guard tests round out the coverage:

- `src/test/a11ymixpath_test.cpp` (309 lines) — guards that the engine sinks
  are actually wired into `EngineMixer::process()`'s output, not just
  functional in isolation. See Spec 05, Invariant 2.
- `src/test/announcetext_guard_test.cpp` (551 lines) — two tiers: a genuine
  behavioural test of `Library::announceText()` reaching the TTS transport
  (`AnnounceTextBehaviourTest`, `:176–225`), plus an explicitly-labelled
  **source-level census** (`AnnounceTextCallSiteCensusTest`, `:453–…`) that
  parses the fork's own `.cpp` files and checks each announcing function
  still textually contains an `announceText()` call. The file is explicit
  that the census tier proves the call site still exists in source, **not**
  that it's reachable or correct — a genuinely weaker guarantee than the
  behavioural tier, worth remembering before treating a green run here as
  proof of anything more.
- `src/test/keyboardbindings_test.cpp` (493 lines) — locale parity and chord
  collision detection across all 12 shipped `.kbd.cfg` files. See the Rebase
  checklist and Spec 07 for specifics.

**Net effect: this invariant is no longer purely a documentation warning.**
A rebase that renames an observed control now has a real chance of being
caught by `a11ycontrols_test.cpp`, provided the renamed control is in its
table — which is a **maintained, hand-written list**, not derived from the
production code automatically. Adding a new observer without adding it to
that table (or to `kValueControls` in Spec 07) reintroduces exactly the same
silent-failure risk with zero automated defence. Treat the guard test's table
as part of the contract, not a one-time chore.

Highest-risk keys, because they are upstream-owned and historically volatile,
are unchanged from the previous verification (`[Main],peak_indicator`, the
`[Master]`→`[Main]` migration family, `[EffectRack1_…]`/`[EqualizerRack1_…]`/
`[QuickEffectRack1_…]` naming, fork-added `[Library]` keys, `[Recording],status`).

## Invariant B — `--tts-log` now records outcome, not just intent (previously Invariant B; largely resolved)

The previous verification's headline claim — "the log answers 'was this
string requested,' never 'did the user hear it'" — **is now outdated**. See
Spec 05, Invariant 6 for the full mechanism; the summary for this spec's
purposes:

- `AnnouncementManager::speak()` still calls into the log **first**, before
  either early return (TTS-disabled at `cpp:1079–1080`, sink-destroyed at
  `:1089–1090`) — this ordering is unchanged and still deliberate, now
  producing a REQUESTED record either way (`:1074`), with the early-return
  path additionally producing a SUPPRESSED record with a `reason=`.
- A successfully dispatched utterance produces SPOKEN
  (`dispatchSpeech()`, `:1140`) and, downstream in the engine (Spec 05),
  SUPERSEDED / FLUSHED / COMPLETED depending on what actually happened to its
  audio.
- `endSpeechBatch()` gives the joined, batched text its **own** fresh
  REQUESTED id rather than reusing any constituent call's id (`:1169`,
  comment `:1164–1168`) — so a log reader correlating REQUESTED→outcome pairs
  by id will see the individual pre-batch calls as REQUESTED with no matching
  SPOKEN of their own, by design; only the combined utterance gets the full
  lifecycle.

**What is genuinely new and worth stating plainly for Spec 04's test
strategy:** an E2E harness can now assert COMPLETED for "this was audibly
delivered," not just that a string was requested. COMPLETED is described in
`ttslog.h` as the strongest signal available short of capturing the physical
device output — a proxy for audibility, not literal proof a human perceived
it (no assertion is made about volume, device routing to actual speakers,
etc.). Do not overstate what COMPLETED proves; it closes the specific gap
that hid the Smart Cue bug, not every conceivable "did the user hear this"
question.

## Invariant C — shutdown ordering (issue #30, unchanged)

Two independent guards against one use-after-free, unchanged in mechanism:

1. `EngineTts::~EngineTts()` emits `sinkDestroyed()` before destroying members
   (now also flushing pending audibility outcomes first — Spec 05);
   `AnnouncementManager::onTtsSinkDestroyed()` sets `m_ttsSinkDestroyed`,
   nulls `m_pTtsSink`, and clears the `TtsEngine`'s own sink pointer.
2. `CoreServices::finalize()` destroys the manager before the engine
   (`coreservices.cpp:997–1001`).

Keep both. The `ControlProxy` observing `[Tts],enabled` is parented to the
manager and can fire during teardown.

## Invariant D — the debounce/dedup state machine is load-bearing

Unchanged in substance from the previous verification, now extended by the
per-control hash (issue #114, above) rather than replaced. `m_lastValueByKey`
is still intentionally **session-lifetime** and never pruned; do not add an
eviction policy. The new `m_pendingControls` hash is bounded the same way —
by the number of physical controls that can be mid-debounce simultaneously,
which is small.

## Rebase checklist for this spec

1. All control keys `a11ycontrols_test.cpp` enumerates still exist after
   engine bring-up — this now runs in CI, but the table is hand-maintained;
   a new observer needs a new table entry, not just new observer code.
2. `getParameter()` (not `get()`) still used for `volume`, `pregain`,
   `[Master],gain`, `[Master],headGain`.
3. `speak()` early-return order unchanged; `ttslog::logRequested()` still
   called before both early returns.
4. `m_lastControlKey` clear/restore dance intact; `m_pendingControls`
   hash (issue #114) still keyed per-control, not a single shared slot.
5. `emitCue()` remains the sole earcon dispatch point for transport events;
   feedback-mode semantics (`0`/`1`/`2`) unchanged. The new `Xrun` earcon is
   deliberately outside this system (Spec 05).
6. 37 `[Accessibility]` keys present with the documented defaults; especially
   `AnnounceMixer = true`. Confirm the two new keys
   (`ControllerNavigationWithoutFocus`, `OrientationPlayed`) survive.
7. NATO phonetic letters and "B P M" spacing preserved verbatim.
8. `Library::announceText()` still routes through `quickPickerItemHighlighted`
   and `slotQuickPickerItemHighlighted()` stays **ungated**.
9. `ErrorDialogHandler::errorDialogAnnouncement` still emitted before
   `QMessageBox` construction, HTML still stripped, 300-char cap intact
   (mechanism unchanged; re-grep for current line numbers if touched).
10. Both shutdown guards intact.
11. `beginSpeechBatch()`/`endSpeechBatch()` intact and still used by: Smart
    Cue's track-load/pfl cascade, `slotSoundDevicesReady()`'s "Mixxx
    ready"/orientation pair, and `slotAnnouncePendingControl()`'s multi-control
    flush. This is now load-bearing production code (Spec 05, Invariant 3),
    not a pending branch.
12. `maybeSpeakFirstRunOrientation()` still gated by `OrientationPlayed` and
    still called from `slotSoundDevicesReady()`, not from skin-load or any
    earlier boot hook (it would then race ahead of a confirmed-open audio
    device — see Spec 05).
13. Exit-confirmation and dialog-trap speech (issues #115, #63) still fire
    **before** their respective `QMessageBox::question()`/`exec()` calls, not
    after — the whole point is to speak before the modal blocks input.
14. Update `a11ycontrols_test.cpp`'s stale "`[Shoutcast],enabled` is read by
    nothing" comment (`:625–636`) and add the key, since
    `AccessMenuController` now reads it (Spec 07, Invariant B).

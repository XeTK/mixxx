# Spec 06 — Announcement layer (`AnnouncementManager`)

**Status:** Verified current state — rebase contract
**Branch:** `spec-speech` (based on `accessibility-improvements-2026-06-25` @ `2390edf423`)
**Owner:** accessibility fork
**Related:** Spec 05 (speech engine and audio path), Spec 07 (spoken menu), Spec 04 (E2E testing)

## Purpose

`AnnouncementManager` (`src/util/announcementmanager.{h,cpp}`, 284 + 2250
lines) is the largest piece of fork-specific code and the layer that decides
**what gets said**. Spec 05 covers how a string becomes audible; this spec
covers how a `ControlObject` change or a `Library` signal becomes a string.

It also states, bluntly, the two structural weaknesses a rebase is most likely
to trip over: the `AllowMissingOrInvalid` silent-failure mode, and the
intent-versus-audibility gap in `--tts-log`.

## Background / current state (verified on this branch)

| Aspect | Value |
|---|---|
| Construction | `AnnouncementManager::create()`, `announcementmanager.cpp:323–338` |
| Instantiated at | `coreservices.cpp:647–653`, after `PlayerManager::bindToLibrary()` |
| Destroyed at | `coreservices.cpp:987–991`, **before** the engine (issue #30) |
| Observer construction sites | 40 (`make_parented<ControlProxy>` + `connectValueChanged`) |
| `ControlFlag::AllowMissingOrInvalid` uses | **45** |
| Distinct CO keys observed | **48** (loops expanded to patterns) |
| Distinct CO keys polled but not observed | 6 |
| Distinct COs *owned* (created) | 9 |
| `[Accessibility]` preference keys | **35** (`src/preferences/accessibilitysettings.h`) |
| Unit tests | `src/test/announcementmanager_test.cpp`, 2954 lines |

The class is deliberately decoupled: it takes a `Library*`, a
`PlayerManagerInterface*`, a `UserSettingsPointer`, an owned
`std::unique_ptr<TtsEngine>`, and raw `EngineTts*` / `EngineEarcon*` sinks.
Tests inject a spy `TtsEngine` and pass `nullptr` for both sinks.

Effect and QuickEffect **names** are supplied by two injected
`std::function` resolvers (`setEffectNameResolvers()`,
`announcementmanager.cpp:1569–1574`), wired from `CoreServices` where
`EffectsManager` lives (`coreservices.cpp:683–704`). Without them the
announcements degrade to numeric descriptions ("effect 2") rather than
failing.

## The two input channels

### 1. ControlObject observation (the bulk)

Every observer is a `ControlProxy` parented to the manager, with a
`connectValueChanged()` lambda. Global observers are set up in `init()`
(`announcementmanager.cpp:375–856`); per-deck observers in
`connectGroupControls()` (`:978–1567`), called from `connectDeck()` (`:1580`)
for each deck and again from `slotNumberOfDecksChanged()` (`:1608`) as decks
are added.

### 2. `Library` signals and the `announceText()` free-text channel

`Library::announceText(const QString&)` (`src/library/library.h:120`,
`src/library/library.cpp:485–487`) is a one-line adapter:

```cpp
void Library::announceText(const QString& text) {
    emit quickPickerItemHighlighted(text, -1, 0);
}
```

It reuses the quick-picker signal with "no position", landing in
`slotQuickPickerItemHighlighted()` (`announcementmanager.cpp:1740–1752`).
That slot is **unconditional** — deliberately not gated by any
`[Accessibility]` preference, because every caller is a deliberate one-off
event rather than ambient chatter.

**Verified count: 49 direct call sites**, plus one signal connection
(`ErrorDialogHandler`, below) that feeds the same channel without calling it
directly. The channel has grown well beyond the boot dialogs it started as.

| Area | File | Sites | Examples |
|---|---|---|---|
| Boot / hardware dialogs | `src/mixxxmainwindow.cpp` | 11 | 450 (AccessMenu speak callback), 737/816/848 (sound-device dialogs), 880 (no output), 1305/1334/1363/1392 (no vinyl / passthrough / mic / aux input), 1460 (library scan summary), 1812 (direct rendering) |
| Playlist dialogs | `src/library/trackset/baseplaylistfeature.cpp` | 16 | create / rename / duplicate / delete, plus the "name already exists" and "blank name" validation messages |
| Crate dialogs | `src/library/trackset/crate/cratefeature.cpp` | 7 | rename, delete, "You entered: %1" echo |
| Crate creation helper | `src/library/trackset/crate/cratefeaturehelper.cpp` | 9 | create / duplicate + validation |
| Track table | `src/widget/wtracktableview.cpp` | 3 | 1165/1169 ("Moved to position %1 of %2"), 1529 |
| Library views | `src/library/library.cpp` | 2 | 669 ("Playlists view"), 671 ("Crates view") |
| Library control | `src/library/librarycontrol.cpp` | 1 | 800 ("Deck %1, no track loaded") |

The boot-dialog strings are factored into pure helpers (`noOutputSpeech()`,
`noVinylControlInputSpeech()`, `libraryScanSummarySpeech()`, …) declared at
`mixxxmainwindow.h:49–52` and unit-tested in
`src/test/bootdialog_speech_test.cpp` without needing a running app.

### 2b. Error dialogs via `ErrorDialogHandler` (issue #52, merged)

The newest and highest-leverage feeder into the free-text channel. Rather than
touching every call site, the fork emits from the **single choke point** every
`requestErrorDialog()` call funnels through:

| Element | Location |
|---|---|
| Signal declaration | `src/errordialoghandler.h:158` — `void errorDialogAnnouncement(const QString& text)` |
| Emit site | `src/errordialoghandler.cpp:180–181`, inside `ErrorDialogHandler::errorDialog()` |
| Text sanitiser | `spokenMessageText()`, `src/errordialoghandler.cpp:36–41` |
| Length cap | `kMaxSpokenMessageChars = 300`, `:30` |
| Connection | `coreservices.cpp:663–666` → `Library::announceText` |
| Tests | `src/test/errordialoghandler_test.cpp`, 4 cases |

Behaviour worth preserving:

- **Emitted *before* `QMessageBox` is shown**, so speech is not deferred until
  a modal `exec()` returns — i.e. until the user has already dismissed a dialog
  they could not perceive.
- **HTML stripped** via `QTextDocumentFragment::fromHtml(...).toPlainText()
  .simplified()`; several callers embed `<br>` / `<b>` in the primary message,
  which would otherwise be read aloud literally.
- **Truncated at 300 chars** with an ellipsis. The collapsed "Show Details"
  section (backtraces etc.) is deliberately **not** spoken.
- Format is `tr("%1. %2").arg(title, message)`.
- `ErrorDialogHandler` is a process-wide singleton created in `main.cpp`
  **before** `CoreServices` builds the announcement machinery, so error dialogs
  raised in very early boot have no listener and are simply not spoken. The
  dialog is still shown normally either way.

This closes a real gap: these dialogs are frequently non-modal and may not even
take focus, so a TTS-only user previously got no indication anything went
wrong.

## Observed ControlObject keys, by subsystem

40 observer construction sites; loops expand to 48 distinct key patterns. A
rebase that renames any of these upstream kills the corresponding feature
**silently** — see the risk section below.

### Global — application, library, recording

| Key | Site | Gate | Behaviour |
|---|---|---|---|
| `[Library],sort_column` | `:441` | `AnnounceSort` | Starts the 400 ms sort debounce |
| `[Library],sort_order` | `:449` | `AnnounceSort` | Same debounce (a column change also resets order) |
| `[Library],focused_widget` | `:469` | `AnnounceLibraryFocus` | "Search bar" / "Sidebar" / "Track list" |
| `[Tts],enabled` | `:479` | always | Speaks "Speech on" on the 0→1 edge only (off flushes the FIFO, so it can't be spoken) |
| `[Recording],status` | `:492` | `AnnounceRecording` | 0 = off, 1 = ready, 2 = recording; announces both transitions of `>= 2.0` |
| `[Main],peak_indicator` | `:515` | `AnnounceClipping` | Rising edge → `Clipping` earcon, throttled to one per 5 s |

### Global — mixer and master

| Key | Site | Gate | Readout |
|---|---|---|---|
| `[Master],crossfader_lock` | `:534` | always | "Crossfader locked/unlocked" (Alt+X) |
| `[Master],crossfader` | `:547` | `AnnounceMixer` | "left/right <fraction>", "center" within ±0.05; **suppressed while locked** |
| `[Master],headMix` | `:579` | `AnnounceMixer` | "cue/main <fraction>", "even" within ±0.05 |
| `[Master],gain` | `:617` | `AnnounceMixer` | "Main volume …" — read via `getParameter()`, not `get()` |
| `[Master],headGain` | `:617` | `AnnounceMixer` | "Headphone volume …" — same |
| `[Master],headSplitDecks` | `:687` | always | "Split cue on. Deck 1 left, deck 2 right" (Alt+H) |
| `[Master],disable_touch_scratch` | `:710` | always | "Jog wheel touch locked/unlocked" (Alt+J) |
| `[BeatClick],enabled` | `:698` | always | "Beat click on/off" (Alt+B) |

### Global — effects

| Key | Site | Gate | Notes |
|---|---|---|---|
| `[EffectRack1_EffectUnitN],mix` (N = 1..4) | `:659` | `AnnounceMixer` | "Effect N mix …" |
| `[EffectRack1_EffectUnitN],super1` (N = 1..4) | `:659` | `AnnounceMixer` | "Effect N super …" |
| `[EffectRack1_EffectUnitU_EffectS],enabled` (16) | `:729` | `AnnounceEffects` | "Unit U <name> on/off"; name from resolver |
| `[EffectRack1_EffectUnitU_EffectS],loaded_effect` (16) | `:747` | `AnnounceEffects` | "Unit U: <name> loaded" / "cleared"; debounced |

### Per deck (`connectGroupControls(group, deckIndex)`)

| Key | Site | Gate | Behaviour |
|---|---|---|---|
| `play` | `:1008` | `AnnouncePlay` / `AnnounceStop` | Distinguishes real play from a held-cue preview by reading `cue_default`; suppresses "Stopped" when `end_of_track` fired |
| `end_of_track` | `:1045` | `AnnounceEndOfTrack` | "End of track. N minutes M seconds remaining." |
| `start`, `cue_gotoandstop` | `:1070` | `AnnouncePlay` | "<deck> back to start" + `Restart` earcon |
| `pfl` | `:1083` | `AnnounceCue` | "<deck> headphone cue on/off" — "headphone cue", never bare "cue" |
| `sync_enabled` | `:1110` | `AnnounceSync` | Latch-aware, see below |
| `keylock`, `quantize` | `:1153` | `AnnounceSync` | "<deck> key lock on" etc. |
| `loop_enabled` | `:1170` | `AnnounceLoop` | "<deck> loop N beats" (reads `beatloop_size`) |
| `hotcue_1..8_status` | `:1199` | `AnnounceHotcue` | Only 0↔1 edges; suppressed for 1 s after a track change |
| `hotcue_1..8_activate` | `:1222` | `AnnounceHotcue` | Only when the pad was **already** set (otherwise `_status` speaks "set") |
| `cue_set` | `:1243` | `AnnounceHotcue` | "<deck> cue set" |
| `beatloop_size` | `:1255` | `AnnounceLoop` | Debounced |
| `beatjump_size` | `:1270` | `AnnounceLoop` | Debounced |
| `beatjump_forward`, `beatjump_backward` | `:1291` | `AnnounceLoop` | "<deck> jump forward N beats"; debounced |
| `rate_ratio` | `:1310` | `AnnounceTempo` | Keyed readout: "<deck> pitch" then "up 2 percent. 128 B P M" |
| `beats_set_halve`, `beats_set_double` | `:1343` | `AnnounceTempo` | "<deck> B P M halved/doubled". Deliberately does **not** read the new BPM back — the engine updates `bpm` on its own schedule, so it would race; `tts_bpm` gives the exact number |
| `volume` | `:1360` | `AnnounceMixer` | `getParameter()`, not `get()` — see the taper note |
| `pregain` | `:1383` | `AnnounceMixer` | Center-split: "trim plus a quarter" |
| `[EqualizerRack1_<group>_Effect1],parameter1/2/3` | `:1416` | `AnnounceMixer` | "E Q low/mid/high" (or "low/mid/high" in concise mode), center-split from unity=1 over range 0..4 |
| `[QuickEffectRack1_<group>],super1` | `:1450` | `AnnounceMixer` | "<deck> filter", center-split around 0.5 |
| `[EffectRack1_EffectUnitU],group_<group>_enable` (U = 1..4) | `:1469` | `AnnounceEffects` | "<deck> effect unit U on/off" |
| `[QuickEffectRack1_<group>],loaded_chain_preset` | `:1488` | `AnnounceEffects` | "<deck> filter: <preset>"; debounced |
| `vinylcontrol_enabled` | `:1514` | **none** | Always spoken |
| `vinylcontrol_mode` | `:1524` | **none** | absolute / relative / constant |
| `vinylcontrol_cueing` | `:1546` | **none** | needle-drop cueing off / cue point / nearest hotcue |

The three DVS observers are ungated on purpose (`announcementmanager.cpp:1509–1513`):
mode changes also happen *automatically* (a loop or seek drops absolute mode to
relative; the end of the record switches to constant), and without feedback a
blind DJ has no way to know why the deck stopped following the turntable.

**Sync is latch-aware** (`:1103–1140`). `sync_enabled` is `LongPressLatching`:
it flips to 1 on press and reverts if released within 300 ms. A `QTimer` probes
at `kSyncLatchProbeMs = 450` (`:63`) to tell the two apart:

| Outcome | Spoken |
|---|---|
| Held past 450 ms | "<deck> sync locked" |
| Released inside the window | "<deck> beat synced. Hold sync to lock" |
| 1 → 0 outside the window | "<deck> sync off" |

### Polled, not observed

Read on demand via the `readGroupControl()` helper (`:65–67`), which
constructs a throwaway `ControlProxy` with `AllowMissingOrInvalid`:

`cue_default`, `duration`, `playposition`, `bpm`, `key`, and
`[Library],key_notation`.

## ControlObjects the manager *owns*

| Key | Type | Mode | Purpose |
|---|---|---|---|
| `[Tts],repeat` | `ControlPushButton` | Trigger | Re-speaks `m_lastSpoken` (Alt+Shift+R) |
| `[Tts],shift` | `ControlPushButton` | default | Controller mappings set 1 while hardware shift is held; announced on **press only** |
| `[Tts],pad_mode` | `ControlObject` | — | Cross-controller pad-layer vocabulary, see table below |
| `[ChannelN],tts_status` | `ControlPushButton` | Trigger | Full deck status |
| `[ChannelN],tts_time` | `ControlPushButton` | Trigger | Time remaining |
| `[ChannelN],tts_bpm` | `ControlPushButton` | Trigger | BPM |
| `[ChannelN],tts_key` | `ControlPushButton` | Trigger | Musical key |
| `[ChannelN],tts_bar` | `ControlPushButton` | Trigger | Bar / beat position |
| `[ChannelN],tts_track` | `ControlPushButton` | Trigger | Artist / title |

All six per-deck readouts are created in one table-driven loop
(`:982–1006`). **Trigger** mode is required so every press fires even though
the value doesn't change.

`[Tts],pad_mode` values (`:822–852`) — a fixed cross-controller vocabulary,
so a mapping sets the value matching the mode button pressed. Setting the same
value again is a no-op (no CO change), which conveniently deduplicates hardware
that fires one mode press for both decks at once (Numark Scratch):

| Value | Spoken as "Pads, …" |
|---|---|
| 1 | hot cues |
| 2 | beat loop |
| 3 | beat jump |
| 4 | sampler |
| 5 | keyboard |
| 6 | pad effects 1 |
| 7 | pad effects 2 |
| 8 | key shift |
| 9 | loop roll |

Keyboard bindings (`res/keyboard/en_US.kbd.cfg:45–50`, `:109–114`) —
odd hotkeys are deck 1, even are deck 2:

| Readout | Deck 1 | Deck 2 |
|---|---|---|
| `tts_status` | Alt+1 | Alt+2 |
| `tts_time` | Alt+3 | Alt+4 |
| `tts_bpm` | Alt+5 | Alt+6 |
| `tts_key` | Alt+7 | Alt+8 |
| `tts_bar` | Alt+9 | Alt+0 |
| `tts_track` | Alt+Shift+T | Alt+Shift+Y |

Readout formatters (`formatDeckStatus`, `formatTimeRemaining`, `formatBpm`,
`formatKey`, `formatBarPosition`, `formatTrackName`,
`announcementmanager.cpp:1950–2109`) are all **public** so tests can verify
formatting without a running app. `formatBarPosition()` assumes 4/4 and
returns "Before first beat." / "No beat grid." rather than guessing.

## Debounce, dedup, and name-once-then-value

The mechanism that makes a continuously-moving knob usable rather than
maddening. Two overloads of `announceControlDebounced()`:

| Overload | Site | Use |
|---|---|---|
| `(text)` | `:2126` | One-shot debounced string (loop size, effect loaded, …) |
| `(key, name, valueText)` | `:2134` | Keyed readout for physical knobs and faders |

The keyed path is the interesting one. Timing constants (`:35–63`):

| Constant | Value | Meaning |
|---|---|---|
| `kSelectionDebounceMs` | 400 | Track-browsing selection |
| `kSearchDebounceMs` | 600 | Library search |
| `kControlDebounceMs` | 400 | Knob/fader at rest |
| `kSortDebounceMs` | 400 | Sort column + order collapse into one utterance |
| `kMovingThrottleMs` | 300 | Min gap in announce-while-moving mode |
| `kControlContextMs` | 8000 | How long a control keeps its spoken "context" |
| `kHotcueSuppressMs` | 1000 | Hotcue silence after a track change |
| `kClippingThrottleMs` | 5000 | Min gap between clipping warnings |
| `kSyncLatchProbeMs` | 450 | Long-press latch probe |

### The rules, in order

1. **Name on touch.** The first movement of a control speaks its name
   immediately ("Deck 1 volume") so the DJ knows what they grabbed. The value
   follows once it stops (`:2141–2161`).
2. **Name once.** While the *same* key keeps moving within
   `kControlContextMs`, only the new value is spoken ("a half"), not the name
   again. A long slow drag must not re-announce the name halfway through
   (`:2150–2153`).
3. **No-change suppression.** If the readout text is identical to the last one
   announced for that key (`m_lastValueByKey`, session-lifetime), **nothing is
   said at all** — neither name nor value (`:2154–2155`, `:2198–2208`). This is
   the worn-pot guard: a jittery potentiometer resting near a boundary would
   otherwise chant the same readout forever.
4. **Any unrelated utterance clears the context.** `speak()` clears
   `m_lastControlKey` on its very first line (`:874–878`), so the next control
   move names the control again.
   `slotAnnouncePendingControl()` deliberately **restores** the context after
   its own `speak()` (`:2211–2217`) — a subtle dance worth preserving verbatim.
5. **Repeat gets the full text.** Even when only the value was spoken,
   `m_lastSpoken` is set to `name + " " + value` (`:2214`) so Alt+Shift+R
   re-speaks something intelligible standing alone.
6. **Announce-while-moving** (`AnnounceWhileMoving`, default off) speaks
   immediately, throttled to `kMovingThrottleMs`, and *still* arms the debounce
   so the resting value is always spoken (`:2165–2178`).

### Taper traps (do not "simplify" these)

`volume`, `pregain`, `[Master],gain` and `[Master],headGain` are
`ControlAudioTaperPot`s. `get()` returns the **linear gain multiplier**, not
the fader position. The code reads `getParameter()` instead
(`:1355–1359`, `:603–608`). Using `get()` makes a half-way fader read out as
roughly a quarter. This is a correctness bug that reads as a plausible number,
so it will not be caught by eye.

Similarly, `[Master],gain` / `headGain` are spoken as **plain travel**
("a half"), not center-split — testers reading "minus a quarter" concluded the
volume had gone negative.

### Readout vocabulary

`fractionText()` (`:92–129`) snaps to the user's `MixerFractionDetail`
denominator (4 / 8 / 16), then expresses in sixteenths so naming simplifies
identically at every detail level: zero, a sixteenth, an eighth, a quarter,
3 eighths, a half, … full. `centerSplitText()` (`:134–149`) wraps it as
"center" / "plus X" / "minus X".

Two deliberate pronunciation workarounds that must survive a rebase:

- **"B P M" with spaces** (`:1928–1929`) — engines read the letters
  individually instead of attempting a word.
- **NATO phonetic deck letters** (`phoneticLetter()`, `:248–282`). An earlier
  version used short words ("Ay", "Bee", "See"). "Ay" is a real homograph
  (the vote/nautical interjection) and macOS speech resolved "Ay", "Aye" and
  "eye" to **byte-for-byte identical audio** — confirmed by rendering and
  diffing. All 26 NATO words were rendered and diffed for collisions. Costs a
  syllable or two; correctness matters more for a feature meant to be trusted
  at face value. The comma in `tr("Deck, %1")` (`:2088–2099`) is equally
  deliberate: without it engines glue the letter on ("Decka").

## Per-event feedback modes: speech / earcon / both

`emitCue(earconId, deckIndex, speechText)` (`:938–976`) is the single dispatch
point for every earcon-capable transport event:

```
mode != 1  →  speak(speechText)
mode != 0  →  m_pEarcon->trigger(id, pan)
```

so `0` = speech only, `1` = sounds only, `2` = both (the default). Pan is
derived from `deckIndex`: 0 → Left, 1 → Right, anything else → Center.

| `EngineEarcon::Id` | Preference key | Default |
|---|---|---|
| `Play` | `FeedbackModePlay` | 2 |
| `Stop` | `FeedbackModeStop` | 2 |
| `EndOfTrack` | `FeedbackModeEndOfTrack` | 2 |
| `CueOn` / `CueOff` / `CuePreview` | `FeedbackModeCue` | 2 |
| `Restart` | `FeedbackModeRestart` | 2 |
| `LoopOn` / `LoopOff` | `FeedbackModeLoop` | 2 |
| `Clipping` | `FeedbackModeClipping` | 2 |

The mode is consulted **only** when the matching `Announce*` toggle is already
on — the enable check is the caller's responsibility (`announcementmanager.h:131–134`).
Every other announcement in the class is speech-only.

The `CuePreview` earcon exists because repeated cue taps while beat-matching
turned "Cue" into an irritating chant (`:1020–1026`); the Cue feedback mode
lets the user swap it for a short tick.

## `[Accessibility]` preference keys

**Verified count: 35** (`src/preferences/accessibilitysettings.h`, all via
`DEFINE_PREFERENCE_HELPERS`). The brief said ~36 — the code wins.

### Event enables (17, all default `true`)

`AnnounceTrackSelection`, `AnnounceTrackLoad`, `AnnouncePlay`, `AnnounceCue`,
`AnnounceStop`, `AnnounceEndOfTrack`, `AnnounceLibraryFocus`,
`AnnounceStartup`, `AnnounceSearch`, `AnnounceSort`, `AnnounceSync`,
`AnnounceTempo`, `AnnounceLoop`, `AnnounceHotcue`, `AnnounceRecording`,
`AnnounceEffects`, `AnnounceMixer`.

> `AnnounceMixer` defaults **on** (issue #36). A blind DJ needs to hear
> faders and EQ; the debounce plus the opt-in while-moving mode keep it from
> being chatty mid-mix. Do not let a rebase revert this to `false`.

### Per-event feedback modes (7, all default `2`)

`FeedbackModePlay`, `FeedbackModeStop`, `FeedbackModeEndOfTrack`,
`FeedbackModeCue`, `FeedbackModeRestart`, `FeedbackModeLoop`,
`FeedbackModeClipping`.

### Other event enables (2)

`AnnounceClipping` (default `true` — a safety signal, not a style choice),
`AnnouncePlaylist` (default `true`).

### Style and phrasing (5)

| Key | Type | Default | Meaning |
|---|---|---|---|
| `MixerReadoutStyle` | int | 0 | 0 = fractions, 1 = percentages |
| `MixerFractionDetail` | int | 1 | 0 = quarters, 1 = eighths, 2 = sixteenths |
| `AnnounceWhileMoving` | bool | false | Speak during movement, throttled |
| `DeckNamesAsNumbers` | bool | false | "Deck 1" instead of "Deck, Alpha" |
| `ConciseAnnouncements` | bool | false | Drop "Deck" / "E Q" filler; single-fact hotkeys speak just the value |

### Speech engine (4)

| Key | Type | Default | Meaning |
|---|---|---|---|
| `TtsVoice` | QString | `""` | Empty = system default voice |
| `TtsRate` | int | 0 | [-10, 10] |
| `TtsRoute` | int | 0 | 0 = headphone/cue, 1 = main. Matches `EngineTts::Route` |
| `TtsVoiceQualityFilter` | int | 0 | macOS only; filters the voice picker by `AVSpeechSynthesisVoice` quality tier. Browsing aid only |

Smart cue is deliberately **not** here — it lives under `[Controls]`, owned by
`DlgPrefDeck` (`announcementmanager.cpp:21–24`), because it is a general
deck-loading behaviour, not accessibility-specific. `AnnouncementManager`
therefore keeps a raw `m_pConfig` alongside `m_settings` to read it.

`speak()` re-reads `TtsVoice`, `TtsRate` and `TtsRoute` on every utterance and
pushes them down only on change (`:904–933`), so preference edits take effect
on the next spoken string with no restart and no explicit apply.

---

# Invariants — and the danger

## Invariant A — 45 `AllowMissingOrInvalid` proxies fail *silently*

This is the fork's single largest structural risk. It is not a code-quality
observation; it is a specific, mechanical failure mode.

`ControlProxy`'s constructor, verified at `src/control/controlproxy.cpp:10–18`:

```cpp
ControlProxy::ControlProxy(const ConfigKey& key, QObject* pParent, ControlFlags flags)
        : QObject(pParent) {
    m_pControl = ControlDoublePrivate::getControl(key, flags);
    if (!m_pControl) {
        DEBUG_ASSERT(flags & ControlFlag::AllowMissingOrInvalid);
        m_pControl = ControlDoublePrivate::getDefaultControl();
    }
    DEBUG_ASSERT(m_pControl);
}
```

When the control does not exist, the proxy binds to a **shared, process-wide
dummy control that never changes value**. `ControlFlag::AllowMissingOrInvalid`
(`src/control/control.h:25`) is `AllowInvalidKey | NoAssertIfMissing`, so the
`DEBUG_ASSERT` passes and no assertion fires.

Consequences, in order of nastiness:

1. `connectValueChanged()` succeeds. The connection is real. It is connected to
   a control that will never change.
2. **The observer never fires.** Not once.
3. There is **no crash**, **no test failure**, and **no exception**.
4. `ControlProxy` **never re-binds**. Binding early is permanent — this is the
   exact hazard `EngineBeatClick` was restructured to avoid
   (`src/engine/enginebeatclick.h:31–40`: constructing per-deck proxies in the
   constructor latched onto the dummy forever, reading "not playing"
   regardless of actual deck state).
5. For a blind user, the only perceptible signal is **silence** — which is
   indistinguishable from "this feature was never implemented".

So: **if upstream renames one observed control object during the rebase, the
corresponding announcement simply stops existing, and every automated check
still passes.**

Highest-risk keys, because they are upstream-owned and historically volatile:

| Key | Why it is at risk |
|---|---|
| `[Main],peak_indicator` | Already carries an alias to `[Master],PeakIndicator`; the `[Master]` → `[Main]` migration is ongoing upstream |
| `[Master],crossfader`, `crossfader_lock`, `headMix`, `gain`, `headGain`, `headSplitDecks` | Same `[Master]` → `[Main]` migration |
| `[EffectRack1_EffectUnitN…]` | Effects group naming has churned repeatedly upstream |
| `[EqualizerRack1_<group>_Effect1],parameterN` | Same |
| `[QuickEffectRack1_<group>],super1` / `loaded_chain_preset` | Same |
| `[Library],focused_widget`, `sort_column`, `sort_order` | **Fork-added** COs — a rebase can drop the code that creates them without touching the observer |
| `[Recording],status` | Upstream-owned |

Exactly **one** observed proxy is bound *without* the flag: `[ChannelN],play`
(`announcementmanager.cpp:1008`). If that one goes missing it asserts in a
debug build. That is the behaviour we want everywhere and cannot have, because
the manager legitimately connects before some controls exist.

> **Guard test — Status: pending, branch `guard-co-existence`.**
> `src/test/a11ycontrols_test.cpp` is intended to assert, once, that every
> control key the accessibility layer observes actually exists after a normal
> engine + player-manager bring-up, converting the silent failure into a red
> test.
>
> **Verified: `guard-co-existence` currently points at `ac931df0ed` and
> contains no such file.** It has not been written yet. Until it lands, the
> only defence against this failure mode is a human listening to the app. Say
> so out loud when planning the rebase.

Note that `EngineBeatClick` (`enginebeatclick.cpp:50–53`, `:58–63`) and
`EngineEarcon` (`engineearcon.cpp:98–101`) use the same flag for
`[Tts],route_to_main` and the per-deck `play` / `beat_distance` / `bpm`
proxies, so the same failure mode reaches into the engine layer. Any guard test
should cover those too.

## Invariant B — `--tts-log` records *intent*, never audibility

`--tts-log PATH` (`src/util/cmdlineargs.cpp:394–400`, `:476–477`;
accessor `cmdlineargs.h:51–53`) appends every spoken string to a file. Spec 04
identifies it as "the single highest-value test hook". It is — but only if you
understand exactly what it measures.

`AnnouncementManager::speak()`, `announcementmanager.cpp:874–936`, in order:

| Line | Statement |
|---|---|
| 878 | `m_lastControlKey.clear();` |
| **883–890** | **`--tts-log` write** |
| 893–895 | `if (m_pTtsSink && !m_pTtsSink->isUserEnabled()) return;` — **TTS disabled** |
| 900–902 | `if (m_ttsSinkDestroyed) return;` — **shutdown race** |
| 904–933 | voice / rate / sample-rate / route sync |
| 935 | `m_pTts->say(text);` |

The log write happens **before both early returns**, deliberately — the comment
at `:880–882` says "Log before the TTS-disabled early return so the hook
captures all utterances."

Therefore the log answers **"was this string requested?"** and never **"did the
user hear it?"**. Specifically, a line appears in the log even when:

| Condition | Where it drops | Detectable in log? |
|---|---|---|
| TTS toggled off by the user | `:893` | **no** |
| Engine sink already destroyed (shutdown) | `:900` | **no** |
| No sound device open yet (boot dialogs) | Spec 05 — no engine output exists | **no** |
| Barge-in: superseded by a newer utterance | `TtsEngine::say()` generation counter | **no** |
| FIFO full; synthesizer drops the tail (macOS) | `ttsenginemac.mm:143–145` | **no** |
| Speech routed to a headphone bus the DJ isn't monitoring | `EngineTts::process()` | **no** |
| `NullTtsEngine` (no backend compiled in) | `ttsengine.cpp:652` | **no** |

**Any E2E harness built on `--tts-log` alone cannot distinguish "spoken" from
"requested but silent."** This is the precise reason the Smart-Cue barge-in bug
(Spec 05, Invariant 3) hid for weeks: the log showed the track-load
announcement every single time. It was never audible.

Consequences for the test strategy in Spec 04:

- `--tts-log` assertions are valid for **string content and ordering** — did we
  compute the right text, in the right order, with the right debouncing?
- They are **not** valid as an audibility assertion. Scenario 1 ("assert TTS
  log contains 'Mixxx ready'") passes on a build with no audio backend at all.
- To close the gap, a second sink is needed at or below
  `EngineTts::writeSamples()` / `EngineTts::process()` — i.e. log what actually
  reached the FIFO, and what survived the flush. That instrumentation does not
  exist today.

Do not "tidy" the log write to sit after the early returns during a rebase. Its
current position is a deliberate choice; what is missing is a *second*,
lower-level hook, not a relocated one.

## Invariant C — shutdown ordering (issue #30)

Two independent guards against one use-after-free:

1. `EngineTts::~EngineTts()` emits `sinkDestroyed()` before destroying members
   (`enginetts.cpp:61–68`); `AnnouncementManager::onTtsSinkDestroyed()`
   (`:860–872`) sets `m_ttsSinkDestroyed`, nulls `m_pTtsSink`, and clears the
   `TtsEngine`'s own sink pointer. Connected at `:367–370`.
2. `CoreServices::finalize()` destroys the manager before the engine
   (`coreservices.cpp:987–991`).

Keep both. The `ControlProxy` observing `[Tts],enabled` is parented to the
manager and can fire during teardown.

## Invariant D — the debounce/dedup state machine is load-bearing

`speak()` clearing `m_lastControlKey` and `slotAnnouncePendingControl()`
restoring it is not incidental bookkeeping — it is what makes rules 1–5 above
work. A rebase that "simplifies" either half produces a knob that either
re-announces its name on every tick, or never announces it at all. Both are
regressions a sighted reviewer will not notice.

`m_lastValueByKey` is intentionally **session-lifetime** and never pruned
(`announcementmanager.h:246–248`). Bounded by the number of physical controls;
do not add an eviction policy.

## Rebase checklist for this spec

1. All 48 observed CO keys still exist after engine bring-up. **No automated
   check covers this yet** (see Invariant A). Pair the rebase with landing
   `src/test/a11ycontrols_test.cpp`.
2. `getParameter()` (not `get()`) still used for `volume`, `pregain`,
   `[Master],gain`, `[Master],headGain`.
3. `speak()` early-return order unchanged; `--tts-log` write still first.
4. `m_lastControlKey` clear/restore dance intact.
5. `emitCue()` remains the sole earcon dispatch point; feedback-mode semantics
   (`0`/`1`/`2`) unchanged.
6. 35 `[Accessibility]` keys present with the documented defaults; especially
   `AnnounceMixer = true`.
7. NATO phonetic letters and "B P M" spacing preserved verbatim.
8. `Library::announceText()` still routes through `quickPickerItemHighlighted`
   and `slotQuickPickerItemHighlighted()` stays **ungated**.
9. `ErrorDialogHandler::errorDialogAnnouncement` still emitted **before**
   `QMessageBox` construction (`errordialoghandler.cpp:180`), still connected
   at `coreservices.cpp:663–666`, HTML still stripped, 300-char cap intact.
10. Both shutdown guards intact.

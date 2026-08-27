# Spec 05 — Speech engine and audio path

**Status:** Verified current state — rebase contract (refreshed against the current PR wave)
**Branch:** `spec-speech` — content re-verified against the merged target-state tree
(`scratch-target-state` @ `1ba6bee542`: the `accessibility-improvements-2026-06-25`
tip plus all 19 other currently-open accessibility PRs merged together, with the
cross-PR bugs that merge surfaced found and fixed). 142 commits landed on top of
the previous verification point, `2390edf423`.
**Owner:** accessibility fork
**Related:** Spec 06 (announcement layer), Spec 07 (spoken menu), Spec 04 (E2E testing)

## Purpose

This spec documents **how a string becomes audible** on the accessibility fork:
the platform synthesizer backends, the lock-free hand-off to the audio thread,
the ducking and routing rules, the three call sites in `EngineMixer::process()`
that make all of it audible, and — new since the last verification — the
lock-free audibility log that finally lets a test tell "requested" apart from
"heard."

It is written as the **contract a rebase onto a newer upstream 2.6 must
preserve**. Every invariant below is stated because breaking it produces
*silence*, not a crash — and for a blind user, silence is indistinguishable
from "the feature was never there".

## Background / current state

The fork does **not** speak through the OS screen reader or a separate audio
device. It synthesizes speech to PCM and mixes it into Mixxx's own engine
output. That single decision drives everything else in this spec, and it has
not changed.

| Component | File | Lines | Role |
|---|---|---|---|
| `TtsEngine` (abstract) | `src/util/ttsengine.h` | 85 | Backend interface: `say()`, `setVoice()`, `setRate()`, `setSink()`, `setSampleRate()`, plus a new `setUtteranceId()` (`:70–72`) so a backend can tag its render with the id the audibility log should attribute it to |
| `SapiTtsEngine` | `src/util/ttsengine.cpp` | 158–456 | Windows SAPI, renders via temp WAV, worker thread |
| `MacTtsEngine` | `src/util/ttsenginemac.mm` | 40–196 | macOS `AVSpeechSynthesizer`, `writeUtterance:toBufferCallback:` |
| `EspeakTtsEngine` | `src/util/ttsengine.cpp` | 457–695 | Linux eSpeak NG, synth callback + worker thread |
| `NullTtsEngine` | `src/util/ttsengine.cpp` | 696–707 | Silent fallback when no backend is compiled in |
| `EngineTts` (sink) | `src/engine/enginetts.{h,cpp}` | 180 / 290 | Lock-free FIFO, sidechain ducking, bus routing, **plus a second lock-free audibility ring (new)** |
| `EngineEarcon` | `src/engine/engineearcon.{h,cpp}` | 83 / 201 | Percussive transport cues, 24-voice pool, **plus a new `Xrun` cue** |
| `EngineBeatClick` | `src/engine/enginebeatclick.{h,cpp}` | 86 / 162 | Per-deck metronome, deck-panned — **byte-for-byte unchanged since `2390edf423`** (`git diff --stat` is empty) |
| `TtsLog` (new) | `src/util/ttslog.{h,cpp}` | 106 / 211 | The implementation behind `--tts-log`, now an audibility-outcome log (REQUESTED → SUPPRESSED / SPOKEN → SUPERSEDED / FLUSHED / COMPLETED) rather than a flat transcript |
| Mix point | `src/engine/enginemixer.cpp` | 817–839 | The three blocks that make all of the above audible |

`TtsEngine::create()` (`ttsengine.cpp:708–718`) still selects the backend by
platform at compile time — `Q_OS_WIN` → SAPI, `Q_OS_MACOS` →
`createMacTtsEngine()`, `MIXXX_USE_ESPEAK` → eSpeak, else `NullTtsEngine`.
`TtsEngine::isAvailable()` (`ttsengine.cpp:720–726`) is the same test, exposed
so the UI can warn rather than fail silently. Both are unchanged in behaviour.

`EngineMixer` still owns all three engine-side objects, constructed in its own
constructor (`enginemixer.cpp:91–93`) with group `[Tts]`, and hands the sink
and earcon player to `AnnouncementManager::create()` at
`coreservices.cpp:657–663`.

## Threading model

The hand-off shape is unchanged from the last verification, with one addition:
the synthesizer worker thread now also feeds a **second**, purely diagnostic
lock-free ring back to the GUI thread (the audibility rings described in
Invariant 6), separate from the audio-callback-facing `FIFO<CSAMPLE>`.

```
GUI thread                   backend worker              audio callback
──────────                   ──────────────              ──────────────
AnnouncementManager::speak()
  └─ dispatchSpeech()
       ├─ ttslog: REQUESTED (always) ──────────────────────► --tts-log file
       ├─ ttslog: SUPPRESSED (disabled/shutdown) → return
       ├─ ttslog: SPOKEN, TtsEngine::setUtteranceId(id)
       └─ TtsEngine::say(text)
            ├─ store pending text
            ├─ ++generation ────────────────┐
            ├─ sink->requestFlush()         │  (atomic bool)
            └─ notify worker ───────────────┼──► synthesize to PCM
                                            │      ├─ resample to engine rate
                                            │      ├─ mono → interleaved stereo
                                            │      ├─ beginUtterance(id)
                                            │      └─ EngineTts::writeSamples()
                                            │           └─ FIFO<CSAMPLE>.write()
                                            │      endUtterance(id) ──► UtteranceSpan ring
                                            └─ generation check   ▼
                                               at every chunk   EngineTts::process()
                                                                  ├─ honour flush
                                                                  ├─ FIFO.read()
                                                                  ├─ noteSamplesConsumed()
                                                                  │    → AudibilityEvent ring
                                                                  ├─ sidechain duck
                                                                  └─ mix into bus
                                                                       │
                                            QTimer (100 ms, GUI thread) ◄┘
                                            pollAudibilityEvents()
                                              → ttslog: COMPLETED / FLUSHED
```

`FIFO<CSAMPLE>` (`src/util/fifo.h`) still wraps PortAudio's `PaUtil_RingBuffer`
— single-producer / single-consumer, lock-free, no allocation on either side
after construction. `EngineTts` sizes it via `kFifoSamples = 48000 * 2 * 10`
(`enginetts.cpp:26`), i.e. ~10 s of stereo speech at 48 kHz, rounded up to the
next power of two by `FIFO`'s constructor. The depth exists so a long
announcement never blocks the (non-realtime) synthesizer thread.

### Invariant 1 — nothing allocates or blocks in `process()`

`EngineTts::process()` (`enginetts.cpp:205–290`), `EngineEarcon::process()`
(`engineearcon.cpp:139–…`) and `EngineBeatClick::process()`
(`enginebeatclick.cpp:70–130`, unchanged) are called from the audio callback.
They may only touch:

- control atomics (`ControlObject::get()` / `forceSet()`, `ControlProxy::get()`),
- the pre-allocated `FIFO` and `mixxx::SampleBuffer m_tts`,
- POD per-voice / per-deck state,
- the two new pre-allocated `FIFO<UtteranceSpan>` / `FIFO<AudibilityEvent>`
  rings (`enginetts.h:157–158`), sized `kAudibilityRingEntries = 64`
  (`enginetts.cpp:31`) — these are the same lock-free primitive as the speech
  FIFO, not a new allocation risk,
- `SampleUtil` on the caller's buffers.

Any rebase change that adds a `QString`, a heap allocation, a mutex, a signal
emission, or an `ITimer`-style wait into these three functions is a
correctness regression even if it "works" on a fast machine. The new
audibility bookkeeping was written to the same discipline: `noteSamplesConsumed()`
(`enginetts.cpp:167–192`, audio-callback-only) does ring writes and integer
arithmetic only; the file write and the string formatting live entirely on the
GUI-thread `QTimer` callback `pollAudibilityEvents()` (`enginetts.cpp:194–203`,
started at `enginetts.cpp:53–58` and gated by `m_logAudibility`, itself gated
by `mixxx::ttslog::isEnabled()` — i.e. this machinery costs nothing at all
unless `--tts-log` is passed).

The corollary constrains the producer side too: the backends deliberately
**spin with a 2 ms sleep** rather than block when the FIFO is full and macOS
**drops the tail** rather than block its buffer callback (`ttsenginemac.mm:143–…`,
now also logging `SUPPRESSED(reason=no-audio)` for that case at `:146–147`).

### Invariant 2 — the three mix blocks in `EngineMixer::process()` are load-bearing

```cpp
// src/engine/enginemixer.cpp:764
if (m_pEngineSideChain) { m_pEngineSideChain->writeSamples(...); }
// ... main gain / balance / mono-mixdown ...
// src/engine/enginemixer.cpp:817
if (m_pTts) { m_pTts->process(...); }
// src/engine/enginemixer.cpp:827
if (m_pBeatClick) { m_pBeatClick->process(...); }
// src/engine/enginemixer.cpp:836
if (m_pEarcon) { m_pEarcon->process(...); }
```

Order and position are unchanged from the last verification: these are the
**only** places accessibility audio enters the output, and they still sit
after the sidechain tap and after main gain/balance — the comment at
`enginemixer.cpp:813–816` states the intent verbatim ("Done after the
sidechain/recording tap so speech is never recorded or broadcast, and after
main gain and balance so it is not attenuated by them").

**Still verified: no test in `src/test/` exercises them through the mixer.**
`enginemixertest.cpp` and `signalpathtest.{h,cpp}` contain zero references to
`Tts`, `Earcon` or `BeatClick`. The unit tests that exist —
`enginetts_test.cpp` (now 595 lines, 25 `TEST_F`, up from 402 lines before
this PR wave), `engineearcon_test.cpp` (12 `TEST_F`, unchanged),
`enginebeatclick_test.cpp` (8 `TEST_F`, unchanged), `ttsengine_integration_test.cpp`
(131 lines, unchanged) — all instantiate the engine objects **directly** and
call `process()` themselves on scratch buffers.

**A new guard test partially closes this gap from a different angle:**
`src/test/a11ymixpath_test.cpp` (309 lines, new) builds a real `EngineMixer`
through `BaseSignalPathTest`/`MockedEngineBackendTest` (decks render silence
via a `MockScaler`) and asserts that speech routed to main, a triggered
earcon, and the beat click all produce non-zero energy at the **mixer's own
output** — not just at the sink's `process()` boundary. See
`SpeechRoutedToMain_ReachesMixerMainOutput` (`:131`),
`TriggeredEarcon_ReachesMixerOutput` (`:206`), `BeatClick_ReachesMixerOutput`
(`:270`), using energy thresholds `kSilent = 1e-6` / `kAudible = 1e-3`
(`:58–59`). This is the first automated defence against a rebase silently
dropping the three call sites above — but it still would not catch, say, a
reordering relative to the sidechain tap, since it only checks presence at the
final output, not position in the pipeline. **Verify audibly and by reading
`enginemixer.cpp:764–839` directly; do not trust the test run alone.**

### Invariant 3 — barge-in via generation counter, plus batched dispatch to avoid false barge-in

Every backend still carries a generation counter and increments it on `say()`:

| Backend | `say()` | `++generation` | Render-side checks |
|---|---|---|---|
| SAPI | `ttsengine.cpp:175` | `:190` | `:258`, `:344`, `:360`, `:374` |
| eSpeak | `ttsengine.cpp:483` | `:498` | `:590`, `:614`, `:636`, `:654` |
| macOS | `ttsenginemac.mm:52` | `:60` | `:127`, `:180` |

The flush itself is still deferred to the audio thread so the FIFO stays
single-reader: `requestFlush()` (`enginetts.h:78–80`) only stores an atomic
bool; `process()` consumes it at `enginetts.cpp:233–239`.

**The "same-call-stack double-`speak()` loses the first utterance" bug
documented in the previous verification is fixed and merged** (it was
"pending, branch `wt-48-tts-queue`" at `2390edf423`; that work has since
landed). `AnnouncementManager` now has `beginSpeechBatch()` /
`endSpeechBatch()` (`announcementmanager.cpp:1145–1147` / `:1149–1171`,
declared `announcementmanager.h:206–207`, doc at `:202–205`: "Nestable; only
the outermost `endSpeechBatch()` actually dispatches"):

```cpp
// speak(), announcementmanager.cpp:1094-1105 (abbreviated)
if (m_speechBatchDepth > 0) {
    m_batchedSpeech << text;
    return;
}
dispatchSpeech(text, ttsLogUtteranceId);

// endSpeechBatch(), announcementmanager.cpp:1149-1171 (abbreviated)
if (--m_speechBatchDepth > 0) return;
const QString combined = m_batchedSpeech.join(QStringLiteral(". "));
m_batchedSpeech.clear();
dispatchSpeech(combined, mixxx::ttslog::logRequested(combined));
```

`m_speechBatchDepth` (int, `announcementmanager.h:339`) and `m_batchedSpeech`
(`QStringList`, `:340`) are the whole mechanism. Batching does **not** bypass
the generation counter above — it ensures only **one** `TtsEngine::say()` call
happens per logical event, so the intermediate `speak()` calls never reach
`dispatchSpeech()`/`say()` at all while nested. The generation counter remains
the mechanism for genuinely distinct, successive events. Real call sites:
Smart Cue's track-load-then-pfl cascade (`announcementmanager.cpp:2092/2127`,
the original issue #48 bug), "Mixxx ready" plus the new first-run orientation
speech both firing on the very first `slotSoundDevicesReady()`
(`:2179/2185` — see Spec 06 for `maybeSpeakFirstRunOrientation()`), and the
per-control debounce flush joining multiple settled controls into one
utterance (`:2787/2840` — see Spec 06, issue #114).

`AccessMenuController` (Spec 07) independently applies the same fix at a
smaller scale, by folding what used to be two sequential `speak()` calls into
one formatted string per transition (e.g. `"Main menu. %1"` at
`accessmenucontroller.cpp:309–310`) rather than calling the general batching
API — the two mechanisms solve the same problem at different layers and both
must be preserved.

### Invariant 4 — routing, with a main-output fallback

`EngineTts::process()` still reads `[Tts],route_to_main` and branches
(`enginetts.cpp:263–288`):

| `route_to_main` | Ducks and mixes into | Fallback | Rationale |
|---|---|---|---|
| `0` (`Route::Headphones`, default) | headphone (cue) bus | **main, when `pHead == nullptr`** | DJ-only; audience never hears it |
| `1` (`Route::Main`) | main **and** head | — | audience hears it; head folds the same so the DJ monitors what goes out |

The fallback is one line — `enginetts.cpp:283`:

```cpp
CSAMPLE* pOut = pHead ? pHead : pMain;
```

**A single-output install must never be silent.** This is the first-run case:
one sound card, no headphone output configured, `pHead == nullptr`. Without
the fallback, the default route (headphones) drops every announcement on the
floor and the user has no way to discover why. Unchanged since the last
verification.

`EngineBeatClick` (`enginebeatclick.cpp:76–78`, unchanged) and `EngineEarcon`
(`engineearcon.cpp:143–145`) apply the **same** rule with the **same**
fallback, and deliberately follow `[Tts],route_to_main` rather than
hard-wiring head-else-main — the comment at `enginebeatclick.h:21` and
`:31–40` records why.

Both read `[Tts],route_to_main` through a `ControlProxy` with
`ControlFlag::AllowMissingOrInvalid` (`enginebeatclick.cpp:50–53`,
`engineearcon.cpp:105–107`) — see the risk note in Spec 06.

### Invariant 5 — ducking parameters

`EngineTts` still uses `EngineSideChainCompressor` with the speech as the key
signal (`enginetts.cpp:255`). Parameters (`enginetts.cpp:17–21`, `:115–119`):

| Parameter | Value | Source |
|---|---|---|
| Threshold | `0.1f` | `kDuckThreshold`, `enginetts.cpp:17` — matches `EngineTalkoverDucking` |
| Strength | `[Tts],duckStrength`, default `0.5` | `kDefaultDuckStrength`, `enginetts.cpp:21` — gentler than mic talkover (0.9) so music stays present |
| Attack | `sampleRate / 2 * 0.1` | `enginetts.cpp:118` |
| Decay | `sampleRate / 2` | `enginetts.cpp:119` |

Recomputed only when the sample rate or strength changes
(`enginetts.cpp:210–214`). When idle *and* fully recovered, `process()`
returns early without touching the output buffers at all
(`enginetts.cpp:256–260`) — the zero-cost idle path. All unchanged.

### Invariant 6 — the audibility log distinguishes intent from delivery (new)

This is the single biggest addition to this spec since the last verification.
`src/util/ttslog.{h,cpp}` (new files) is now the **entire implementation**
behind the pre-existing `--tts-log PATH` flag — the flag's declaration and help
text are unchanged (`src/util/cmdlineargs.cpp:396–402`,
`getTtsLogPath()` at `cmdlineargs.h:51–53`/`:121`) but what it writes changed
completely. The old behaviour — append every spoken *string*, with no
indication of whether it was ever audible — is exactly what `ttslog.h:8–14`
now documents as **superseded**, and names as the root cause of the Smart Cue
barge-in bug hiding for weeks (Invariant 3, previous verification).

Public API, namespace `mixxx::ttslog` (`ttslog.h:60–106`):

| Function | Line | Called from |
|---|---|---|
| `logRequested(text) -> id` | `:84` | `AnnouncementManager::speak()` (`announcementmanager.cpp:1074`) and `endSpeechBatch()` (`:1169`) — every utterance, batched or not, gets one |
| `logSuppressed(id, text, reason)` | `:88` | `speak()` early returns: TTS disabled (`:1079–1080`, `kReasonTtsDisabled`), sink destroyed (`:1089–1090`, `kReasonSinkDestroyed`) |
| `logSpoken(id, text)` | `:91` | `dispatchSpeech()`, immediately before `TtsEngine::say()` (`announcementmanager.cpp:1140–1142`) |
| `logSuperseded(id, stage)` | `:94` | Backend generation-counter barge-in: `kStageQueued` when a new `say()` drops one still waiting in the queue, `kStageRender` when it interrupts one already rendering |
| `logSuppressedById(id, reason)` | `:98` | Backend "no audio produced" cases (`kReasonNoAudio`) and `EngineTts::endUtterance()` when a bracketed utterance wrote zero samples (`enginetts.cpp:159`) |
| `logFlushed(id)` | `:102` | `EngineTts::pollAudibilityEvents()` when a span was discarded before finishing (`enginetts.cpp:200`) |
| `logCompleted(id)` | `:103` | Same, when a span drained fully (`enginetts.cpp:198`) |

Record vocabulary (literal strings written by `TtsLog::write()`,
`ttslog.cpp:140–155`, wire format documented at `ttslog.h:21–34`): **REQUESTED**,
**SUPPRESSED** (`reason=`), **SPOKEN**, **SUPERSEDED** (`stage=`), **FLUSHED**,
**COMPLETED**. `logRequested`/`logSuppressed`/`logSpoken` take the id-bearing
call directly; `logSuppressedById`/`logSuperseded`/`logFlushed`/`logCompleted`
recover the text from a small remembered-id ring keyed by id, since the
audio-thread-adjacent callers (`EngineTts`, the backends) don't carry the
original `QString` around.

**How an utterance is threaded through, end to end:** `AnnouncementManager::speak()`
calls `logRequested()` to get an id, then (assuming it isn't suppressed)
`dispatchSpeech()` calls `logSpoken(id, text)` and
`TtsEngine::setUtteranceId(id)` (`announcementmanager.cpp:1141`) before
`say(text)`. The backend brackets its render with `beginUtterance(id)` /
`endUtterance(id)` on `EngineTts` (e.g. SAPI at `ttsengine.cpp:353`/`:389`),
which records where in the FIFO write-stream that utterance's samples start
and end. `EngineTts::noteSamplesConsumed()` (audio-callback thread,
`enginetts.cpp:167–192`) retires spans as they're read or discarded and
queues a completed/flushed outcome; the GUI-thread `pollAudibilityEvents()`
timer drains that queue into the log every 100 ms
(`kAudibilityPollMs`, `enginetts.cpp:36`).

**Practical effect on the test strategy described in Spec 04:** a `--tts-log`
assertion can now distinguish "the code decided to say this" (REQUESTED) from
"a human plausibly heard it" (COMPLETED), with SUPPRESSED / SUPERSEDED /
FLUSHED accounting for every way the two can diverge. COMPLETED means audio
reached the mix buffers — it is the strongest signal available short of
capturing the physical device output, not literal proof of perception. See
Spec 06's Invariant B for the announcement-layer side of this same story.
`src/test/ttslog_test.cpp` (192 lines, 12 `TEST_F`) pins the format and all
six outcomes directly against the `TtsLog` class, independent of any engine
object.

**Rebase note:** do not "simplify" this into a single flat log again. The
REQUESTED/outcome split is the fix for a real, previously-undetectable bug
class, not incidental complexity.

## Control objects owned by the engine layer

| Control | Type | Persist | Default | Owner | Meaning |
|---|---|---|---|---|---|
| `[Tts],enabled` | `ControlPushButton`, Toggle mode | no | `0` | `EngineTts` (`enginetts.cpp:66–69`) | User on/off. Toggle mode is required: the keyboard filter sends press=1/release=0, so a plain control would be momentary |
| `[Tts],speaking` | `ControlObject` (read-only) | no | `0.0` | `EngineTts` (`:73–74`) | `1.0` while the FIFO has data. Written **only** by `process()` |
| `[Tts],route_to_main` | `ControlObject` | **yes** | `0` | `EngineTts` (`:76–77`) | `0` = headphones, `1` = main |
| `[Tts],duckStrength` | `ControlObject` | **yes** | `0.5` | `EngineTts` (`:79–80`) | Sidechain strength |
| `[Earcon],volume` | `ControlPotmeter` | **yes** | `0.6` | `EngineEarcon` | Earcon level |
| `[BeatClick],enabled` | `ControlPushButton`, Toggle mode | no | `0` | `EngineBeatClick` (`enginebeatclick.cpp:32`, unchanged) | Metronome on/off (keyboard `Alt+B`) |
| `[BeatClick],volume` | `ControlPotmeter` | **yes** | `0.75` | `EngineBeatClick` (`:36`, unchanged) | Click level |

`[Tts],repeat`, `[Tts],shift`, `[Tts],pad_mode` and the per-deck `tts_*`
readouts are owned by `AnnouncementManager`, not the engine — see Spec 06.

Keyboard bindings (`res/keyboard/en_US.kbd.cfg:21–26`) — **unchanged since the
last verification**, and confirmed identical across all 12 shipped locale
files by `src/test/keyboardbindings_test.cpp` (new, see the Rebase checklist):

```
[BeatClick]
enabled Alt+b

[Tts]
enabled Alt+Shift+a
repeat Alt+Shift+r
```

## Earcon vocabulary

`EngineEarcon` synthesizes short sine "grains" — no samples, no file I/O
(`engineearcon.cpp:25–43`, unchanged). Pitches sit in a 350–1300 Hz niche that
cuts through a mix without ducking it. Direction encodes meaning: rising =
engage, falling = disengage.

| `EngineEarcon::Id` | Grains (Hz @ ms, dur) | Gesture |
|---|---|---|
| `Play` | 587 @ 0, 55 · 880 @ 50, 65 | rising pair |
| `Stop` | 880 @ 0, 55 · 587 @ 50, 70 | falling pair |
| `EndOfTrack` | 1175 ×3 @ 0/70/140 | three urgent pips |
| `CueOn` | 784 @ 0, 60 | single |
| `CueOff` | 523 @ 0, 70 | single |
| `Restart` | 659 ×2 @ 0/45, 35 | "tuk-tuk" double-tap |
| `LoopOn` | 698 @ 0, 55 · 1047 @ 50, 65 | rising pair |
| `LoopOff` | 1047 @ 0, 55 · 698 @ 50, 70 | falling pair |
| `Clipping` | 350 ×2 @ 0/70, 60 | low urgent double-buzz — deliberately outside the mid-high register of every other gesture |
| `CuePreview` | 988 @ 0, 40 | one very short high tick |
| `Xrun` (**new**) | 196 ×3 @ 0/110/220, 90 | low, harsh triple-buzz — lower and longer than `Clipping` so an audio dropout can never be mistaken for "turn the gain down"; this is an engine-level glitch cue, not a mixing note (`engineearcon.cpp:44–47`) |

Panning: `Pan::Left` = deck 1, `Pan::Right` = deck 2, `Pan::Center` = whole
mix. This matches the beat-click geography (deck 1 left, deck 2 right) and
split-cue, so **the ear a sound arrives from identifies the deck for free**.
Preserve the pan convention across the rebase.

Trigger path is still a `FIFO<Trigger>` of depth 64; `trigger()` is
GUI-thread single-writer and drops on overflow rather than blocking.
Unchanged.

## Shutdown ordering

`~EngineTts()` (`enginetts.cpp:87–101`) still emits `sinkDestroyed()` **before**
any member is destroyed — now with one addition: it first flushes any
audibility outcomes the poll timer hasn't picked up yet
(`enginetts.cpp:91–93`), so a shutdown immediately after the last announcement
doesn't lose its COMPLETED record. `AnnouncementManager` connects to
`sinkDestroyed()` and drops its raw sink pointer, which also clears the
`TtsEngine`'s own sink pointer — see Spec 06, Invariant C for the current line
numbers on that side.

`CoreServices::finalize()` additionally resets the manager *before* the engine
(`coreservices.cpp:997–1001`). Both guards exist because of the same real
crash (issue #30): the `ControlProxy` observing `[Tts],enabled` can fire during
shutdown and call `speak()` on a torn-down sink.

**Rebase note:** these are two independent guards for one hazard. Keep both.

## Known architectural limitation — nothing can be spoken before a sound device is open

This is the fork's largest user-visible design constraint, and it remains a
*consequence* of the "mix into the engine" decision, not a bug that can be
patched locally. It is **still true**.

`MixxxMainWindow::initialize()` records the constraint at
`mixxxmainwindow.cpp:563–575` (comment moved down the file as unrelated code
was inserted above it, wording essentially unchanged from the previous
verification):

> Accessibility note (chicken-and-egg, issue #49): on this branch TTS is mixed
> into Mixxx's own engine output, so speech is only audible once a sound device
> is open and the engine is running. […] "Mixxx ready" (spoken from
> `loadConfiguredSkin()`, which runs before this loop) does not have that
> problem: `AnnouncementManager` queues it and only speaks it once
> `SoundManager::devicesSetup()` confirms a device actually opened […]

Sequence, in program order:

| Step | `mixxxmainwindow.cpp` | Speech audible? |
|---|---|---|
| `AccessMenuController` constructed + its always-on keyboard chords wired (issue #57, see Spec 07) | 467–539 | n/a |
| Skin loaded → `emit skinLoaded()` → `slotSkinLoaded()` | 545 / `announcementmanager.cpp` | **queued** (see below) |
| `checkDirectRendering()` → `announceText(directRenderingSpeech())` | 556 / 1899–1900 | **no** — see the discrepancy note below |
| `setupDevices()` retry loop → `soundDeviceBusyDlg` / `soundDeviceErrorMsgDlg` | 576–591 | **no** |
| No-output loop → `noOutputDlg()` | 597–607 | **no** |
| Device open → `SoundManager::devicesSetup()` → `slotSoundDevicesReady()` | — / `coreservices.cpp:684–687` | **yes — queued "Mixxx ready" (and, on first run, the new orientation speech — see Spec 06) flush here** |

**Discrepancy found and worth flagging:** the comment immediately above the
`checkDirectRendering()` speech call reads "Accessibility: the engine is up
here, so this is audible" (`mixxxmainwindow.cpp:1899`). This is **not
consistent with the actual boot order** — `checkDirectRendering()` is called
at `mixxxmainwindow.cpp:556`, strictly *before* the `setupDevices()` retry
loop at `:576–591`, so no sound device is open yet and (per Invariant 2 of
this spec) nothing is pulling from `EngineTts`'s FIFO at that point. The table
above — and the old spec's table, which predates this comment being added —
both correctly list this announcement as inaudible. Treat the code comment as
stale/incorrect, not the architecture; do not let a rebase "fix" the boot
order to match the comment, and consider correcting the comment itself as a
small, independent cleanup.

### Deferred startup announcement (issue #49, unchanged) plus first-run orientation (issue #105, new)

`AnnouncementManager` still carries the same two booleans gating "Mixxx
ready" (`m_audioEngineReady`, `m_pendingReadyAnnouncement` —
see Spec 06 for current line numbers), and `slotSoundDevicesReady()` still
sets `m_audioEngineReady` and flushes the queued announcement on first firing
only, wired at `coreservices.cpp:684–687`. `devicesSetup()` still fires only
on the success path of `setupDevices()`.

New on top of that: `slotSoundDevicesReady()` now also calls
`AnnouncementManager::maybeSpeakFirstRunOrientation()` in the same batched
speech block (`beginSpeechBatch()`/`endSpeechBatch()` at
`announcementmanager.cpp:2179`/`:2185`), so the very first successful device
open can produce two logical announcements ("Mixxx ready" and a one-time
orientation script) without one barging in on the other. This is Spec 06's
content in detail (issue #105); the point for this spec is that it rides
entirely on the pre-existing `devicesSetup()` → `slotSoundDevicesReady()`
plumbing described above — no new connection was added to
`mixxxmainwindow.cpp` or `coreservices.cpp` for it.

Two failure modes this closes, both real, both still closed:

- Writing "Mixxx ready" into the FIFO with nothing draining it.
- Barge-in (Invariant 3) discarding it the moment any boot dialog spoke next.

### What is still silent at first run

- Every boot-time failure dialog is **written to the `--tts-log`** (as a
  REQUESTED/SUPPRESSED pair — see Invariant 6) but is **not audible**. A blind
  user hitting a sound-device error still gets a dialog they cannot hear and
  cannot see.
- The normal path is survivable because `SoundManagerConfig::loadDefaults()`
  auto-configures the system default output, so the loops at `:576` and `:597`
  are skipped, `devicesSetup()` fires, and "Mixxx ready" (plus, on first run,
  the orientation script) is the first thing spoken.
- The dialogs' *accessible names* are still set, so an OS screen reader
  (VoiceOver / NVDA / Orca) running alongside Mixxx can read them. That remains
  the only mitigation for the pre-device window.

**Rebase note:** do not let a rebase reorder the boot sequence on the
assumption that speech works before `setupDevices()`. It does not, and the
queueing mechanism is the only thing standing between a blind user and a
silent launch.

## Rebase checklist for this spec

1. `EngineMixer::process()` still calls `m_pTts`, `m_pBeatClick`, `m_pEarcon`
   — in that order, after the sidechain tap at `:764`, before the delay
   processing. **Verify audibly**; `a11ymixpath_test.cpp` now provides partial
   automated coverage but does not check relative ordering.
2. `EngineTts::process()` head→main fallback (`enginetts.cpp:283`) intact.
3. `EngineBeatClick` and `EngineEarcon` still follow `[Tts],route_to_main`
   with the same fallback.
4. `EngineBeatClick::addDeck()` is still called from
   `EngineMixer::addChannel()`, **not** the constructor — see the ordering
   rationale in `enginebeatclick.h:31–40`.
5. Generation-counter barge-in intact in all three backends (table in
   Invariant 3); `beginSpeechBatch()`/`endSpeechBatch()` intact in
   `AnnouncementManager` (now load-bearing production code, not a pending
   branch).
6. `sinkDestroyed()` signal and `CoreServices` teardown order both intact
   (`coreservices.cpp:997–1001`).
7. No allocation, lock, or blocking wait introduced into any `process()`,
   including the new audibility-ring bookkeeping.
8. `[Tts],enabled` and `[BeatClick],enabled` still `ButtonMode::Toggle`.
9. `SoundManager::devicesSetup()` → `slotSoundDevicesReady()` connection intact
   (`coreservices.cpp:684–687`), and `m_audioEngineReady` /
   `m_pendingReadyAnnouncement` survive. Upstream renaming or removing
   `devicesSetup()` silently reverts issue #49 **and** issue #105 (the
   orientation speech rides the same signal).
10. `--tts-log` still resolves through `mixxx::ttslog` (`ttslog.h`/`.cpp`),
    not a flat per-string transcript. A rebase that reintroduces a simple
    "log every spoken string" hook reintroduces the exact bug class that hid
    the Smart Cue barge-in for weeks.
11. `EngineTts::beginUtterance()`/`endUtterance()` still bracket every
    backend's render, and `TtsEngine::setUtteranceId()` is still called from
    `AnnouncementManager::dispatchSpeech()` before `say()`
    (`announcementmanager.cpp:1140–1142`) — otherwise COMPLETED/FLUSHED
    records silently stop being produced while REQUESTED/SPOKEN keep working,
    which is itself a silent regression of the exact kind this spec exists to
    prevent.

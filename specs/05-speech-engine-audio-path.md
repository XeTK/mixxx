# Spec 05 — Speech engine and audio path

**Status:** Verified current state — rebase contract
**Branch:** `spec-speech` (based on `accessibility-improvements-2026-06-25` @ `2390edf423`)
**Owner:** accessibility fork
**Related:** Spec 06 (announcement layer), Spec 07 (spoken menu), Spec 04 (E2E testing)

## Purpose

This spec documents **how a string becomes audible** on the accessibility fork:
the platform synthesizer backends, the lock-free hand-off to the audio thread,
the ducking and routing rules, and the three call sites in
`EngineMixer::process()` that make all of it audible.

It is written as the **contract a rebase onto a newer upstream 2.6 must
preserve**. Every invariant below is stated because breaking it produces
*silence*, not a crash — and for a blind user, silence is indistinguishable
from "the feature was never there".

## Background / current state (verified on this branch)

The fork does **not** speak through the OS screen reader or a separate audio
device. It synthesizes speech to PCM and mixes it into Mixxx's own engine
output. That single decision drives everything else in this spec.

| Component | File | Lines | Role |
|---|---|---|---|
| `TtsEngine` (abstract) | `src/util/ttsengine.h` | 73 | Backend interface: `say()`, `setVoice()`, `setRate()`, `setSink()`, `setSampleRate()` |
| `SapiTtsEngine` | `src/util/ttsengine.cpp` | 157–380 | Windows SAPI, renders via temp WAV, worker thread |
| `MacTtsEngine` | `src/util/ttsenginemac.mm` | 28–155 | macOS `AVSpeechSynthesizer`, `writeUtterance:toBufferCallback:` |
| `EspeakTtsEngine` | `src/util/ttsengine.cpp` | 433–648 | Linux eSpeak NG, synth callback + worker thread |
| `NullTtsEngine` | `src/util/ttsengine.cpp` | 652–656 | Silent fallback when no backend is compiled in |
| `EngineTts` (sink) | `src/engine/enginetts.{h,cpp}` | 103 / 172 | Lock-free FIFO, sidechain ducking, bus routing |
| `EngineEarcon` | `src/engine/engineearcon.{h,cpp}` | 82 / 195 | Percussive transport cues, 24-voice pool |
| `EngineBeatClick` | `src/engine/enginebeatclick.{h,cpp}` | 86 / 162 | Per-deck metronome, deck-panned |
| Mix point | `src/engine/enginemixer.cpp` | 813–841 | The three blocks that make all of the above audible |

`TtsEngine::create()` (`ttsengine.cpp:660–670`) selects the backend by
platform at compile time — `Q_OS_WIN` → SAPI, `Q_OS_MACOS` →
`createMacTtsEngine()`, `MIXXX_USE_ESPEAK` → eSpeak, else `NullTtsEngine`.
`TtsEngine::isAvailable()` (`ttsengine.cpp:672–678`) is the same test,
exposed so the UI can warn rather than fail silently.

`EngineMixer` owns all three engine-side objects, constructed in its own
constructor (`enginemixer.cpp:91–93`) with group `[Tts]`, and hands the sink
and earcon player to `AnnouncementManager::create()` at
`coreservices.cpp:647–653`.

## Threading model

```
GUI thread                   backend worker              audio callback
──────────                   ──────────────              ──────────────
AnnouncementManager::speak()
  └─ TtsEngine::say(text)
       ├─ store pending text
       ├─ ++generation ────────────────┐
       ├─ sink->requestFlush()         │  (atomic bool)
       └─ notify worker ───────────────┼──► synthesize to PCM
                                       │      │
                                       │      ├─ resample to engine rate
                                       │      ├─ mono → interleaved stereo
                                       │      └─ EngineTts::writeSamples()
                                       │           └─ FIFO<CSAMPLE>.write()
                                       │                     │
                                       └─ generation check   ▼
                                          at every chunk   EngineTts::process()
                                                             ├─ honour flush
                                                             ├─ FIFO.read()
                                                             ├─ sidechain duck
                                                             └─ mix into bus
```

`FIFO<CSAMPLE>` (`src/util/fifo.h`) wraps PortAudio's `PaUtil_RingBuffer` —
single-producer / single-consumer, lock-free, no allocation on either side
after construction. `EngineTts` sizes it at `48000 * 2 * 10` samples
(`enginetts.cpp:25`), i.e. ~10 s of stereo speech at 48 kHz, rounded up to the
next power of two by `FIFO`'s constructor. The depth exists so a long
announcement never blocks the (non-realtime) synthesizer thread.

### Invariant 1 — nothing allocates or blocks in `process()`

`EngineTts::process()` (`enginetts.cpp:98–172`), `EngineEarcon::process()`
(`engineearcon.cpp:133–195`) and `EngineBeatClick::process()`
(`enginebeatclick.cpp:70–130`) are called from the audio callback. They may
only touch:

- control atomics (`ControlObject::get()` / `forceSet()`, `ControlProxy::get()`),
- the pre-allocated `FIFO` and `mixxx::SampleBuffer m_tts`,
- POD per-voice / per-deck state,
- `SampleUtil` on the caller's buffers.

Any rebase change that adds a `QString`, a heap allocation, a mutex, a signal
emission, or an `ITimer`-style wait into these three functions is a
correctness regression even if it "works" on a fast machine.

The corollary constrains the producer side too: the backends deliberately
**spin with a 2 ms sleep** rather than block when the FIFO is full
(`ttsengine.cpp:359`, `ttsengine.cpp:619`) and macOS **drops the tail**
rather than block its buffer callback (`ttsenginemac.mm:143–145`).

### Invariant 2 — the three mix blocks in `EngineMixer::process()` are load-bearing

```cpp
// src/engine/enginemixer.cpp:817
if (m_pTts) { m_pTts->process(...); }
// src/engine/enginemixer.cpp:827
if (m_pBeatClick) { m_pBeatClick->process(...); }
// src/engine/enginemixer.cpp:836
if (m_pEarcon) { m_pEarcon->process(...); }
```

These are the **only** places accessibility audio enters the output. They sit
between `m_pMainMonoMixdown` (`enginemixer.cpp:809`) and the main/head/booth
delay processing (`enginemixer.cpp:843–853`).

**Verified**: no test in `src/test/` exercises them. `enginemixertest.cpp` and
`signalpathtest.h` contain zero references to `Tts`, `Earcon` or `BeatClick`.
The unit tests that exist — `enginetts_test.cpp` (19 cases),
`engineearcon_test.cpp` (11), `enginebeatclick_test.cpp` (7),
`ttsengine_integration_test.cpp` (2) — all instantiate the engine objects
**directly** and call `process()` themselves.

Consequence, stated plainly: **a rebase that drops these three blocks silences
every accessibility sound while the entire DSP and accessibility test suite
still passes green.** `EngineMixer::process()` is the single highest-risk
merge conflict site in the fork. Re-apply these blocks by hand and verify
audibly; do not trust the test run.

### Invariant 3 — barge-in via generation counter

Every backend carries `std::atomic<long> m_generation`. `say()` increments it
and calls `EngineTts::requestFlush()`:

| Backend | `say()` | `++generation` | Render-side checks |
|---|---|---|---|
| SAPI | `ttsengine.cpp:174` | `:182` | `:246`, `:329`, `:340`, `:353` |
| eSpeak | `ttsengine.cpp:459` | `:467` | `:557`, `:579`, `:596`, `:613` |
| macOS | `ttsenginemac.mm:40` | `:48` | `:101`, `:139` |

The design intent is documented at `ttsengine.cpp:418–420`:

> A dedicated worker thread (mirroring `SapiTtsEngine`) serializes synthesis
> and pushes PCM into the sink in chunks, supporting barge-in via a generation
> counter.

The flush itself is deferred to the audio thread so the FIFO stays
single-reader: `requestFlush()` (`enginetts.h:77–79`) only stores an atomic
bool; `process()` consumes it at `enginetts.cpp:122–124`.

**Known consequence — same-call-stack double-`speak()` loses the first
utterance.** Because `say()` supersedes unconditionally, two `speak()` calls
in the same synchronous call stack produce only the second utterance. This is
a real bug, not a theoretical one: the Smart Cue feature
(`announcementmanager.cpp:1637–1658`) writes `[ChannelN],pfl` immediately
after the track-load announcement, and the `pfl` observer
(`announcementmanager.cpp:1085`) speaks "headphone cue on" before the load
announcement has rendered a single sample. The load announcement is discarded.

> **Status: pending — branch `wt-48-tts-queue`.** The fix introduces
> `AnnouncementManager::beginSpeechBatch()` / `endSpeechBatch()` plus a
> `dispatchSpeech()` split, deferring dispatch of every `speak()` between the
> markers and joining them into one utterance. Nestable; only the outermost
> `endSpeechBatch()` dispatches. **These methods do not exist on
> `accessibility-improvements-2026-06-25`.** Verified: `grep -rn
> "beginSpeechBatch" src/` returns nothing on this branch.
>
> If the rebase lands before `wt-48-tts-queue` merges, the batching mechanism
> must be re-derived; it is not part of the current contract.

### Invariant 4 — routing, with a main-output fallback

`EngineTts::process()` reads `[Tts],route_to_main` and branches
(`enginetts.cpp:145–170`):

| `route_to_main` | Ducks and mixes into | Fallback | Rationale |
|---|---|---|---|
| `0` (`Route::Headphones`, default) | headphone (cue) bus | **main, when `pHead == nullptr`** | DJ-only; audience never hears it |
| `1` (`Route::Main`) | main **and** head | — | audience hears it; head folds the same so the DJ monitors what goes out |

The fallback is one line — `enginetts.cpp:165`:

```cpp
CSAMPLE* pOut = pHead ? pHead : pMain;
```

**A single-output install must never be silent.** This is the first-run case:
one sound card, no headphone output configured, `pHead == nullptr`. Without
the fallback, the default route (headphones) drops every announcement on the
floor and the user has no way to discover why.

`EngineBeatClick` (`enginebeatclick.cpp:76–78`) and `EngineEarcon`
(`engineearcon.cpp:137–139`) apply the **same** rule with the **same**
fallback, and deliberately follow `[Tts],route_to_main` rather than
hard-wiring head-else-main — the comment at `enginebeatclick.cpp:71–75`
records why: the earlier hard-wired version let a configured-but-unmonitored
headphone bus swallow the clicks while speech stayed audible on main.

Both read `[Tts],route_to_main` through a `ControlProxy` with
`ControlFlag::AllowMissingOrInvalid` (`enginebeatclick.cpp:50–53`,
`engineearcon.cpp:98–101`) — see the risk note in Spec 06.

### Invariant 5 — ducking parameters

`EngineTts` uses `EngineSideChainCompressor` with the speech as the key
signal (`enginetts.cpp:137`). Parameters (`enginetts.cpp:78–87`):

| Parameter | Value | Source |
|---|---|---|
| Threshold | `0.1f` | `kDuckThreshold`, `enginetts.cpp:16` — matches `EngineTalkoverDucking` |
| Strength | `[Tts],duckStrength`, default `0.5` | `kDefaultDuckStrength`, `enginetts.cpp:20` — gentler than mic talkover (0.9) so music stays present |
| Attack | `sampleRate / 2 * 0.1` | `enginetts.cpp:85` |
| Decay | `sampleRate / 2` | `enginetts.cpp:86` |

Recomputed only when the sample rate or strength changes
(`enginetts.cpp:101–107`). When idle *and* fully recovered, `process()` returns
early without touching the output buffers at all
(`enginetts.cpp:138–142`) — the zero-cost idle path.

### Invariant 6 — accessibility audio is never recorded or broadcast

The sidechain tap runs at `enginemixer.cpp:763–765`:

```cpp
if (m_pEngineSideChain) {
    m_pEngineSideChain->writeSamples(m_sidechainMix.data(), iFrames);
}
```

All three accessibility `process()` calls happen **after** it (lines 817, 827,
836) — and after main gain and balance, so speech is not attenuated by them.
The comment at `enginemixer.cpp:813–816` states the intent.

Moving any of the three blocks above line 765 would put spoken announcements
into the user's recordings and their live broadcast. This is a
user-facing-severity ordering constraint, not a stylistic one.

## Control objects owned by the engine layer

| Control | Type | Persist | Default | Owner | Meaning |
|---|---|---|---|---|---|
| `[Tts],enabled` | `ControlPushButton`, Toggle mode | no | `0` | `EngineTts` (`enginetts.cpp:40–43`) | User on/off. Toggle mode is required: the keyboard filter sends press=1/release=0, so a plain control would be momentary |
| `[Tts],speaking` | `ControlObject` (read-only) | no | `0.0` | `EngineTts` (`:47`) | `1.0` while the FIFO has data. Written **only** by `process()` |
| `[Tts],route_to_main` | `ControlObject` | **yes** | `0` | `EngineTts` (`:50`) | `0` = headphones, `1` = main |
| `[Tts],duckStrength` | `ControlObject` | **yes** | `0.5` | `EngineTts` (`:53`) | Sidechain strength |
| `[Earcon],volume` | `ControlPotmeter` | **yes** | `0.6` | `EngineEarcon` (`engineearcon.cpp:84`) | Earcon level |
| `[BeatClick],enabled` | `ControlPushButton`, Toggle mode | no | `0` | `EngineBeatClick` (`enginebeatclick.cpp:32`) | Metronome on/off (keyboard `Alt+B`) |
| `[BeatClick],volume` | `ControlPotmeter` | **yes** | `0.75` | `EngineBeatClick` (`:36`) | Click level |

`[Tts],repeat`, `[Tts],shift`, `[Tts],pad_mode` and the per-deck `tts_*`
readouts are owned by `AnnouncementManager`, not the engine — see Spec 06.

Keyboard bindings (`res/keyboard/en_US.kbd.cfg:19–24`):

```
[BeatClick]
enabled Alt+b

[Tts]
enabled Alt+Shift+a
repeat Alt+Shift+r
```

## Earcon vocabulary

`EngineEarcon` synthesizes short sine "grains" — no samples, no file I/O
(`engineearcon.cpp:25–43`). Pitches sit in a 350–1300 Hz niche that cuts
through a mix without ducking it. Direction encodes meaning: rising =
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

Panning: `Pan::Left` = deck 1, `Pan::Right` = deck 2, `Pan::Center` = whole
mix. This matches the beat-click geography (deck 1 left, deck 2 right,
`enginemixer.cpp:1005–1010`) and split-cue, so **the ear a sound arrives from
identifies the deck for free**. Preserve the pan convention across the rebase.

Trigger path is a `FIFO<Trigger>` of depth 64 (`engineearcon.cpp:78`);
`trigger()` is GUI-thread single-writer and drops on overflow rather than
blocking (`engineearcon.cpp:106–111`).

## Shutdown ordering

`~EngineTts()` (`enginetts.cpp:61–68`) emits `sinkDestroyed()` **before** any
member is destroyed. `AnnouncementManager` connects to it
(`announcementmanager.cpp:367–370`) and drops its raw sink pointer in
`onTtsSinkDestroyed()` (`announcementmanager.cpp:860–872`), which also clears
the `TtsEngine`'s own sink pointer.

`CoreServices::finalize()` additionally resets the manager *before* the engine
(`coreservices.cpp:987–991`). Both guards exist because of the same real crash
(issue #30): the `ControlProxy` observing `[Tts],enabled` can fire during
shutdown and call `speak()` on a torn-down sink.

**Rebase note:** these are two independent guards for one hazard. Keep both.

## Known architectural limitation — nothing can be spoken before a sound device is open

This is the fork's largest user-visible design constraint, and it is a
*consequence* of the "mix into the engine" decision, not a bug that can be
patched locally. It is **still true** on this branch. What has changed
(issue #49, merged) is that the one announcement it used to break — "Mixxx
ready" — is now queued instead of lost.

`MixxxMainWindow::initialize()` records the constraint at
`mixxxmainwindow.cpp:483–495`:

> Accessibility note (chicken-and-egg, issue #49): on this branch TTS is mixed
> into Mixxx's own engine output, so speech is only audible once a sound device
> is open and the engine is running. At this point in boot no device is open
> yet, so the failure dialogs below are spoken as a best-effort only […] there
> is no engine output to render them into audibly, and this loop is the only
> thing that can bring one up.

Sequence, in program order:

| Step | `mixxxmainwindow.cpp` | Speech audible? |
|---|---|---|
| `AccessMenuController` constructed | 447–459 | n/a |
| Skin loaded → `emit skinLoaded()` → `slotSkinLoaded()` | 465 / 1680 | **queued** (see below) |
| `checkDirectRendering()` → `announceText(directRenderingSpeech())` | 480 / 1812 | **no** |
| `setupDevices()` retry loop → `soundDeviceBusyDlg` / `soundDeviceErrorMsgDlg` | 497–511 (speech at 737, 816, 848) | **no** |
| No-output loop → `noOutputDlg()` → `announceText(noOutputSpeech())` | 517–527 / 880 | **no** |
| Device open → `SoundManager::devicesSetup()` → `slotSoundDevicesReady()` | — / `announcementmanager.cpp:1693` | **yes — queued "Mixxx ready" flushes here** |

### Deferred startup announcement (current behaviour, issue #49)

`AnnouncementManager` carries two booleans (`announcementmanager.h:196–202`):

| Member | Set by | Meaning |
|---|---|---|
| `m_audioEngineReady` | `slotSoundDevicesReady()` | A sound device is confirmed open and the audio callback is pulling from the `EngineTts` sink |
| `m_pendingReadyAnnouncement` | `slotSkinLoaded()` | "Mixxx ready" wanted to speak but audio wasn't up yet |

`slotSkinLoaded()` (`announcementmanager.cpp:1674–1691`):

1. Returns immediately if `AnnounceStartup` is off.
2. If `m_audioEngineReady`, speaks "Mixxx ready" now.
3. Otherwise sets `m_pendingReadyAnnouncement = true` and speaks nothing.

`slotSoundDevicesReady()` (`announcementmanager.cpp:1693–1700`) sets
`m_audioEngineReady` and, **on the first firing only**, flushes the queued
announcement. Wired at `coreservices.cpp:674–677`:

```cpp
connect(m_pSoundManager.get(),
        &SoundManager::devicesSetup,
        m_pAnnouncementManager.get(),
        &AnnouncementManager::slotSoundDevicesReady);
```

`devicesSetup()` fires only on the **success** path of `setupDevices()`, so it
is a genuine "audio is running" signal, not merely "we tried".

Two failure modes this closes, both real:

- Writing "Mixxx ready" into the FIFO with nothing draining it.
- Barge-in (Invariant 3) discarding it the moment any boot dialog spoke next.

Covered by six tests in `src/test/announcementmanager_test.cpp:451–503`
(queued-then-flushed, immediate-when-ready, setting-disabled,
nothing-queued-is-silent, second-firing-does-not-repeat). Note that many other
announcement tests in that file now have to call `slotSoundDevicesReady()`
first (e.g. lines 971, 983, 995, 1012, 1034, 1132) — a rebase that drops the
two booleans will fail those tests loudly, which is the intended safety net.

### What is still silent at first run

- Every boot-time failure dialog is **written to the `--tts-log`** (see Spec 06)
  but is **not audible**. A blind user hitting a sound-device error still gets
  a dialog they cannot hear and cannot see.
- The normal path is survivable because `SoundManagerConfig::loadDefaults()`
  auto-configures the system default output, so the loops at 497 and 517 are
  skipped, `devicesSetup()` fires, and "Mixxx ready" is the first thing spoken.
- The dialogs' *accessible names* are still set, so an OS screen reader
  (VoiceOver / NVDA / Orca) running alongside Mixxx can read them. That remains
  the only mitigation for the pre-device window.

**Rebase note:** do not let a rebase reorder the boot sequence on the
assumption that speech works before `setupDevices()`. It does not, and the
queueing mechanism is the only thing standing between a blind user and a
silent launch.

## Rebase checklist for this spec

1. `EngineMixer::process()` still calls `m_pTts`, `m_pBeatClick`, `m_pEarcon`
   — in that order, after the sidechain tap at `:763`, before the delay
   processing. **Verify audibly; no test covers this.**
2. `EngineTts::process()` head→main fallback (`enginetts.cpp:165`) intact.
3. `EngineBeatClick` and `EngineEarcon` still follow `[Tts],route_to_main`
   with the same fallback.
4. `EngineBeatClick::addDeck()` is still called from
   `EngineMixer::addChannel()` (`enginemixer.cpp:1005–1010`), **not** the
   constructor — see the ordering rationale in `enginebeatclick.h:31–40`.
5. Generation-counter barge-in intact in all three backends.
6. `sinkDestroyed()` signal and `CoreServices` teardown order both intact.
7. No allocation, lock, or blocking wait introduced into any `process()`.
8. `[Tts],enabled` and `[BeatClick],enabled` still `ButtonMode::Toggle`.
9. `SoundManager::devicesSetup()` → `slotSoundDevicesReady()` connection intact
   (`coreservices.cpp:674–677`), and `m_audioEngineReady` /
   `m_pendingReadyAnnouncement` survive. Upstream renaming or removing
   `devicesSetup()` silently reverts issue #49.

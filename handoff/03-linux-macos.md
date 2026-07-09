# Task 03 — Linux and macOS support

## Goal

Make the accessibility build work on Linux and macOS. The code is
already cross-platform; this is a build/verify/package task.

## Current state

- `src/util/ttsengine.cpp` selects a backend at compile time: SAPI
  (`Q_OS_WIN`), native AVSpeechSynthesizer (`Q_OS_MACOS`, implemented in
  `src/util/ttsenginemac.mm`), Qt TextToSpeech (`MIXXX_USE_QT_TTS`, the
  remaining case — effectively Linux only now), and a silent null
  engine. CMake defines `MIXXX_USE_QT_TTS` only when Qt6 TextToSpeech
  >= 6.6 is found (the `synthesize()` PCM API is 6.6+).
- macOS is done and verified for real (see below). The QtTtsEngine
  (Linux) path has still never been run on a real box.
- The preferences page shows a warning when no backend exists
  (`TtsEngine::isAvailable()`).
- Everything else (announcement manager, engine sink, beat click,
  split cue) is platform-independent.

### 2026-07-09 macOS: done, native backend, fully verified

Investigated on a real Mac (arm64, macOS 26.3). Original plan was "Qt
TTS wraps AVSpeechSynthesizer" via `MIXXX_USE_QT_TTS`, but that hit a
real packaging wall: Mixxx's official macOS dependency bundle
(`mixxx-deps-2.6-arm64-osx-*.zip`, built from the `mixxxdj/vcpkg` fork)
does not include the Qt6 TextToSpeech module at all — confirmed by
inspecting `buildenv/*/installed/arm64-osx-min1100/share/` (no
`Qt6TextToSpeech` directory) — and mixing in a Homebrew-built copy
isn't viable for a distributable build (different Qt version/ABI than
the vcpkg-built app; loading two incompatible QtCore/QtGui copies in
one process risks crashes). Rather than depend on that packaging fix
landing upstream, we bypassed Qt TextToSpeech on macOS entirely and
talk to `AVSpeechSynthesizer` directly instead, the same way
`SapiTtsEngine` talks to Windows' SAPI directly — no extra dependency
to bundle at all, since `AVFAudio` is a system framework already
weak-linked into `mixxx-lib` (also used by the existing AU effects
integration).

- **Implementation:** `src/util/ttsenginemac.h` / `.mm` (Objective-C++,
  same pattern as `darkappearance.mm`/`itunesmacosimporter.mm`).
  `AVSpeechSynthesizer writeUtterance:toBufferCallback:` (available
  since macOS 10.15, well under Mixxx's 11.0 floor) renders offline to
  an `AVAudioPCMBuffer` — no separate audio device, matching the
  "render into our own sink" model the other backends use. Voice
  enumeration uses `AVSpeechSynthesisVoice.identifier` as the id
  (unique, unlike Qt's voice-name-as-id approach flagged as a risk
  below) and `name (language)` as the display string. Barge-in: a
  generation counter (same pattern as `SapiTtsEngine`) plus
  `stopSpeakingAtBoundary:AVSpeechBoundaryImmediate` so a superseded
  utterance's callback is both stopped early and, if it still fires,
  ignored. Wired into `CMakeLists.txt`'s existing
  `if(APPLE) ... else()` (non-iOS) block alongside `darkappearance.mm`;
  links `-weak_framework AVFAudio`.
- **Real bug found & fixed in the process:** built a throwaway
  standalone harness against Homebrew's `qtspeech` package (6.11.1) to
  understand what format the darwin AVSpeechSynthesizer backend
  actually emits (never linked into the real build — just used to
  learn the API before writing `ttsenginemac.mm`). Found
  `synthesize()`/`writeUtterance:` deliver **Float** PCM at 22050 Hz
  mono, not Int16. Qt's own `QtTtsEngine::feed()` in `ttsengine.cpp`
  hard-required `QAudioFormat::Int16` and silently dropped every other
  chunk — on any backend that emits Float (macOS's being one), 100% of
  announcements would render as silence with no error. Fixed
  `feed()` to handle `UInt8`/`Int16`/`Int32`/`Float` via a `sampleAt()`
  helper. This fix is now dead code on macOS (which no longer uses the
  Qt path at all) but stays relevant for Linux, since some
  speech-dispatcher/flite backends could plausibly emit non-Int16
  PCM too.
- **Also found:** AVSpeechSynthesizer (and by extension anything
  wrapping it, including Qt's darwin plugin) only fires its
  callbacks/delegate methods under a genuine Cocoa run loop
  (`QGuiApplication`/`QApplication` + the `cocoa` platform plugin). A
  bare `QCoreApplication`, or Qt's `offscreen` platform (which
  `mixxx-test` forces by default — `src/test/main.cpp:12`), never
  delivers them at all — no error, no callback, forever. Mixxx itself
  and `mixxx-test`'s `QApplication` are fine *if* run with
  `QT_QPA_PLATFORM=cocoa`; **running the TTS integration tests without
  that override will just hang until the 10 s timeout and fail.**
- **Also found and fixed — a real, pre-existing, non-mac-specific bug
  in `src/test/ttsengine_integration_test.cpp`:** `EngineTts::process()`
  requires the `[group],enabled` control to be on (default `false`) or
  it flushes the FIFO and bails every call; the test never set it. And
  independently, `process()` only clears `say()`'s
  `requestFlush()` request on its *own* next call — in the real app the
  audio thread calls `process()` continuously so that flush always
  lands before any synthesized audio exists, but this test only calls
  `process()` once, manually, after confirming the FIFO is non-empty.
  Native macOS synthesis renders a short utterance in ~1 ms — fast
  enough that the whole utterance can land in the FIFO before the
  test's first `process()` call, which then discards it all via the
  still-pending flush and reports false silence. Rewrote the test to
  poll by draining (`waitForNonSilentAudio()`), mirroring how the real
  audio thread consumes the stream, instead of peeking at `isEmpty()`
  then draining once.
- **Verification (real, not simulated):** built the actual
  `mixxx-lib`/`mixxx-test` targets from this checkout (`cmake --build
  build --target mixxx-test`, after `source tools/macos_buildenv.sh
  setup`) and ran the real gtest suite:
  `QT_QPA_PLATFORM=cocoa ./mixxx-test --gtest_filter="TtsEngineIntegrationTest*:AnnouncementManager*:EngineTts*:EngineBeatClick*"`
  — all pass, including both TTS integration tests
  (`SayProducesAudioInOutputBuffer`, `SecondSayInterruptsAndProducesAudio`),
  which exercise the full real `say()` → AVSpeechSynthesizer → resample
  → FIFO → `process()` → mixed-output pipeline end to end. 161/161 in
  the combined filter.

## Work items

1. Build on Linux with Qt >= 6.6 including the TextToSpeech module
   (distro packages often split it: `qt6-speech` / `libqt6texttospeech6-dev`).
2. Run the accessibility test suites (they are hermetic):
   `mixxx-test --gtest_filter=AnnouncementManager*:EngineTts*:EngineBeatClick*:TtsEngineIntegrationTest*`
   — on macOS this needs `QT_QPA_PLATFORM=cocoa` set (see findings
   above) or the TTS integration tests will time out.
3. Manually verify: startup announcement, track-load announcement,
   voice enumeration in Preferences, speech rate mapping. On Linux,
   watch for `QtTtsEngine::enumerateVoices()` using voice *names* as
   IDs — check duplicates behave (macOS's native backend already fixed
   this by using the voice's unique `identifier`).
4. Verify barge-in behaviour on Linux: QtTtsEngine relies on
   `m_pSink->requestFlush()` + synthesize callbacks arriving on the GUI
   thread; watch for overlapping utterances if callbacks interleave.
   (macOS's barge-in is verified — see above.)
5. Keyboard: `Alt+<key>` bindings may collide with desktop-environment
   shortcuts (Alt+1..0 switch workspaces on some DEs) — document or
   provide an alternative binding set if reported.

## Acceptance

- Green accessibility test suites on Linux CI or a local Linux build.
- A written smoke-test log of the manual checklist above.
- Any Linux-specific fixes kept behind the existing #ifdef structure.
- macOS: done — native backend, real bugs fixed, full test suite green
  on real hardware (see 2026-07-09 above). Remaining macOS work, if
  any, is manual UI smoke-testing (Preferences voice list, keyboard
  shortcuts) rather than the backend itself.

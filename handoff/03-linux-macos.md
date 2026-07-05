# Task 03 — Linux and macOS support

## Goal

Make the accessibility build work on Linux and macOS. The code is
already cross-platform; this is a build/verify/package task.

## Current state

- `src/util/ttsengine.cpp` has three backends selected at compile
  time: SAPI (`Q_OS_WIN`), Qt TextToSpeech (`MIXXX_USE_QT_TTS`), and a
  silent null engine. CMake defines `MIXXX_USE_QT_TTS` only when Qt6
  TextToSpeech >= 6.6 is found (the `synthesize()` PCM API is 6.6+).
- The QtTtsEngine path (`synthesize` → nearest-neighbour resample →
  engine sink) has never been run on a real Linux/macOS box.
- The preferences page shows a warning when no backend exists
  (`TtsEngine::isAvailable()`).
- Everything else (announcement manager, engine sink, beat click,
  split cue) is platform-independent.

## Work items

1. Build on Linux with Qt >= 6.6 including the TextToSpeech module
   (distro packages often split it: `qt6-speech` / `libqt6texttospeech6-dev`).
2. Run the accessibility test suites (they are hermetic):
   `mixxx-test --gtest_filter=AnnouncementManager*:EngineTts*:EngineBeatClick*`
3. Manually verify: startup announcement, track-load announcement,
   voice enumeration in Preferences (QtTtsEngine::enumerateVoices uses
   voice *names* as IDs — check duplicates behave), speech rate
   mapping (`setRate(rate/10.0)` — Qt range is -1..1).
4. Verify barge-in behaviour: QtTtsEngine relies on
   `m_pSink->requestFlush()` + synthesize callbacks arriving on the GUI
   thread; watch for overlapping utterances if callbacks interleave.
5. Keyboard: `Alt+<key>` bindings may collide with desktop-environment
   shortcuts (Alt+1..0 switch workspaces on some DEs) — document or
   provide an alternative binding set if reported.
6. macOS afterwards: same checklist; Qt TTS wraps AVSpeechSynthesizer.

## Acceptance

- Green accessibility test suites on Linux CI or a local Linux build.
- A written smoke-test log of the manual checklist above.
- Any Linux-specific fixes kept behind the existing #ifdef structure.

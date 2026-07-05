# Task 02 — Accessible first-run setup

## Goal

Remove the last dependency on sighted help: a fresh Mixxx install
currently shows the sound-hardware dialog *before* the speech engine
can produce audio (speech is mixed into Mixxx's own output, which
doesn't exist until a device is configured).

## Context

- On first run with no usable output, `MixxxMainWindow::initialize()`
  loops a "no sound device" dialog (`src/mixxxmainwindow.cpp`, search
  `noSoundDeviceError` / the `getOutputs().isEmpty()` loop).
- Preferences > Sound Hardware (`dlgprefsound*`) is standard Qt, so a
  screen reader can drive it — but only if the user has one running.
- The engine TTS cannot help before a device opens. Two options that
  can: (a) the platform screen-reader-independent speech path — on
  Windows, SAPI can speak to the *default* device directly without the
  engine (see `SapiTtsEngine` in `src/util/ttsengine.cpp`; a variant
  that calls `pVoice->Speak` without `SetOutput` plays via the system
  default), or (b) making the first-run flow "zero-question": default
  to the system default stereo device automatically and speak once the
  engine is up.

## Suggested approach (incremental)

1. **Auto-default (cheap, big win):** on first run, if no config
   exists, auto-select the system default output for Main before
   showing any dialog, so the engine starts and "Mixxx ready" speaks.
   Only show the error dialog if even that fails.
2. **Spoken fallback for the failure dialog:** when the no-device
   dialog does appear on Windows, speak its text via a direct SAPI
   utterance to the default device (bypass the engine sink). Keep it
   crude — this is a break-glass path.
3. Optional later: a guided "press key to cycle devices, Enter to
   accept" flow in the dialog.

## Acceptance

- Fresh config + working default audio: app starts, engine runs,
  "Mixxx ready" is spoken, no dialogs.
- Fresh config + no working audio: the failure dialog is spoken aloud
  on Windows.
- Existing configs behave exactly as before.
- Unit-test the device-defaulting decision logic if it is factored
  into a testable function; the dialog path is manual-test only.

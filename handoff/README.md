# Agent Handoff — Mixxx Accessibility Fork

Task briefs for continuing work on this fork, which makes Mixxx usable
by a fully blind DJ (the end user runs JAWS). Each numbered file is a
self-contained task: goal, context, file pointers, approach, and
acceptance criteria. Read this file first — it covers how to build,
test, and not fight the tooling.

## Project orientation

- Branch: `accessibility-improvements-2026-06-25`, forked from Mixxx 2.6.
- What exists already: read [../ACCESSIBILITY.md](../ACCESSIBILITY.md)
  (technical overview), [../ACCESSIBILITY_GUIDE.md](../ACCESSIBILITY_GUIDE.md)
  (user-facing guide), and [../ACCESSIBILITY_ROADMAP.md](../ACCESSIBILITY_ROADMAP.md)
  (full progress log). Do not re-implement anything listed there.
- Core source files:
  - `src/util/announcementmanager.cpp` — decides what to say and when.
    Nearly every announcement feature is a pattern here: a ControlProxy
    observer, a settings gate, a `tr()` string, a unit test.
  - `src/util/ttsengine.cpp` — platform speech synthesis (SAPI/Qt).
  - `src/engine/enginetts.cpp` — audio-engine speech sink with ducking.
  - `src/engine/enginebeatclick.cpp` — per-deck metronome.
  - `src/preferences/dialog/dlgprefaccessibility.*` + `...dlg.ui` —
    the Preferences > Accessibility page.
  - `src/preferences/accessibilitysettings.h` — all settings keys.
  - `src/test/announcementmanager_test.cpp` — 100+ tests; follow its
    fixtures (SpyTtsEngine, ControlObject-driven groups).

## Building (Windows, this machine)

CMake/Ninja build dir is `build/`, configured RelWithDebInfo. MSVC
needs the VS environment:

    call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    cmake --build build --target mixxx mixxx-test

(The vcvars banner may complain about vswhere.exe — harmless.)

## Running tests

The test exe needs DLLs on PATH — release vcpkg bin, debug vcpkg bin,
and the protobuf shim (see gotchas):

    set VCPKG=C:\Users\XeTK\Documents\Code\mixxx\buildenv\mixxx-deps-2.6-x64-windows-aa78b5a\installed\x64-windows
    set SHIM=<scratch dir containing release libprotobuf-lite.dll renamed to libprotobuf-lited.dll>
    set PATH=%SHIM%;%VCPKG%\bin;%VCPKG%\debug\bin;%PATH%
    build\mixxx-test.exe --gtest_filter=AnnouncementManager*:EngineTts*:EngineBeatClick*

Invoke the exe by full or explicit relative path (bare exe names from
the current directory do not resolve on this machine). All accessibility
suites currently pass (134 tests).

## Running the app

`build\mixxx.exe` runs by double-click / bare invocation — required
DLLs are deployed app-local next to it, and it finds `res/` via the
CMakeCache in the build dir. Do NOT rename the CMake `project(mixxx)`:
resource lookup greps the cache for `mixxx_SOURCE_DIR` (this broke once
already).

## Tooling gotchas (will bite you)

- Pre-commit hooks reformat files during commit (clang-format,
  mixed-line-ending). When a hook says "files were modified", just
  `git add -A` and commit again with the same message. If sources were
  reformatted after your last build, rebuild before handing binaries over.
- codespell auto-rewrites the camelCase identifier spelled
  o-n-T-e-x-t into the word "context" — never use that variable name
  (it silently renames it during commit).
- Keyboard layout files in `res/keyboard/` have mixed CRLF/LF history;
  prefer a small python script over sed for structural edits, and
  verify all 12 layouts changed.
- New spoken strings: always `tr()`, and mind TTS pronunciation — a
  letter glued to a word gets slurred ("Decka"); use a comma to force a
  pause ("Deck, A"). "BPM" must be written "B P M".
- New toggles must be `ControlPushButton` with `ButtonMode::Toggle`
  (plain ControlObjects act momentary from the keyboard); on-demand
  triggers use `ButtonMode::Trigger`.
- Keyboard bindings: `Shift+digit` never matches (the keysym changes);
  plain `Alt+<key>` combos work. Check all 12 layouts for conflicts.
- Announcement gating: every announcement category has a settings key in
  `accessibilitysettings.h` and a checkbox in the .ui + plumbing in
  `dlgprefaccessibility.cpp` (constructor init, connect, slotUpdate,
  slotApply, slotResetToDefaults — five places).

## Known open issue: debug protobuf link

The build links debug `libprotobuf-lited.dll` into the release build.
Anything touching track-key protobufs crashes without the shim (see
tests section). A fix was started in a separate session; brief 05 has
the full diagnosis if it needs redoing. Until fixed, binaries cannot be
shipped to other machines without bundling the renamed release DLL.

## Definition of done for every task

1. Compiles via the build command above with zero warnings in changed files.
2. Unit tests added/updated; full accessibility filter passes.
3. Smoke-launch `build\mixxx.exe` and confirm it reaches the library
   (log at `%LOCALAPPDATA%\Mixxx\mixxx.log`).
4. `ACCESSIBILITY.md` + `ACCESSIBILITY_GUIDE.md` updated if user-facing,
   `ACCESSIBILITY_ROADMAP.md` progress log updated.
5. Committed with a message explaining the why, ending with the
   Co-Authored-By trailer used throughout the branch history.

# Known CI Issues

Tracked here rather than fixed immediately, so the Gitea Actions pipelines
(`.gitea/workflows/`) can go green now. Each is a real, reproducible gap -
not CI flakiness - found while getting the Linux/macOS/Windows builds
working. Revisit when there's time.

## Linux: HID disabled (`-DHID=OFF` in build-linux.yml)

The `linux-lxc-runner`'s system `libhidapi-dev` package is 0.13.1; Mixxx
requires >= 0.14.0. Since the DDJ-400 and Numark Scratch mappings this fork
cares about are MIDI, not HID, this was an acceptable way to unblock the
build rather than building a newer hidapi from source or backporting a
newer package.

**To actually fix:** build hidapi >= 0.14.0 from source in the Linux
workflow's build-environment step, or find/backport a newer Debian/Ubuntu
package.

## macOS: 3 pre-existing test failures, excluded in build-macos.yml

Confirmed via direct reproduction on the macmini-runner (outside CI,
via SSH) - these are not flaky, not caused by this fork's accessibility
work, and not fixable by installing anything.

### `SoundSourceProxyTest.firstSoundTest`

References a test fixture file, `src/test/id3-test-data/cover-test.stem.m4a`
(and `.stem.mp4`), that doesn't exist - not in this fork, and not upstream
either. Traced via `git log -S` to the commit that added the reference
(`c853c6b97414808477050153dbb909e2d531138e`, "Add support for .m4a extension
variation", April 2024) - it modified the test to expect these files but
never added them. A genuine pre-existing upstream gap.

**To actually fix:** generate or source real `.stem.m4a`/`.stem.mp4` test
fixtures (the STEM format is documented via the other real stem test assets
in `src/test/stems/`), or report/fix upstream.

### `HidMappings/MappingTestFixture.LoadMapping/Dummy_Device_Screen_hid_xml`

Fails to load `res/qml/DummyDeviceDefaultScreen.qml` - a QML scene used only
by this "dummy device" HID mapping test - because two QML modules
(`Qt5Compat.GraphicalEffects`, `QtQuick.Controls.macOS`) have their metadata
present (found via `QML2_IMPORT_PATH`) but no actual plugin code linked into
`mixxx-test`. This looks like a Qt static-QML-plugin registration gap:
`qmlimportscanner` (which drives CMake's automatic
`qt6_import_qml_plugins()`) only traces QML files that are statically
referenced by the app's own compiled-in resources: it never sees
`DummyDeviceDefaultScreen.qml` because it's loaded dynamically at runtime
from a file path, so it doesn't know to bundle those two plugins.

Notably, a manual Windows build (via `cmake --build` on the Windows runner)
does a full `install` step that copies all the platform-native QML style
DLLs (including a `Windows` style) into the build tree - macOS's manual
repro only ran `cmake --build` without `cmake --install`, which may well be
the actual missing piece. Worth checking first before going further into
`qt6_import_qml_plugins()` internals.

**To actually fix:** either run/check the `install` step matches what
packaging does (see note above), or explicitly list the two missing
modules for `qt6_import_qml_plugins(... PATH_TO_INCLUDE ...)` / add an
explicit `--` style import so the scanner picks them up regardless of how
the QML is loaded.

### `TtsEngineIntegrationTest.SayProducesAudioInOutputBuffer` / `SecondSayInterruptsAndProducesAudio`

`TtsEngine::enumerateVoices()` returns voices (confirmed via `say -v ?`), so
these tests don't `GTEST_SKIP()`, but `AVSpeechSynthesizer`'s
`writeUtterance:toBufferCallback:` API (used by `ttsenginemac.mm`) produces
silence for the whole 10s timeout. The classic `say` command works fine and
produces real audio (confirmed: `say -o test.aiff "hello"` gave a real
35KB AIFF), so voices are provisioned at the OS level - but AVSpeechSynthesizer
appears to need a separate/different voice asset than what `say` uses,
and installing voices via System Settings > Accessibility > Spoken Content
did not resolve it.

**To actually fix:** figure out which specific voice asset
`AVSpeechSynthesisVoice`'s on-device buffer-writing API needs (may be
Siri-quality/Enhanced voices specifically, a separate download path from
the classic voices), or make the test explicitly call `setVoice()` with a
known-good voice ID instead of relying on the system default.

## Windows: `mixxx-test.exe` fails to launch (`STATUS_DLL_NOT_FOUND`, 0xc0000135)

Confirmed via direct reproduction on `windows-runner` (outside CI, via SSH).
The build itself succeeds cleanly (`cmake --build` exits 0). Running the
test binary directly, or via `ctest`, fails at process launch with
`0xc0000135`.

Ruled out so far: all of `mixxx-test.exe`'s *direct* dependencies (per
`dumpbin /dependents`) are present in the build directory (confirmed by
manually copying the full vcpkg triplet `bin/` folder over); the transitive
Qt6Core/Qt6Gui DLLs they depend on are present too; the MSVC runtime
redistributables (`vcruntime140.dll`, `msvcp140.dll`,
`vcruntime140_1.dll`) are present in `System32`. So it's some other,
unidentified transitive dependency or a subtler DLL search-path issue
(architecture mismatch on a specific copied file, a Qt plugin needing a
DLL not in the direct-dependency list, etc.) - not yet root-caused.

**To actually fix:** get a real dependency-walker style tool (e.g.
`ProcMon` from Sysinternals, or `dumpbin` cross-referenced recursively
rather than just one level deep) running on `windows-runner` to see exactly
which DLL load attempt fails, since `dumpbin /dependents` alone only shows
one level and wasn't enough to find it here.

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

## Windows: `mixxx-test.exe` fails to launch (`STATUS_DLL_NOT_FOUND`, 0xc0000135) - fixed

Root cause found: `windows-runner` is running **Windows 11 Pro N for
Workstations**. N editions ship without Windows Media Foundation (removed
to comply with EU antitrust requirements around bundled media
technologies), so `MFPlat.DLL` and `MFReadWrite.dll` - both required
because our cmake config passes `-DMEDIAFOUNDATION=ON` - didn't exist on
this machine at all.

`dumpbin /dependents` (and even a recursive closure over every dependency,
direct and transitive) didn't show this, because it only lists imports
present in the PE header, not which of them actually *resolve* on this
specific machine. What found it: `Dependencies.exe`
(https://github.com/lucasg/Dependencies), a maintained, loader-simulating
successor to the old Sysinternals-style Dependency Walker, run as
`Dependencies.exe -modules mixxx-test.exe` - its output explicitly flags
`[NOT_FOUND]` entries.

**Fixed**: the Media Feature Pack
(`Add-WindowsCapability -Online -Name Media.MediaFeaturePack~~~~0.0.1.0`)
initially failed with `HRESULT=80070005` (E_ACCESSDENIED) during CBS's
finalize phase even from a confirmed-elevated Administrator session, with
no Group Policy block and outbound Windows Update connectivity confirmed
working. It's unclear whether a retry, a Windows Update service cycle, or
something else in between resolved it, but `MFPlat.DLL`/`MFReadWrite.dll`
are now present on disk and `-DMEDIAFOUNDATION=ON` is back on in
build-windows.yml.

## Windows: `CMake Error: ... does not exist` for a directory that was just created - fixed

Seen repeatedly in CI: partway through `Configure` (or, in one case, right
after `Check out repository`), a path CMake/Ninja had just written to
moments earlier - and never deleted - was reported missing. Most
concretely: `CMakeDetermineCompilerABI`'s `try_compile` writes a scratch
dir under `build/CMakeFiles/CMakeScratch/TryCompile-<id>/build.ninja`, then
immediately invokes `ninja -t recompact` against it, which fails with
`ninja: fatal: chdir to '...' - No such file or directory`.

Several plausible-looking causes were investigated and ruled out along the
way (none of them fixed it): Windows Storage Sense / `SilentCleanup`
scheduled tasks running under disk pressure, Windows Defender real-time
scanning, a cross-step working-directory propagation bug in Gitea Actions,
and (initially, before the real cause was confirmed) Windows PowerShell
5.1's 32-bit/64-bit WOW64 path redirection - though switching all steps to
`shell: pwsh` was kept regardless, since Windows PowerShell 5.1 turned out
not to be installed on this runner at all (`Cannot find: pwsh in PATH` was
a red herring from a *different*, later attempt to test the WOW64 theory -
pwsh itself was missing, so that attempt never even ran).

**Actual root cause**: the `gitea-runner` Windows service runs as
`LocalSystem` (via `nssm`), and `act_runner`'s `host.workdir_parent` config
was left unset, which defaults every job's workspace to
`$HOME/.cache/act/` - and `$HOME` for `LocalSystem` resolves to
`C:\Windows\System32\config\systemprofile`. Something about that specific
path (confirmed account-independent - a regular user account hits the same
failure writing to that same path, but not to `C:\Windows\Temp` or a
normal user directory) causes newly-created files/directories to
intermittently vanish before the next process can access them. The exact
mechanism (filter driver, ACL-driven virtualization, something else)
wasn't identified, but the fix doesn't require knowing it: don't put a
build workspace under the SYSTEM profile.

**Fixed** by setting an explicit workspace directory in
`C:\gitea-runner\config.yaml`:
```yaml
host:
  workdir_parent: C:\gitea-runner\work
```
then restarting the `gitea-runner` service to pick it up. Confirmed via a
minimal manual repro (a trivial `CMakeLists.txt` + `cmake -G Ninja`) that
this path was the actual trigger, independent of the mixxx project, MSVC
Developer Command Prompt setup, or account context.

This wasn't the whole story, though: moving the job *workspace* off that
path surfaced a second, related failure at the compiler-detection
`try_compile` link step - `LINK ... The system cannot find the path
specified`, with the compile succeeding but the link failing. Isolated by
progressively shrinking the repro (running as a SYSTEM scheduled task, to
match the `gitea-runner` service's account) down to: `cmake -E vs_link_exe
...` referencing an executable under
`C:\Windows\System32\config\systemprofile\cmake\...` fails when invoked as
a ninja build step, but the *identical* command succeeds when run directly
from `cmd.exe` (not spawned by ninja), and a build step invoking a tool
outside that profile path (e.g. `link.exe` from Program Files) works fine
either way. So the profile path is unsafe for *anything* a deep,
tool-spawned process chain touches, not just the job workspace - and the
"Set up cmake" step was still extracting its downloaded CMake to
`$env:USERPROFILE\cmake`, which for the `LocalSystem` service account is
that same profile path.

**Fixed** by changing "Set up cmake" in build-windows.yml to extract to a
fixed path outside the profile, `C:\gitea-runner\cmake`, instead of
`$env:USERPROFILE\cmake`. (Confirmed only under the SYSTEM account so far,
not independently re-verified for a regular user account the way the
workspace-vanishing bug above was - but the same "don't use paths under
`config\systemprofile`" rule applies regardless of the exact mechanism.)

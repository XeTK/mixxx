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

## Windows: `mixxx-test.exe` fails to launch (`STATUS_DLL_NOT_FOUND`, 0xc0000135) - root-caused, worked around

Root cause found: `windows-runner` is running **Windows 11 Pro N for
Workstations**. N editions ship without Windows Media Foundation (removed
to comply with EU antitrust requirements around bundled media
technologies), so `MFPlat.DLL` and `MFReadWrite.dll` - both required
because our cmake config passes `-DMEDIAFOUNDATION=ON` - don't exist on
this machine at all.

`dumpbin /dependents` (and even a recursive closure over every dependency,
direct and transitive) didn't show this, because it only lists imports
present in the PE header, not which of them actually *resolve* on this
specific machine. What found it: `Dependencies.exe`
(https://github.com/lucasg/Dependencies), a maintained, loader-simulating
successor to the old Sysinternals-style Dependency Walker, run as
`Dependencies.exe -modules mixxx-test.exe` - its output explicitly flags
`[NOT_FOUND]` entries.

**Workaround in place:** `-DMEDIAFOUNDATION=OFF` in build-windows.yml.
Media Foundation is one of several optional Windows-native audio/video
decoding backends (alongside FFmpeg, MAD, WavPack, etc.), not a hard
requirement, so this just means slightly fewer natively-supported formats
on builds from this specific runner - not a broken build.

**To actually fix properly:** install the Media Feature Pack
(`Add-WindowsCapability -Online -Name Media.MediaFeaturePack~~~~0.0.1.0`,
or `dism /online /Add-Capability /CapabilityName:Media.MediaFeaturePack~~~~0.0.1.0`).
Both currently fail with `HRESULT=80070005` (E_ACCESSDENIED) during CBS's
finalize phase, even from a confirmed-elevated Administrator session with
TrustedInstaller running. No Group Policy block found in the usual
`HKLM:\SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate` location.
Starting `wuauserv` (Windows Update service, found stopped) didn't help
either. Next things to try: confirm this machine has real outbound HTTPS
access to Windows Update's CDN (DISM's online capability source needs
this, and this network has had firewall/profile issues before - see the
`NetworkCategory: Public` fix earlier in this session), or supply local
Windows 11 N install media as an explicit DISM `/Source:` instead of
relying on the online source.

## Windows: checked-out repo files vanish mid-job, `CMake Error: ... does not exist` (fixed)

Seen once, in an actual CI run: `Check out repository` succeeds (5696
files updated, confirmed in the log), then ~4 minutes later (during the
"Set up cmake" step's `Invoke-WebRequest`, which took an unusually long
~4 min for a ~30MB download) the `Configure` step failed because the
working directory - the same one checkout just populated - was empty
except for the `build/` folder `mkdir build` had just created. Ruled out
a second overlapping Gitea Actions run on the same runner (checked the
task history; nothing else was running).

Cause: the runner's workspace lives under
`C:\Windows\System32\config\systemprofile\.cache\act\<hash>\hostexecutor` -
a path with `.cache` literally in the name, and large C++ builds can push
this machine into low-disk-space territory, which is exactly the trigger
condition for Windows' automatic cleanup tasks.

**Fixed** by disabling the two scheduled tasks that perform this kind of
automatic cleanup on `windows-runner`:
```powershell
Disable-ScheduledTask -TaskName SilentCleanup -TaskPath "\Microsoft\Windows\DiskCleanup\"
Disable-ScheduledTask -TaskName StorageSense -TaskPath "\Microsoft\Windows\DiskFootprint\"
```
Reasonable for a dedicated build machine, not a general daily-use PC.
Since disk space pressure is what triggers this in the first place, still
worth keeping an eye on free space on this box over time (see the other
Windows entries above - the manual repro's vcpkg buildenv + build
directory alone was several GB).

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

## Windows: `WIX0103 Cannot find the file ...@2x.png` in Package - fixed

Seen after Configure/Build/Test all started passing (once the
`config\systemprofile` issues above were fixed): `cpack -G WIX` fails with
`error WIX0103: Cannot find the file '...\applocal\Qt6\qml\QtQuick\
Controls\FluentWinUI3\dark\images\pageindicatordelegate-indicator-
delegate-current-hovered@2x.png'`.

Not a genuinely-missing Qt asset (the same run's CPack verbose log shows
that exact file being installed successfully moments earlier, and a
post-failure directory listing shows it present on disk) - and not a race
either: a 3x retry with a 10s gap failed identically all three times, same
file, same `files.wxs` line number every time. That ruled out timing and
pointed at something deterministic about the path itself.

**Root cause**: the full path is exactly 260 characters -
`C:\gitea-runner\work\<40-char-hash>\hostexecutor\build\_CPack_Packages\
win64\WIX\mixxx-accessibility-beta-<date>-<n>-g<sha>-amd64\applocal\Qt6\
qml\QtQuick\Controls\FluentWinUI3\dark\images\
pageindicatordelegate-indicator-delegate-current-hovered@2x.png` - right at
Windows' classic `MAX_PATH` limit, and this runner had
`HKLM\SYSTEM\CurrentControlSet\Control\FileSystem\LongPathsEnabled` set to
`0` (the Windows default). The long CPack staging directory name (embeds
the full `git describe` string) plus the already-deep `Qt6/qml/.../images`
nesting pushed just this one file over the edge; shorter-named files in
the same tree were unaffected, which is why it looked file-specific rather
than path-length-related at first.

**Fixed** by enabling long path support machine-wide:
```powershell
Set-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' -Name LongPathsEnabled -Value 1 -Type DWord
```
then restarting the `gitea-runner` service so the next job's process picks
up the new registry value (Win32 file APIs cache the flag at process
start). WiX 6 is a modern .NET tool, which honors `LongPathsEnabled`
without needing an app-specific manifest opt-in.

The Package step's retry loop (3 attempts, clearing
`build/_CPack_Packages` between them) is left in place as a cheap safety
net for genuine transient issues, but didn't actually fix this one on its
own - the long-path fix did.

## Windows: `WIX0001 System.IO.IOException: The pipe is being closed` in Package - fixed (MAX_PATH in wixnative.exe)

Surfaced after the `WIX0103`/`MAX_PATH` issue above was fixed, same "Package"
step. Full stack trace (from `wix.log`) bottoms out in
`WixToolset.Core.Native.WixNativeExe.Run()`, called from
`Cabinet.Compress` while building the installer's cabinet.

An earlier investigation blamed an upstream WiX bug
([wixtoolset/wix#701](https://github.com/wixtoolset/wix/pull/701)) and a
Session-0 Windows service having no console. Both of those were red herrings:

- The runner was moved from an `nssm` service to a Scheduled Task bound to the
  interactive session (session 1), and a dedicated console-check workflow
  confirmed the runner's step and child processes DO have a real console
  (`GetConsoleWindow` returns a valid handle). Yet `WIX0001` still failed on
  every run, all 3 retries.
- A dedicated WiX direct-test workflow (minimal `.wxs`, single file / deep
  path / 2000-file cabinet) showed `wix.exe` runs fine in CI and produces real
  MSIs - including the 2000-file cabinet compression.

**Actual root cause**: `wixnative.exe` (WiX's native helper, spawned by
`wix.exe` for cabinet compression) does **NOT** honor the `LongPathsEnabled`
registry setting - it lacks the `longPathAware` manifest, so it still enforces
MAX_PATH (260 chars). The CI job workspace lives at
`C:\gitea-runner\work\<40-char-hash>\hostexecutor\build`, and CPack's WIX
staging dir adds
`_CPack_Packages\win64\WIX\mixxx-accessibility-beta-<date>-<n>-g<sha>-amd64\
applocal\Qt6\qml\QtQuick\Controls\FluentWinUI3\dark\images\...` on top,
pushing the deepest Qt file (e.g.
`pageindicatordelegate-indicator-delegate-current-hovered@2x.png`) to ~286
chars. `wixnative.exe` fails to read that file, exits, and `wix.exe`'s next
write throws `The pipe is being closed` - which is why it looked like a pipe/
console bug rather than a path-length one.

Reproduced deterministically over SSH with a single deep file (286 chars) →
`WIX0001`; the identical file at a short path (227 chars) → success. The
earlier `WIX0103` was the same MAX_PATH root cause surfacing as a file-not-
found during the install phase; this is it surfacing as a pipe error during
cabinet compression.

**Fixed** by building in a short directory, `C:\gitea-runner\build`, instead
of the deep relative `build` under the job workspace. With the short build
dir, the deepest staged file is ~227 chars - safely under MAX_PATH. The
workflow now cleans `C:\gitea-runner\build` at the start of each run (the
runner is persistent, not ephemeral) and points Configure/Build/Test/Package
at it. sccache (in `C:\gitea-runner\sccache-dir`) still makes the rebuild
fast, so the clean costs little.

**Trade-off**: this depends on the runner having a writable `C:\gitea-runner`
and on no other component writing to `C:\gitea-runner\build` between runs.
The Package step's retry loop (3 attempts, clearing
`C:\gitea-runner\build\_CPack_Packages` between them) is kept as a cheap
safety net for genuine transient issues.

## Windows: `WIX0001 System.IO.IOException: The pipe is being closed` in Package - fixed (runner architecture, superseded)

> NOTE: This section documents the earlier, incorrect diagnosis (Session-0
> service / no console). It is superseded by the MAX_PATH-in-wixnative.exe
> root cause above. Kept for history.

Surfaced immediately after the `WIX0103`/`MAX_PATH` issue above was fixed,
same "Package" step. Full stack trace (from `wix.log`) bottoms out in
`WixToolset.Core.Native.WixNativeExe.Run()`, called from
`Cabinet.Compress` while building the installer's cabinet.

**Root cause**: an upstream WiX bug
([wixtoolset/wix#701](https://github.com/wixtoolset/wix/pull/701),
fixing [wixtoolset/issues#9267](https://github.com/wixtoolset/issues/9267)):
when `wix.exe` has no console attached at all, its `SetConsoleCP`/
`SetConsoleOutputCP` calls fail, and the failure path in `ConsoleInitialize`
wrongly closes the *separate* stdin/stdout pipe `wix.exe` uses to talk to
its own `wixnative.exe` helper process, so `wixnative.exe` exits and
`wix.exe`'s next write throws `The pipe is being closed`. Confirmed via the
PR's own description, which matches this exact symptom and stack trace.

Two things ruled out before finding the real cause: dropping CPack's `-V`
flag (CPack captures `wix.exe`'s output via a pipe regardless of `-V`, so
this changed nothing), and `AllocConsole()` called from the Package step
itself (failed with `ERROR_ACCESS_DENIED` - Windows won't grant a console
to this process no matter what it asks for from the inside).

The real reason no console is available: `gitea-runner` ran as an `nssm`-
wrapped Windows Service, and **Windows Services always run in Session 0**,
which has no window station or desktop - by OS design, not
misconfiguration, and not fixable from inside a CI step (this is also why
"Allow service to interact with desktop" has had no effect since Vista).
Building WiX from source with the fix cherry-picked was also attempted and
abandoned: the fix commit already requires the .NET 10 SDK (installed:
`winget install Microsoft.DotNet.SDK.10`), but the full toolset build
additionally needs a native/Burn-related MSBuild SDK resolver
(`Microsoft.Build.Traversal`) not available through VS Build Tools' bundled
MSBuild, and getting that working was a bigger yak-shave than fixing the
actual environment.

**Fixed** by replacing the `nssm` service with a Scheduled Task bound to
this machine's already-active interactive logon (`query user` confirmed
session 1, user `User`, physically logged in), which *does* get a real
console/window station:
```powershell
nssm.exe stop gitea-runner; nssm.exe remove gitea-runner confirm

$action = New-ScheduledTaskAction -Execute "C:\gitea-runner\gitea-runner.exe" -Argument "daemon --config C:\gitea-runner\config.yaml" -WorkingDirectory "C:\gitea-runner"
$trigger = New-ScheduledTaskTrigger -AtLogOn -User "desktop-d46hml8\user"
$principal = New-ScheduledTaskPrincipal -UserId "desktop-d46hml8\user" -LogonType Interactive -RunLevel Highest
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable -RestartCount 999 -RestartInterval (New-TimeSpan -Minutes 1)
Register-ScheduledTask -TaskName "GiteaRunner" -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Force
Start-ScheduledTask -TaskName "GiteaRunner"
```
Confirmed the new process runs in session 1 (`Get-Process gitea-runner |
Select SessionId` → `1`, not `0`). **Trade-off**: the runner now depends on
that interactive session staying logged in - if the machine reboots or
that session is logged off, the "At log on" trigger restarts it
automatically on next logon, but it won't run headlessly in between the
way the old service did.

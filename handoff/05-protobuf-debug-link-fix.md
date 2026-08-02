# Task 05 — Fix the debug-protobuf link (release build correctness)

**Status: RESOLVED (verified 2026-07-16). No source change was ever
needed — the cause was a stale CMake cache directory, and the fix is a
clean reconfigure. This brief is kept as the diagnosis, in case the
symptom reappears after a dependency swap. Nothing to implement.**

## Root cause and fix

`build/CMakeFiles/<cmake-version>/` caches compiler-detection results.
When that directory is stale from an earlier configure, the vcpkg
toolchain's `if(NOT DEFINED CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO)`
guard short-circuits on an *incremental* reconfigure, so its
config-mapping fixup never runs. protobuf and qtkeychain advertise
`IMPORTED_CONFIGURATIONS` of `DEBUG;RELEASE` with no exact
RelWithDebInfo match, so CMake falls back to the first available
configuration — DEBUG. That is why `qt6keychaind.dll` came along too:
it was never protobuf-specific.

Fix: delete `build/CMakeFiles/<version>/` (or the whole `build/`) and
reconfigure clean. Verify with
`grep -c libprotobuf-lited build/build.ninja` → must be 0. Confirmed by
`dumpbin /dependents` picking up release `libprotobuf-lite.dll` and
`qt6keychain.dll`, and by all seven
`AnnouncementManagerTest.FormatForLoad*` tests passing with no shim on
PATH.

A failed first configure in a fresh worktree (e.g. one missing
`-DCMAKE_TOOLCHAIN_FILE=<buildenv>/scripts/buildsystems/vcpkg.cmake`)
poisons that same cache and reproduces the bug — so always configure a
new worktree correctly the first time, or `rm -rf build` after a failed
attempt.

## Original diagnosis (kept for reference)

## Symptom

The RelWithDebInfo build links the DEBUG vcpkg protobuf:
`dumpbin /dependents build\mixxx-test.exe` shows `libprotobuf-lited.dll`
(and `qt6keychaind.dll`) among otherwise-release DLLs. Mixing the debug
protobuf runtime with release-compiled generated code corrupts memory:
anything touching track-key protobufs (`Track::setKeyText`,
`proto/keys.pb.h`) crashes with access violation 0xC0000005. Confirmed
via Windows Event Log (faulting module libprotobuf-lited.dll) — it
crashed the app on startup when the library loaded key-analyzed tracks.

## Former workaround (no longer required)

A copy of the RELEASE `libprotobuf-lite.dll` renamed to
`libprotobuf-lited.dll` was placed next to `build\mixxx.exe` (and in a
shim dir used for test runs). The same-named DLL satisfied the import
and the release ABI matched the release-compiled callers. Obsolete
since the reconfigure fix; leftover copies in `build/` are inert and
can be deleted.

## Where to look

- vcpkg toolchain: `buildenv/mixxx-deps-2.6-x64-windows-aa78b5a/`.
- Why did CMake select the debug lib for a RelWithDebInfo config?
  Suspects: `find_package(Protobuf)` resolving `Protobuf_LIBRARY_DEBUG`
  and mapping RelWithDebInfo → Debug via `CMAKE_MAP_IMPORTED_CONFIG_*`,
  or vcpkg's config mapping treating RelWithDebInfo as non-Release.
  `qt6keychaind.dll` being debug too says it's a config-mapping issue,
  not protobuf-specific.
- Check `CMakeCache.txt` for `Protobuf_LIBRARY*` values, and CMakeLists
  for how protobuf/qtkeychain targets are consumed.
- Likely fix: set `CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "RelWithDebInfo;Release;"`
  (or the vcpkg-recommended equivalent) before the affected find_packages,
  then a clean reconfigure.

## Acceptance (met 2026-07-16, apart from deleting the inert leftovers)

- `dumpbin /dependents` on both exes shows no `*d.dll` vcpkg libraries.
- `mixxx-test.exe --gtest_filter=AnnouncementManagerTest.FormatForLoad*`
  passes WITHOUT the shim on PATH.
- App starts and loads a key-analyzed library without the shim DLL
  next to the exe; delete the shim copies afterwards
  (`build/libprotobuf-lited.dll` and the scratch shim dir) and remove
  the shim mention from handoff/README.md.

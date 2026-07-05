# Task 05 — Fix the debug-protobuf link (release build correctness)

**Status: a fix was started in a separate session (spawned task
"Fix debug protobuf link in RelWithDebInfo build"). Check whether it
landed before redoing anything — if its branch/commits exist, review
and merge them instead.**

## Symptom

The RelWithDebInfo build links the DEBUG vcpkg protobuf:
`dumpbin /dependents build\mixxx-test.exe` shows `libprotobuf-lited.dll`
(and `qt6keychaind.dll`) among otherwise-release DLLs. Mixing the debug
protobuf runtime with release-compiled generated code corrupts memory:
anything touching track-key protobufs (`Track::setKeyText`,
`proto/keys.pb.h`) crashes with access violation 0xC0000005. Confirmed
via Windows Event Log (faulting module libprotobuf-lited.dll) — it
crashed the app on startup when the library loaded key-analyzed tracks.

## Current workaround (must be removed once fixed)

A copy of the RELEASE `libprotobuf-lite.dll` renamed to
`libprotobuf-lited.dll` sits next to `build\mixxx.exe` (also in a shim
dir used for test runs). Same-named DLL satisfies the import and the
release ABI matches the release-compiled callers, so everything works —
but it's a hack, and shipping to another machine requires bundling it.

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

## Acceptance

- `dumpbin /dependents` on both exes shows no `*d.dll` vcpkg libraries.
- `mixxx-test.exe --gtest_filter=AnnouncementManagerTest.FormatForLoad*`
  passes WITHOUT the shim on PATH.
- App starts and loads a key-analyzed library without the shim DLL
  next to the exe; delete the shim copies afterwards
  (`build/libprotobuf-lited.dll` and the scratch shim dir) and remove
  the shim mention from handoff/README.md.

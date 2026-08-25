# Task 05 — Fix the debug-protobuf link (release build correctness)

**Status: RESOLVED. The RelWithDebInfo build no longer links the debug
vcpkg protobuf; the shim is obsolete. Root cause was a config-mapping
issue, and a durable CMake fix now guards against it unconditionally.**

## Root cause and fix

protobuf and qt6keychain advertise `IMPORTED_CONFIGURATIONS` of
`DEBUG;RELEASE` with no exact `RelWithDebInfo` match. When building
RelWithDebInfo, CMake falls back to the first available imported
configuration — DEBUG — so the release build links `libprotobuf-lited.dll`
and `qt6keychaind.dll`. Mixing the debug protobuf runtime with
release-compiled generated code corrupts memory: anything touching
track-key protobufs crashes with access violation 0xC0000005.

`qt6keychaind.dll` being debug too confirmed it was a config-mapping
issue, not protobuf-specific.

The vcpkg toolchain normally sets
`CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "RelWithDebInfo;Release;None;"`,
but only behind an `if(NOT DEFINED ...)` guard. A stale
`build/CMakeFiles/<cmake-version>/` cache can make that guard
short-circuit on an *incremental* reconfigure, so the mapping never gets
applied and imports fall back to DEBUG.

Fix (landed in `CMakeLists.txt`, right after the build-type block): set
`CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "RelWithDebInfo;Release;None;"`
unconditionally (still guarded by `NOT DEFINED` so the user/toolchain
can override) before any `find_package`. This makes the mapping robust
regardless of cache state. A clean reconfigure is still recommended
after a dependency swap or a failed first configure.

Verify with `grep -c libprotobuf-lited build/build.ninja` → must be 0,
and `dumpbin /dependents` on both exes shows no `*d.dll` vcpkg libraries.

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
since the CMake fix; leftover copies in `build/` are inert and can be
deleted.

## Where to look

- vcpkg toolchain: `buildenv/mixxx-deps-2.6-x64-windows-aa78b5a/`.
- The fix lives in `CMakeLists.txt` (the `CMAKE_MAP_IMPORTED_CONFIG_*
  block after the build-type handling).

## Acceptance

- `dumpbin /dependents` on both exes shows no `*d.dll` vcpkg libraries.
- `mixxx-test.exe --gtest_filter=AnnouncementManagerTest.FormatForLoad*`
  passes WITHOUT the shim on PATH.
- App starts and loads a key-analyzed library without the shim DLL
  next to the exe; delete the shim copies afterwards
  (`build/libprotobuf-lited.dll` and the scratch shim dir) and remove
  the shim mention from handoff/README.md.

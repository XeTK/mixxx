#pragma once

#include <QFile>
#include <QString>

namespace mixxx {
namespace android {

/// Resolves an Android `content://` URI (as obtained from the Storage
/// Access Framework folder picker used to grant Mixxx access to a music
/// library) to a live, readable file descriptor.
///
/// Content URIs cannot be opened via fopen()/QFile using their own string
/// representation; only Android's ContentResolver can resolve them, and
/// only to a file descriptor, not a path. Converting that descriptor into
/// a path (e.g. via its `/proc/self/fd/N` symlink) and letting a C
/// library reopen it by that path does not work reliably from within the
/// app's own process - confirmed concretely against a real emulator, and
/// without a corresponding SELinux denial, i.e. for reasons other than
/// the sandboxing SAF is nominally there to enforce. Callers must instead
/// read through the descriptor directly (see sources/posixfdiostream.h
/// for a TagLib::IOStream that does this).
///
/// The returned descriptor is safe to share across multiple concurrent
/// readers *that use pread() rather than lseek()+read()* (as
/// PosixFdIOStream does) - reads specify their own offset explicitly
/// rather than relying on/mutating the shared kernel-level file
/// position. A consumer that can only read via a path or a plain
/// lseek()+read()-based API (e.g. QFile::open(fd, ...)) must first
/// ::dup() the returned descriptor to get its own independent one.
///
/// The descriptor is kept open in a small process-wide cache (see
/// contenturiresolver.cpp) so it stays valid for as long as it might
/// still be needed, with the oldest entries closed once the cache fills
/// up - unbounded accumulation would otherwise exhaust the process' file
/// descriptor limit during a full library (re-)scan.
///
/// Returns -1 if the URI cannot be opened (e.g. the underlying document
/// was deleted, or access was revoked). Do not close the returned
/// descriptor; it is owned by the cache.
int resolveContentUriToSharedReadFd(const QString& contentUri);

/// Opens `pFile` for reading an Android content:// URI, for the
/// QFile-based SoundSource providers (FLAC, MP3, Ogg Vorbis).
///
/// Unlike resolveContentUriToSharedReadFd()'s caller-managed pread()
/// access, QFile reads via a plain, shared, mutable file position - so
/// this duplicates the cached descriptor first (via ::dup()) to give
/// `pFile` its own independent one, safe to seek/read concurrently with
/// any other consumer of the same URI. The duplicate is closed
/// automatically when `pFile` is closed or destroyed.
///
/// Returns false (and leaves `pFile` unopened) if the URI cannot be
/// resolved to a descriptor, or the ::dup() call itself fails (e.g. the
/// process' descriptor limit has been reached).
bool openContentUriAsQFile(const QString& contentUri, QFile* pFile);

} // namespace android
} // namespace mixxx

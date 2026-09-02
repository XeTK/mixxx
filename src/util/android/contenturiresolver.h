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
/// position. A consumer that can only read via a plain
/// lseek()+read()-based API (e.g. QFile) must NOT use this descriptor
/// (nor a ::dup() of it - see openContentUriAsQFile()'s comment for why
/// that doesn't give the independence it looks like it would).
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
/// Gets a fully independent ContentResolver-issued descriptor for every
/// call, rather than sharing/duplicating resolveContentUriToSharedReadFd()'s
/// cached one: ::dup() creates a new descriptor *number*, but that
/// number still shares the same underlying open file description - and
/// therefore the same lseek/read cursor - as the descriptor it was
/// dup'd from. QFile reads via a plain, stateful lseek()+read(), so two
/// QFiles sharing a dup() lineage of the same URI corrupt each other's
/// reads the moment they're used concurrently (confirmed concretely: a
/// track loaded to a deck while the background analyzer thread scanned
/// the same track caused FLAC "LOST_SYNC" decode errors in the
/// analyzer). A fresh, independent ContentResolver.openFileDescriptor()
/// call per QFile avoids this entirely.
///
/// Returns false (and leaves `pFile` unopened) if the URI cannot be
/// resolved to a descriptor.
bool openContentUriAsQFile(const QString& contentUri, QFile* pFile);

/// Resolves an Android content:// URI to a fully independent, caller-owned
/// file descriptor, for consumers with their own raw-fd-based API (e.g.
/// libsndfile's sf_open_fd(), used by SoundSourceSndFile for WAV/AIFF).
///
/// Same independence guarantee as openContentUriAsQFile() and for the same
/// reason: a plain lseek()+read()-based reader must not share a fd (or a
/// ::dup() of one) with any other concurrent reader of the same URI.
///
/// Returns -1 if the URI cannot be resolved to a descriptor. The caller
/// owns the returned descriptor and is responsible for closing it (or
/// handing ownership to a C API that will, e.g. sf_open_fd's close_desc).
int openContentUriIndependentFd(const QString& contentUri);

} // namespace android
} // namespace mixxx

#pragma once

#include <QString>

namespace mixxx {
namespace android {

/// Resolves an Android `content://` URI (as obtained from the Storage
/// Access Framework folder picker used to grant Mixxx access to a music
/// library) to a plain POSIX path that libraries expecting a real
/// filesystem path - TagLib and the QFile-based SoundSource providers -
/// can open directly.
///
/// Content URIs cannot be opened via fopen()/QFile with their own string
/// representation; only Android's ContentResolver can resolve them, and
/// only to a file descriptor, not a path. This obtains that descriptor
/// and exposes it via its `/proc/self/fd/N` symlink, which a subsequent
/// open() call on a regular, seekable, filesystem-backed document (true
/// for local Storage Access Framework providers, e.g. the built-in
/// "Internal storage"/"documentsui" one used to grant a Music folder)
/// can follow like a normal path.
///
/// The underlying descriptor is kept open in a small process-wide cache
/// (see contenturiresolver.cpp) so the /proc symlink stays valid for as
/// long as the returned path might still be in use, with the oldest
/// entries closed once the cache fills up - unbounded accumulation would
/// otherwise exhaust the process' file descriptor limit during a full
/// library (re-)scan.
///
/// Returns an empty string if the URI cannot be opened (e.g. the
/// underlying document was deleted, or access was revoked).
QString resolveContentUriToFilePath(const QString& contentUri);

} // namespace android
} // namespace mixxx

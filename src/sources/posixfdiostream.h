#pragma once

#include <taglib/taglib.h>
#include <taglib/tiostream.h>

namespace mixxx {

#if TAGLIB_MAJOR_VERSION >= 2
using TagLibOffset = TagLib::offset_t;
using TagLibReadLength = size_t;
#else
using TagLibOffset = long;
using TagLibReadLength = unsigned long;
#endif

/// Read-only TagLib::IOStream over a plain POSIX file descriptor.
///
/// Exists because Android's Storage Access Framework hands out tracks as
/// content:// URIs; the only way to get bytes out of one is a live file
/// descriptor from ContentResolver.openFileDescriptor(). Converting that
/// into a path (e.g. via the /proc/self/fd/N symlink trick) and letting
/// TagLib fopen() it itself does not work reliably from within the app's
/// own process - confirmed concretely against a real emulator: the
/// descriptor is valid and points at a real, readable file (verified
/// externally), but the app's own re-open of that same path fails with
/// no corresponding SELinux denial, i.e. for reasons other than the
/// sandboxing SAF is nominally there to enforce. Handing TagLib this
/// stream directly, backed by the original descriptor, sidesteps needing
/// a path at all.
///
/// Uses pread() rather than lseek()+read(), so multiple independent
/// PosixFdIOStream instances can safely share the exact same underlying
/// fd (as util/android/contenturiresolver.h's cache does) without racing
/// each other's read position - each instance tracks its own cursor and
/// passes it explicitly on every read, never mutating the shared
/// kernel-level file offset.
///
/// Does not take ownership of the fd: the caller must keep it open for
/// at least as long as this stream is used and remains responsible for
/// closing it afterwards.
class PosixFdIOStream : public TagLib::IOStream {
  public:
    explicit PosixFdIOStream(int fd);
    ~PosixFdIOStream() override = default;

    TagLib::FileName name() const override;

    TagLib::ByteVector readBlock(TagLibReadLength length) override;
    void writeBlock(const TagLib::ByteVector& data) override;
    void insert(
            const TagLib::ByteVector& data,
            TagLibReadLength start,
            TagLibReadLength replace) override;
    void removeBlock(TagLibReadLength start, TagLibReadLength length) override;

    bool readOnly() const override;
    bool isOpen() const override;

    void seek(TagLibOffset offset, Position p = Beginning) override;
    TagLibOffset tell() const override;
    TagLibOffset length() override;
    void truncate(TagLibOffset length) override;

  private:
    int m_fd;
    TagLibOffset m_position;
};

} // namespace mixxx

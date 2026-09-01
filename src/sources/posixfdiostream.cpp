#include "sources/posixfdiostream.h"

#include <sys/stat.h>
#include <unistd.h>

namespace mixxx {

PosixFdIOStream::PosixFdIOStream(int fd)
        : m_fd(fd),
          m_position(0) {
}

TagLib::FileName PosixFdIOStream::name() const {
    // TagLib only uses this for diagnostic messages; there is no path to
    // report for a content URI's file descriptor.
    return "<file-descriptor>";
}

TagLib::ByteVector PosixFdIOStream::readBlock(TagLibReadLength length) {
    if (m_fd < 0 || length == 0) {
        return TagLib::ByteVector();
    }
    TagLib::ByteVector result(static_cast<unsigned int>(length), '\0');
    const ssize_t bytesRead = ::pread(
            m_fd, const_cast<char*>(result.data()), length, m_position);
    if (bytesRead <= 0) {
        return TagLib::ByteVector();
    }
    m_position += bytesRead;
    if (static_cast<TagLibReadLength>(bytesRead) < length) {
        result.resize(static_cast<unsigned int>(bytesRead));
    }
    return result;
}

void PosixFdIOStream::writeBlock(const TagLib::ByteVector&) {
    // Read-only: writing tags back to an Android content:// track is not
    // supported (see MetadataSourceTagLib::exportTrackMetadata(), which
    // never constructs this stream in the first place).
}

void PosixFdIOStream::insert(const TagLib::ByteVector&, TagLibReadLength, TagLibReadLength) {
}

void PosixFdIOStream::removeBlock(TagLibReadLength, TagLibReadLength) {
}

bool PosixFdIOStream::readOnly() const {
    return true;
}

bool PosixFdIOStream::isOpen() const {
    return m_fd >= 0;
}

void PosixFdIOStream::seek(TagLibOffset offset, Position p) {
    switch (p) {
    case Beginning:
        m_position = offset;
        break;
    case Current:
        m_position += offset;
        break;
    case End:
        m_position = length() + offset;
        break;
    }
}

TagLibOffset PosixFdIOStream::tell() const {
    return m_position;
}

TagLibOffset PosixFdIOStream::length() {
    struct stat st;
    if (m_fd < 0 || ::fstat(m_fd, &st) != 0) {
        return 0;
    }
    return static_cast<TagLibOffset>(st.st_size);
}

void PosixFdIOStream::truncate(TagLibOffset) {
    // Read-only, see writeBlock().
}

} // namespace mixxx

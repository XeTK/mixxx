#pragma once

#include "util/assert.h"

#include <QUrl>

namespace mixxx {

class UrlResource {
  public:
    virtual ~UrlResource() = default;

    QUrl getUrl() const {
        return m_url;
    }
    QString getUrlString() const {
        return m_url.toString();
    }

  protected:
    explicit UrlResource(const QUrl& url)
            : m_url(url) {
    }

    inline bool isLocalFile() const {
        // TODO(XXX): We need more testing how network shares are
        // handled! From the documentation of QUrl::isLocalFile():
        // "Note that this function considers URLs with hostnames
        // to be local file paths, ..."
        return m_url.isLocalFile();
    }

    // On Android, this may return an Android Storage Access Framework
    // content:// URI string rather than an actually-local path: it
    // round-trips through mixxx::FileInfo::toQUrl(), which unconditionally
    // calls QUrl::fromLocalFile() - that forces scheme() to "file" (so
    // isLocalFile() above is misleadingly true for these too), but
    // toLocalFile() still hands back the original content:// string as
    // literal, unmangled text. Callers that actually open the file (as
    // opposed to using this for display/logging) must check for that
    // prefix themselves and go through
    // util/android/contenturiresolver.h instead of treating it as an
    // openable path - see e.g. sources/soundsourceflac.cpp or
    // sources/metadatasourcetaglib.cpp.
    inline QString getLocalFileName() const {
        DEBUG_ASSERT(isLocalFile());
        return m_url.toLocalFile();
    }

  private:
    QUrl m_url;
};

} // namespace mixxx

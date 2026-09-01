#pragma once

#include "util/assert.h"

#include <QUrl>

#ifdef Q_OS_ANDROID
#include "util/android/contenturiresolver.h"
#endif

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

    inline QString getLocalFileName() const {
        DEBUG_ASSERT(isLocalFile());
        const QString localFileName = m_url.toLocalFile();
#ifdef Q_OS_ANDROID
        // Tracks added via the Storage Access Framework folder picker
        // (the only way to grant Mixxx access to a music library on
        // Android) have a content:// URI as their canonical location.
        // That location round-trips through mixxx::FileInfo::toQUrl(),
        // which unconditionally calls QUrl::fromLocalFile() - this
        // forces scheme() to "file" (so isLocalFile() above is
        // misleadingly true, and scheme() can never actually read
        // "content" here), but toLocalFile() still hands back the
        // original content:// string as literal, unmangled text. Detect
        // it by content and bridge to a real, POSIX-openable path so
        // TagLib and the QFile-based SoundSource providers work the same
        // as they do with real file paths.
        if (localFileName.startsWith(QLatin1String("content://"))) {
            return android::resolveContentUriToFilePath(localFileName);
        }
#endif
        return localFileName;
    }

  private:
    QUrl m_url;
};

} // namespace mixxx

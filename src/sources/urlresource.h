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
#ifdef Q_OS_ANDROID
        // Tracks added via the Storage Access Framework folder picker
        // (the only way to grant Mixxx access to a music library on
        // Android) are stored as content:// URIs, not file:// paths -
        // isLocalFile() is false for these, so there is no local path
        // for QUrl to hand back. Bridge to a real, POSIX-openable path
        // instead of asserting/returning empty, so TagLib and the
        // QFile-based SoundSource providers work the same as they do
        // with real file paths.
        if (m_url.scheme() == QLatin1String("content")) {
            return android::resolveContentUriToFilePath(m_url);
        }
#endif
        DEBUG_ASSERT(isLocalFile());
        return m_url.toLocalFile();
    }

  private:
    QUrl m_url;
};

} // namespace mixxx

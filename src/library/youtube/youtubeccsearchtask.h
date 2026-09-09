#pragma once

#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>

#include "library/youtube/youtubecctrack.h"

class QNetworkReply;

/// Queries the YouTube Data API v3 for Creative Commons licensed music.
///
/// All results are filtered to `videoLicense=creativeCommon` at the API, so
/// every track returned is licensed for reuse (with attribution). A single
/// instance may be reused for successive searches; a new search cancels any
/// in-flight request.
///
/// Requires a YouTube Data API key (free, from the Google Cloud console).
/// Only the public `search`/`videos` endpoints are used, so no OAuth or user
/// login is involved.
class YouTubeCcSearchTask : public QObject {
    Q_OBJECT
  public:
    explicit YouTubeCcSearchTask(QObject* parent = nullptr);
    ~YouTubeCcSearchTask() override;

    void setApiKey(const QString& apiKey) {
        m_apiKey = apiKey;
    }
    bool hasApiKey() const {
        return !m_apiKey.isEmpty();
    }

    /// Start a search. Emits succeeded() or failed() when done.
    void search(const QString& query, int maxResults = 25);
    void abort();

  signals:
    void succeeded(const QList<YouTubeCcTrack>& results);
    void failed(const QString& message);

  private slots:
    void onSearchReplyFinished();
    void onDetailsReplyFinished();

  private:
    void requestDetails(QList<YouTubeCcTrack> tracks);

    QNetworkAccessManager m_network;
    QString m_apiKey;
    QPointer<QNetworkReply> m_pReply;
    // Search results awaiting the follow-up videos.list (duration/license) call.
    QList<YouTubeCcTrack> m_pendingTracks;
};

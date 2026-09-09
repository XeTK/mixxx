#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "library/youtube/youtubecctrack.h"

class QProcess;

/// Searches YouTube for Creative Commons licensed music via `yt-dlp`.
///
/// Uses `yt-dlp "ytsearchN:<query>"` with a `--match-filter` on the license
/// field, so only Creative Commons (CC BY) videos are returned — enforced by
/// yt-dlp itself. No YouTube Data API key and no Google account are required;
/// the only dependency is the same `yt-dlp` used for downloading.
///
/// A single instance may be reused; a new search cancels any in-flight one.
class YouTubeCcSearchTask : public QObject {
    Q_OBJECT
  public:
    explicit YouTubeCcSearchTask(QObject* parent = nullptr);
    ~YouTubeCcSearchTask() override;

    /// Path to the yt-dlp executable (looked up on PATH if just "yt-dlp").
    void setYtDlpPath(const QString& path) {
        m_ytDlpPath = path;
    }

    bool isBusy() const;

    /// Start a search. Emits succeeded() or failed() when done. Because each
    /// result is fully extracted to check its license, a search of N results
    /// may take a few seconds.
    void search(const QString& query, int maxResults = 15);
    void abort();

  signals:
    void succeeded(const QList<YouTubeCcTrack>& results);
    void failed(const QString& message);

  private slots:
    void onProcessFinished(int exitCode);
    void onProcessErrorOccurred();

  private:
    QString m_ytDlpPath;
    QProcess* m_pProcess = nullptr;
};

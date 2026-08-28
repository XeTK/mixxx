#pragma once

#include <QObject>
#include <QString>

#include "library/youtube/youtubetrack.h"

class QProcess;

/// Downloads the audio of a single YouTube video via the external `yt-dlp`
/// tool into a local cache directory. Only ever invoked for Creative Commons
/// licensed tracks (see YouTubeSearchTask / the UI gate).
///
/// The download is audio-only and is not re-encoded, so `ffmpeg` is not
/// required. Files are cached by videoId; a second request for an already
/// cached track completes immediately without spawning a process.
class YouTubeDownloader : public QObject {
    Q_OBJECT
  public:
    explicit YouTubeDownloader(QObject* parent = nullptr);
    ~YouTubeDownloader() override;

    /// Path to the yt-dlp executable (looked up on PATH if just "yt-dlp").
    void setYtDlpPath(const QString& path) {
        m_ytDlpPath = path;
    }
    /// Directory into which audio files are downloaded/cached.
    void setCacheDir(const QString& dir) {
        m_cacheDir = dir;
    }

    /// Returns the cached file path for a videoId, or an empty string if the
    /// track has not been downloaded yet.
    QString cachedPath(const QString& videoId) const;

    bool isBusy() const;

    /// Start downloading the given track. Emits progress()/succeeded()/failed().
    /// Only one download runs at a time; a second call while busy is rejected
    /// via failed().
    void download(const YouTubeTrack& track);
    void cancel();

  signals:
    void progress(const QString& videoId, int percent);
    void succeeded(const YouTubeTrack& track, const QString& localPath);
    void failed(const QString& videoId, const QString& message);

  private slots:
    void onReadyReadStandardOutput();
    void onProcessFinished(int exitCode);
    void onProcessErrorOccurred();

  private:
    QString m_ytDlpPath;
    QString m_cacheDir;
    QProcess* m_pProcess = nullptr;
    YouTubeTrack m_currentTrack;
    QString m_resolvedPath;
    // Set if yt-dlp reported the video was skipped by the CC license filter.
    bool m_wasFilteredOut = false;
};

#pragma once

#include <QString>
#include <QUrl>

/// A single Creative Commons search result from the YouTube Data API.
/// This is metadata only; the audio is not downloaded until the user
/// explicitly fetches the track (see YouTubeCcDownloader).
struct YouTubeCcTrack {
    QString videoId;
    QString title;
    QString channelTitle;
    QUrl thumbnailUrl;
    // Duration in seconds. 0 if unknown (search.list does not return it;
    // it is filled in by the follow-up videos.list call).
    int durationSecs = 0;
    // Confirmed Creative Commons via videos.list status.license. The search
    // is already filtered to CC, but we re-verify before offering a download.
    bool licenseIsCreativeCommons = true;

    QUrl watchUrl() const {
        return QUrl(QStringLiteral("https://www.youtube.com/watch?v=") + videoId);
    }

    bool isValid() const {
        return !videoId.isEmpty();
    }
};

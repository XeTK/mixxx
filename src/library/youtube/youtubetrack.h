#pragma once

#include <QString>
#include <QUrl>

/// A single Creative Commons search result from YouTube.
/// This is metadata only; the audio is not downloaded until the user
/// explicitly fetches the track (see YouTubeDownloader).
struct YouTubeTrack {
    QString videoId;
    QString title;
    QString channelTitle;
    QUrl thumbnailUrl;
    // Duration in seconds. 0 if unknown.
    int durationSecs = 0;
    // Whether the video is Creative Commons. Enforced by yt-dlp's license
    // match-filter (see youtubeCcLicenseMatchFilter), so search results are
    // already CC; kept for clarity/defensiveness.
    bool licenseIsCreativeCommons = true;

    QUrl watchUrl() const {
        return QUrl(QStringLiteral("https://www.youtube.com/watch?v=") + videoId);
    }

    bool isValid() const {
        return !videoId.isEmpty();
    }
};

/// yt-dlp `--match-filter` expression that keeps only Creative Commons content.
/// YouTube only offers the CC BY (Attribution) license, so this single string
/// is the complete CC gate. Used for both search and download so the license
/// is enforced by yt-dlp itself, with no API key and no separate re-check.
inline QString youtubeCcLicenseMatchFilter() {
    return QStringLiteral(
            "license=Creative Commons Attribution license (reuse allowed)");
}

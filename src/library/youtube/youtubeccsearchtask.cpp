#include "library/youtube/youtubeccsearchtask.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QtDebug>
#include <utility>

#include "moc_youtubeccsearchtask.cpp"

namespace {

const QString kApiBase = QStringLiteral("https://www.googleapis.com/youtube/v3/");

// Parse an ISO 8601 duration as returned by the YouTube Data API
// (e.g. "PT4M13S", "PT1H2M", "PT45S") into whole seconds.
int parseIso8601DurationSecs(const QString& iso) {
    static const QRegularExpression re(
            QStringLiteral("^PT(?:(\\d+)H)?(?:(\\d+)M)?(?:(\\d+)S)?$"));
    const QRegularExpressionMatch m = re.match(iso);
    if (!m.hasMatch()) {
        return 0;
    }
    const int hours = m.captured(1).toInt();
    const int minutes = m.captured(2).toInt();
    const int seconds = m.captured(3).toInt();
    return hours * 3600 + minutes * 60 + seconds;
}

// Try to extract a human-readable error message from a Google API JSON body.
QString apiErrorMessage(const QByteArray& body) {
    const QJsonObject obj = QJsonDocument::fromJson(body).object();
    const QJsonObject error = obj.value(QStringLiteral("error")).toObject();
    const QString message = error.value(QStringLiteral("message")).toString();
    return message;
}

} // anonymous namespace

YouTubeCcSearchTask::YouTubeCcSearchTask(QObject* parent)
        : QObject(parent) {
}

YouTubeCcSearchTask::~YouTubeCcSearchTask() {
    abort();
}

void YouTubeCcSearchTask::abort() {
    if (m_pReply) {
        m_pReply->abort();
        m_pReply->deleteLater();
        m_pReply = nullptr;
    }
}

void YouTubeCcSearchTask::search(const QString& query, int maxResults) {
    abort();
    if (m_apiKey.isEmpty()) {
        emit failed(tr("No YouTube Data API key configured."));
        return;
    }
    if (query.trimmed().isEmpty()) {
        emit succeeded(QList<YouTubeCcTrack>());
        return;
    }

    QUrl url(kApiBase + QStringLiteral("search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("part"), QStringLiteral("snippet"));
    q.addQueryItem(QStringLiteral("type"), QStringLiteral("video"));
    // The whole point: only Creative Commons licensed videos.
    q.addQueryItem(QStringLiteral("videoLicense"), QStringLiteral("creativeCommon"));
    // Category 10 is "Music". Narrows the noise; results stay CC either way.
    q.addQueryItem(QStringLiteral("videoCategoryId"), QStringLiteral("10"));
    q.addQueryItem(QStringLiteral("maxResults"), QString::number(maxResults));
    q.addQueryItem(QStringLiteral("q"), query.trimmed());
    q.addQueryItem(QStringLiteral("key"), m_apiKey);
    url.setQuery(q);

    m_pReply = m_network.get(QNetworkRequest(url));
    connect(m_pReply,
            &QNetworkReply::finished,
            this,
            &YouTubeCcSearchTask::onSearchReplyFinished);
}

void YouTubeCcSearchTask::onSearchReplyFinished() {
    if (!m_pReply) {
        return;
    }
    QNetworkReply* pReply = m_pReply;
    m_pReply = nullptr;
    pReply->deleteLater();

    const QByteArray body = pReply->readAll();
    if (pReply->error() != QNetworkReply::NoError) {
        QString message = apiErrorMessage(body);
        if (message.isEmpty()) {
            message = pReply->errorString();
        }
        emit failed(message);
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QJsonArray items = root.value(QStringLiteral("items")).toArray();

    QList<YouTubeCcTrack> tracks;
    tracks.reserve(items.size());
    for (const QJsonValue& itemVal : items) {
        const QJsonObject item = itemVal.toObject();
        const QString videoId =
                item.value(QStringLiteral("id")).toObject()
                        .value(QStringLiteral("videoId")).toString();
        if (videoId.isEmpty()) {
            continue;
        }
        const QJsonObject snippet = item.value(QStringLiteral("snippet")).toObject();
        YouTubeCcTrack track;
        track.videoId = videoId;
        track.title = snippet.value(QStringLiteral("title")).toString();
        track.channelTitle = snippet.value(QStringLiteral("channelTitle")).toString();
        const QJsonObject thumb = snippet.value(QStringLiteral("thumbnails")).toObject()
                                          .value(QStringLiteral("default")).toObject();
        track.thumbnailUrl = QUrl(thumb.value(QStringLiteral("url")).toString());
        tracks.append(track);
    }

    if (tracks.isEmpty()) {
        emit succeeded(tracks);
        return;
    }
    // Second call to fill in duration and re-verify the license.
    requestDetails(std::move(tracks));
}

void YouTubeCcSearchTask::requestDetails(QList<YouTubeCcTrack> tracks) {
    QStringList ids;
    ids.reserve(tracks.size());
    for (const YouTubeCcTrack& t : tracks) {
        ids.append(t.videoId);
    }

    QUrl url(kApiBase + QStringLiteral("videos"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("part"), QStringLiteral("contentDetails,status"));
    q.addQueryItem(QStringLiteral("id"), ids.join(QChar(',')));
    q.addQueryItem(QStringLiteral("key"), m_apiKey);
    url.setQuery(q);

    m_pendingTracks = std::move(tracks);
    m_pReply = m_network.get(QNetworkRequest(url));
    connect(m_pReply,
            &QNetworkReply::finished,
            this,
            &YouTubeCcSearchTask::onDetailsReplyFinished);
}

void YouTubeCcSearchTask::onDetailsReplyFinished() {
    if (!m_pReply) {
        return;
    }
    QNetworkReply* pReply = m_pReply;
    m_pReply = nullptr;
    pReply->deleteLater();

    QList<YouTubeCcTrack> tracks = std::move(m_pendingTracks);
    m_pendingTracks.clear();

    if (pReply->error() != QNetworkReply::NoError) {
        // The details call is best-effort. If it fails we still return the
        // search results (already known to be CC), just without durations.
        qWarning() << "YouTubeCcSearchTask: videos.list failed:"
                   << pReply->errorString();
        emit succeeded(tracks);
        return;
    }

    const QByteArray body = pReply->readAll();
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QJsonArray items = root.value(QStringLiteral("items")).toArray();

    QHash<QString, QJsonObject> byId;
    for (const QJsonValue& itemVal : items) {
        const QJsonObject item = itemVal.toObject();
        byId.insert(item.value(QStringLiteral("id")).toString(), item);
    }

    QList<YouTubeCcTrack> merged;
    merged.reserve(tracks.size());
    for (YouTubeCcTrack track : tracks) {
        const QJsonObject item = byId.value(track.videoId);
        const QString iso = item.value(QStringLiteral("contentDetails")).toObject()
                                    .value(QStringLiteral("duration")).toString();
        track.durationSecs = parseIso8601DurationSecs(iso);
        const QString license = item.value(QStringLiteral("status")).toObject()
                                        .value(QStringLiteral("license")).toString();
        // Defensive: only keep tracks the API confirms are Creative Commons.
        track.licenseIsCreativeCommons = (license == QStringLiteral("creativeCommon"));
        if (track.licenseIsCreativeCommons) {
            merged.append(track);
        }
    }
    emit succeeded(merged);
}

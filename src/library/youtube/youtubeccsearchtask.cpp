#include "library/youtube/youtubeccsearchtask.h"

#include <QProcess>
#include <QStringList>
#include <QUrl>
#include <QtDebug>

#include "moc_youtubeccsearchtask.cpp"

namespace {

// One line per video, tab-separated. Titles/channels do not contain tab
// characters, so this is safe to split on. Duration is whole seconds
// (or "NA" when unknown).
const QString kPrintTemplate =
        QStringLiteral("%(id)s\t%(title)s\t%(channel)s\t%(duration)s");

// YouTube's own "Creative Commons" search filter — the `sp=` code behind
// Filters -> Features -> Creative Commons. It restricts the results page to CC
// videos server-side, so a fast flat extraction is enough. We do NOT use a
// yt-dlp --match-filter on the license here, because the per-video `license`
// field is not populated during a search (it comes back "NA"); the download
// step still hard-gates on the license via a full extraction. This filter also
// makes the results page return videos only (not playlists/channels).
const QString kCreativeCommonsSpFilter = QStringLiteral("EgIwAQ%3D%3D");

QStringList buildArgs(const QString& query, int maxResults) {
    const QString encodedQuery = QString::fromUtf8(QUrl::toPercentEncoding(query));
    const QString url =
            QStringLiteral("https://www.youtube.com/results?search_query=") +
            encodedQuery +
            QStringLiteral("&sp=") + kCreativeCommonsSpFilter;
    return QStringList{
            url,
            // Flat: list the results page without extracting each video.
            QStringLiteral("--flat-playlist"),
            QStringLiteral("--playlist-items"),
            QStringLiteral("1:%1").arg(maxResults),
            QStringLiteral("--print"),
            kPrintTemplate,
            QStringLiteral("--ignore-errors"),
            QStringLiteral("--no-warnings"),
    };
}

} // anonymous namespace

YouTubeCcSearchTask::YouTubeCcSearchTask(QObject* parent)
        : QObject(parent),
          m_ytDlpPath(QStringLiteral("yt-dlp")) {
}

YouTubeCcSearchTask::~YouTubeCcSearchTask() {
    abort();
}

bool YouTubeCcSearchTask::isBusy() const {
    return m_pProcess != nullptr;
}

void YouTubeCcSearchTask::abort() {
    if (m_pProcess) {
        m_pProcess->disconnect(this);
        m_pProcess->kill();
        m_pProcess->deleteLater();
        m_pProcess = nullptr;
    }
}

void YouTubeCcSearchTask::search(const QString& query, int maxResults) {
    abort();
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        emit succeeded(QList<YouTubeCcTrack>());
        return;
    }

    m_pProcess = new QProcess(this);
    connect(m_pProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [this](int exitCode, QProcess::ExitStatus) { onProcessFinished(exitCode); });
    connect(m_pProcess,
            &QProcess::errorOccurred,
            this,
            &YouTubeCcSearchTask::onProcessErrorOccurred);
    m_pProcess->start(m_ytDlpPath, buildArgs(trimmed, maxResults));
}

void YouTubeCcSearchTask::onProcessFinished(int exitCode) {
    if (!m_pProcess) {
        return;
    }
    const QByteArray out = m_pProcess->readAllStandardOutput();
    const QString err = QString::fromUtf8(m_pProcess->readAllStandardError());
    m_pProcess->deleteLater();
    m_pProcess = nullptr;

    QList<YouTubeCcTrack> results;
    const QList<QByteArray> lines = out.split('\n');
    for (const QByteArray& rawLine : lines) {
        const QString line = QString::fromUtf8(rawLine).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QStringList fields = line.split(QChar('\t'));
        // A YouTube video id is exactly 11 characters. Skip anything else
        // (playlist/channel entries), which cannot be downloaded as a video.
        if (fields.size() < 2 || fields.at(0).length() != 11) {
            continue;
        }
        YouTubeCcTrack track;
        track.videoId = fields.at(0);
        track.title = fields.at(1);
        track.channelTitle = fields.value(2);
        track.durationSecs = static_cast<int>(fields.value(3).toDouble());
        results.append(track);
    }

    if (results.isEmpty() && exitCode != 0) {
        // Nothing parsed and yt-dlp failed: surface the error.
        QString message = err.trimmed();
        if (message.isEmpty()) {
            message = tr("yt-dlp exited with code %1.").arg(exitCode);
        }
        emit failed(message);
        return;
    }
    emit succeeded(results);
}

void YouTubeCcSearchTask::onProcessErrorOccurred() {
    if (!m_pProcess) {
        return;
    }
    const QProcess::ProcessError error = m_pProcess->error();
    m_pProcess->deleteLater();
    m_pProcess = nullptr;

    if (error == QProcess::FailedToStart) {
        emit failed(tr("Could not start yt-dlp (\"%1\"). Is it installed and on "
                       "your PATH?")
                            .arg(m_ytDlpPath));
    } else {
        emit failed(tr("yt-dlp process error while searching."));
    }
}

#include "library/youtube/youtubeccdownloader.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>
#include <QtDebug>

#include "moc_youtubeccdownloader.cpp"

namespace {

// yt-dlp arguments: audio-only, no playlist expansion, no re-encode (so no
// ffmpeg dependency), Windows-safe filenames, newline-separated progress.
// The output template embeds "[<id>]" so downloads can be found by videoId.
QStringList buildArgs(const QString& cacheDir, const YouTubeCcTrack& track) {
    return QStringList{
            QStringLiteral("--no-playlist"),
            QStringLiteral("--newline"),
            QStringLiteral("--windows-filenames"),
            QStringLiteral("-f"),
            QStringLiteral("bestaudio[ext=m4a]/bestaudio"),
            QStringLiteral("-o"),
            cacheDir + QStringLiteral("/%(title)s [%(id)s].%(ext)s"),
            track.watchUrl().toString(),
    };
}

} // anonymous namespace

YouTubeCcDownloader::YouTubeCcDownloader(QObject* parent)
        : QObject(parent),
          m_ytDlpPath(QStringLiteral("yt-dlp")) {
}

YouTubeCcDownloader::~YouTubeCcDownloader() {
    cancel();
}

bool YouTubeCcDownloader::isBusy() const {
    return m_pProcess != nullptr;
}

QString YouTubeCcDownloader::cachedPath(const QString& videoId) const {
    if (m_cacheDir.isEmpty() || videoId.isEmpty()) {
        return QString();
    }
    // Match by the "[<id>]" marker in the filename. We can't use QDir name
    // filters here because '[' and ']' are wildcard metacharacters.
    const QString marker = QStringLiteral("[") + videoId + QStringLiteral("]");
    const QFileInfoList entries =
            QDir(m_cacheDir).entryInfoList(QDir::Files, QDir::Time);
    for (const QFileInfo& info : entries) {
        if (info.fileName().contains(marker)) {
            return info.absoluteFilePath();
        }
    }
    return QString();
}

void YouTubeCcDownloader::download(const YouTubeCcTrack& track) {
    if (!track.isValid()) {
        emit failed(track.videoId, tr("Invalid track."));
        return;
    }
    if (isBusy()) {
        emit failed(track.videoId, tr("A download is already in progress."));
        return;
    }
    if (m_cacheDir.isEmpty()) {
        emit failed(track.videoId, tr("No download directory configured."));
        return;
    }

    // Already cached? Return it immediately.
    const QString cached = cachedPath(track.videoId);
    if (!cached.isEmpty()) {
        emit succeeded(track, cached);
        return;
    }

    QDir().mkpath(m_cacheDir);

    m_currentTrack = track;
    m_resolvedPath.clear();

    m_pProcess = new QProcess(this);
    m_pProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_pProcess,
            &QProcess::readyReadStandardOutput,
            this,
            &YouTubeCcDownloader::onReadyReadStandardOutput);
    connect(m_pProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [this](int exitCode, QProcess::ExitStatus) { onProcessFinished(exitCode); });
    connect(m_pProcess,
            &QProcess::errorOccurred,
            this,
            &YouTubeCcDownloader::onProcessErrorOccurred);

    emit progress(track.videoId, 0);
    m_pProcess->start(m_ytDlpPath, buildArgs(m_cacheDir, track));
}

void YouTubeCcDownloader::cancel() {
    if (m_pProcess) {
        m_pProcess->disconnect(this);
        m_pProcess->kill();
        m_pProcess->deleteLater();
        m_pProcess = nullptr;
    }
}

void YouTubeCcDownloader::onReadyReadStandardOutput() {
    if (!m_pProcess) {
        return;
    }
    static const QRegularExpression rePercent(
            QStringLiteral("(\\d{1,3}(?:\\.\\d+)?)%"));
    const QString out = QString::fromUtf8(m_pProcess->readAllStandardOutput());
    const QStringList lines = out.split(QChar('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const QRegularExpressionMatch m = rePercent.match(line);
        if (m.hasMatch()) {
            int pct = qBound(0, static_cast<int>(m.captured(1).toDouble()), 100);
            emit progress(m_currentTrack.videoId, pct);
        }
    }
}

void YouTubeCcDownloader::onProcessFinished(int exitCode) {
    if (!m_pProcess) {
        return;
    }
    m_pProcess->deleteLater();
    m_pProcess = nullptr;

    const YouTubeCcTrack track = m_currentTrack;
    if (exitCode != 0) {
        emit failed(track.videoId,
                tr("yt-dlp exited with code %1.").arg(exitCode));
        return;
    }

    const QString path = cachedPath(track.videoId);
    if (path.isEmpty()) {
        emit failed(track.videoId,
                tr("Download finished but the audio file was not found."));
        return;
    }
    emit progress(track.videoId, 100);
    emit succeeded(track, path);
}

void YouTubeCcDownloader::onProcessErrorOccurred() {
    if (!m_pProcess) {
        return;
    }
    const QProcess::ProcessError err = m_pProcess->error();
    // FailedToStart is the common one: yt-dlp not installed / not on PATH.
    const QString videoId = m_currentTrack.videoId;
    m_pProcess->deleteLater();
    m_pProcess = nullptr;

    if (err == QProcess::FailedToStart) {
        emit failed(videoId,
                tr("Could not start yt-dlp (\"%1\"). Is it installed and on "
                   "your PATH?")
                        .arg(m_ytDlpPath));
    } else {
        emit failed(videoId, tr("yt-dlp process error."));
    }
}

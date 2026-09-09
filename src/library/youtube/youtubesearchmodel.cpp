#include "library/youtube/youtubesearchmodel.h"

#include <QDir>
#include <QFileInfo>

#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/youtube/youtubesearchtask.h"
#include "moc_youtubesearchmodel.cpp"
#include "track/track.h"

namespace {
const ConfigKey kYtDlpPathConfigKey = ConfigKey("[youtube]", "ytdlp_path");

QString formatDuration(int secs) {
    if (secs <= 0) {
        return QStringLiteral("--:--");
    }
    return QStringLiteral("%1:%2")
            .arg(secs / 60)
            .arg(secs % 60, 2, 10, QChar('0'));
}

QString ytDlpPath(const UserSettingsPointer& pConfig) {
    const QString path = pConfig->getValueString(kYtDlpPathConfigKey);
    return path.isEmpty() ? QStringLiteral("yt-dlp") : path;
}

QString cacheDir(const UserSettingsPointer& pConfig) {
    return pConfig->getSettingsPath() + QStringLiteral("/youtube_cache");
}
} // anonymous namespace

YouTubeSearchModel::YouTubeSearchModel(QObject* parent,
        TrackCollectionManager* pTrackCollectionManager,
        UserSettingsPointer pConfig)
        : TrackModel(pTrackCollectionManager->internalCollection()->database(),
                  "mixxx.db.model.youtube.search"),
          QStandardItemModel(parent),
          m_pTrackCollectionManager(pTrackCollectionManager),
          m_pConfig(std::move(pConfig)),
          m_pSearchTask(new YouTubeSearchTask(this)) {
    setHorizontalHeaderLabels({tr("Title"), tr("Channel"), tr("Duration")});
    connect(m_pSearchTask,
            &YouTubeSearchTask::succeeded,
            this,
            &YouTubeSearchModel::slotSearchSucceeded);
    connect(m_pSearchTask,
            &YouTubeSearchTask::failed,
            this,
            &YouTubeSearchModel::slotSearchFailed);
}

YouTubeTrack YouTubeSearchModel::resultAt(int row) const {
    if (row < 0 || row >= m_results.size()) {
        return YouTubeTrack();
    }
    return m_results.at(row);
}

void YouTubeSearchModel::setResults(const QList<YouTubeTrack>& results) {
    m_results = results;
    removeRows(0, rowCount());
    for (const auto& track : results) {
        QList<QStandardItem*> row;
        // Rows are informational until downloaded; nothing here is editable.
        for (const auto& text : {track.title,
                     track.channelTitle,
                     formatDuration(track.durationSecs)}) {
            auto* pItem = new QStandardItem(text);
            pItem->setEditable(false);
            row.append(pItem);
        }
        appendRow(row);
    }
}

void YouTubeSearchModel::search(const QString& searchText) {
    const QString query = searchText.trimmed();
    if (query == m_currentSearch) {
        return;
    }
    m_currentSearch = query;
    if (query.isEmpty()) {
        m_results.clear();
        removeRows(0, rowCount());
        emit statusChanged(QString());
        return;
    }
    emit statusChanged(tr("Searching YouTube for \"%1\"…").arg(query));
    m_pSearchTask->setYtDlpPath(ytDlpPath(m_pConfig));
    m_pSearchTask->search(query);
}

const QString YouTubeSearchModel::currentSearch() const {
    return m_currentSearch;
}

void YouTubeSearchModel::slotSearchSucceeded(const QList<YouTubeTrack>& results) {
    setResults(results);
    if (results.isEmpty()) {
        emit statusChanged(tr("No Creative Commons results found."));
    } else {
        emit statusChanged(tr("%n Creative Commons result(s)", "", results.size()));
    }
}

void YouTubeSearchModel::slotSearchFailed(const QString& message) {
    setResults({});
    emit statusChanged(message);
}

QString YouTubeSearchModel::cachedPathFor(const YouTubeTrack& track) const {
    if (!track.isValid()) {
        return QString();
    }
    // Mirrors YouTubeDownloader's cache layout: files are named by videoId,
    // with whatever extension yt-dlp chose for the best audio stream.
    const QDir dir(cacheDir(m_pConfig));
    const QStringList matches =
            dir.entryList({track.videoId + QStringLiteral(".*")}, QDir::Files);
    if (matches.isEmpty()) {
        return QString();
    }
    return dir.absoluteFilePath(matches.first());
}

QString YouTubeSearchModel::getTrackLocation(const QModelIndex& index) const {
    return cachedPathFor(resultAt(index.row()));
}

TrackPointer YouTubeSearchModel::getTrack(const QModelIndex& index) const {
    const QString location = getTrackLocation(index);
    if (location.isEmpty()) {
        // Not downloaded yet: loading goes through requestDeferredLoad().
        return TrackPointer();
    }
    return getTrackByRef(TrackRef::fromFilePath(location));
}

TrackPointer YouTubeSearchModel::getTrackByRef(const TrackRef& trackRef) const {
    // Downloaded files are added to the library so they get analyzed and
    // behave like any other track from then on.
    return m_pTrackCollectionManager->getOrAddTrack(trackRef);
}

QUrl YouTubeSearchModel::getTrackUrl(const QModelIndex& index) const {
    const QString location = getTrackLocation(index);
    if (location.isEmpty()) {
        // Nothing local yet - the watch page is the only meaningful URL.
        return resultAt(index.row()).watchUrl();
    }
    return QUrl::fromLocalFile(location);
}

TrackId YouTubeSearchModel::getTrackId(const QModelIndex& index) const {
    const TrackPointer pTrack = getTrack(index);
    return pTrack ? pTrack->getId() : TrackId();
}

CoverInfo YouTubeSearchModel::getCoverInfo(const QModelIndex& index) const {
    const TrackPointer pTrack = getTrack(index);
    if (!pTrack) {
        return CoverInfo();
    }
    return CoverInfo(pTrack->getCoverInfo(), getTrackLocation(index));
}

const QVector<int> YouTubeSearchModel::getTrackRows(TrackId trackId) const {
    QVector<int> rows;
    for (int row = 0; row < m_results.size(); ++row) {
        const QString location = cachedPathFor(m_results.at(row));
        if (location.isEmpty()) {
            continue;
        }
        const TrackPointer pTrack =
                m_pTrackCollectionManager->getTrackByRef(
                        TrackRef::fromFilePath(location));
        if (pTrack && pTrack->getId() == trackId) {
            rows.append(row);
        }
    }
    return rows;
}

bool YouTubeSearchModel::requestDeferredLoad(const QModelIndex& index,
        const QString& group,
        bool play) {
    const YouTubeTrack track = resultAt(index.row());
    if (!track.isValid()) {
        return false;
    }
    if (!getTrackLocation(index).isEmpty()) {
        // Already downloaded - let the normal synchronous load path run.
        return false;
    }
    emit deferredLoadRequested(track, group, play);
    return true;
}

bool YouTubeSearchModel::isColumnInternal(int column) {
    Q_UNUSED(column);
    return false;
}

bool YouTubeSearchModel::isColumnHiddenByDefault(int column) {
    Q_UNUSED(column);
    return false;
}

TrackModel::SortColumnId YouTubeSearchModel::sortColumnIdFromColumnIndex(int index) const {
    switch (index) {
    case ColumnTitle:
        return SortColumnId::Title;
    case ColumnChannel:
        return SortColumnId::Artist;
    case ColumnDuration:
        return SortColumnId::Duration;
    default:
        return SortColumnId::Invalid;
    }
}

int YouTubeSearchModel::columnIndexFromSortColumnId(SortColumnId sortColumn) const {
    switch (sortColumn) {
    case SortColumnId::Title:
        return ColumnTitle;
    case SortColumnId::Artist:
        return ColumnChannel;
    case SortColumnId::Duration:
        return ColumnDuration;
    default:
        return -1;
    }
}

QString YouTubeSearchModel::rowAccessibleText(const QModelIndex& index) const {
    const YouTubeTrack track = resultAt(index.row());
    if (!track.isValid()) {
        return QString();
    }
    // Spoken, not the column text: the duration reads as words rather than
    // "3:42", and the channel is named so results are distinguishable by ear.
    QString spoken = track.title;
    if (!track.channelTitle.isEmpty()) {
        spoken += tr(", by %1").arg(track.channelTitle);
    }
    if (track.durationSecs > 0) {
        const int minutes = track.durationSecs / 60;
        const int seconds = track.durationSecs % 60;
        if (minutes > 0) {
            spoken += tr(", %n minute(s)", "", minutes);
            if (seconds > 0) {
                spoken += tr(" %n second(s)", "", seconds);
            }
        } else {
            spoken += tr(", %n second(s)", "", seconds);
        }
    }
    if (!getTrackLocation(index).isEmpty()) {
        spoken += tr(", downloaded");
    }
    return spoken;
}

QString YouTubeSearchModel::modelKey(bool noSearch) const {
    if (noSearch) {
        return QStringLiteral("youtube:search");
    }
    return QStringLiteral("youtube:search:") + m_currentSearch;
}

TrackModel::Capabilities YouTubeSearchModel::getCapabilities() const {
    // Deliberately narrow: a result is not a library track until it has been
    // downloaded, so library actions (hide, edit metadata, add to playlist)
    // have nothing to act on. Loading is what a result is for, and sorting is
    // free because the rows are already in memory.
    return Capability::LoadToDeck |
            Capability::LoadToSampler |
            Capability::LoadToPreviewDeck |
            Capability::Sorting;
}

bool YouTubeSearchModel::updateTrackGenre(Track* pTrack, const QString& genre) const {
    // Only reachable once a result has been downloaded and is a library track.
    return m_pTrackCollectionManager->updateTrackGenre(pTrack, genre);
}

Qt::ItemFlags YouTubeSearchModel::flags(const QModelIndex& index) const {
    // Dragging a result to a deck should behave like activating it, and drag
    // hands over a file path we do not have until the download finishes.
    return QAbstractItemModel::flags(index) & ~Qt::ItemIsEditable;
}

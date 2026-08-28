#include "library/youtube/youtubefeature.h"

#include "control/controlobject.h"
#include "controllers/keyboard/keyboardeventfilter.h"
#include "library/library.h"
#include "library/trackcollectionmanager.h"
#include "library/treeitem.h"
#include "library/youtube/youtubedownloader.h"
#include "library/youtube/youtubesearchmodel.h"
#include "library/youtube/youtubetrackmodel.h"
#include "moc_youtubefeature.cpp"
#include "track/track.h"
#include "widget/wlibrary.h"

namespace {
const ConfigKey kYtDlpPathConfigKey = ConfigKey("[youtube]", "ytdlp_path");
const QString kDownloadedNodeData = QStringLiteral("downloaded");
// Matches BaseTrackPlayerImpl: negative means nothing is being fetched.
constexpr double kNoDownloadProgress = -1;

QString ytDlpPath(const UserSettingsPointer& pConfig) {
    const QString path = pConfig->getValueString(kYtDlpPathConfigKey);
    return path.isEmpty() ? QStringLiteral("yt-dlp") : path;
}
} // anonymous namespace

YouTubeFeature::YouTubeFeature(Library* pLibrary, UserSettingsPointer pConfig)
        : LibraryFeature(pLibrary, pConfig, QStringLiteral("computer")),
          m_pSidebarModel(make_parented<TreeItemModel>(this)),
          m_pSearchModel(nullptr),
          m_pDownloadedModel(nullptr),
          m_pDownloader(new YouTubeDownloader(this)),
          m_title(tr("YouTube")) {
    // Sidebar: root ("YouTube") with a single "Downloaded" child.
    auto pRootItem = TreeItem::newRoot(this);
    pRootItem->appendChild(tr("Downloaded"), kDownloadedNodeData);
    m_pSidebarModel->setRootItem(std::move(pRootItem));

    // Search results: a native track table fed by the main search bar.
    m_pSearchModel = new YouTubeSearchModel(this,
            m_pLibrary->trackCollectionManager(),
            m_pConfig);
    connect(m_pSearchModel,
            &YouTubeSearchModel::deferredLoadRequested,
            this,
            &YouTubeFeature::slotDeferredLoadRequested);
    // Searching runs yt-dlp and takes a moment, so say what is happening
    // rather than leaving silence; the result count follows.
    connect(m_pSearchModel,
            &YouTubeSearchModel::statusChanged,
            this,
            [this](const QString& message) {
                if (!message.isEmpty()) {
                    m_pLibrary->announceText(message);
                }
            });

    // Native track table of downloaded tracks (files in the cache directory).
    m_pDownloadedModel = new YouTubeTrackModel(this,
            m_pLibrary->trackCollectionManager(),
            cacheDir());

    connect(m_pDownloader,
            &YouTubeDownloader::progress,
            this,
            &YouTubeFeature::slotDownloadProgress);
    connect(m_pDownloader,
            &YouTubeDownloader::succeeded,
            this,
            &YouTubeFeature::slotDownloadSucceeded);
    connect(m_pDownloader,
            &YouTubeDownloader::failed,
            this,
            &YouTubeFeature::slotDownloadFailed);
}

QString YouTubeFeature::cacheDir() const {
    return m_pConfig->getSettingsPath() + QStringLiteral("/youtube_cache");
}

QVariant YouTubeFeature::title() {
    return m_title;
}

TreeItemModel* YouTubeFeature::sidebarModel() const {
    return m_pSidebarModel;
}

void YouTubeFeature::bindLibraryWidget(WLibrary* libraryWidget,
        KeyboardEventFilter* keyboard) {
    // Both nodes use the shared track table, so there is no view of our own to
    // register or to install a keyboard filter on.
    Q_UNUSED(libraryWidget);
    Q_UNUSED(keyboard);
}

void YouTubeFeature::activate() {
    // Root node: search results, driven by the main search bar.
    emit showTrackModel(m_pSearchModel);
    emit enableCoverArtDisplay(false);
    m_pLibrary->announceText(
            tr("YouTube search. Type in the search box to find tracks."));
}

void YouTubeFeature::activateChild(const QModelIndex& index) {
    TreeItem* pItem = static_cast<TreeItem*>(index.internalPointer());
    if (!pItem) {
        return;
    }
    if (pItem->getData().toString() == kDownloadedNodeData) {
        // Refresh in case new tracks were downloaded, then show the native
        // track table (filtered by the main search bar automatically).
        m_pDownloadedModel->select();
        emit showTrackModel(m_pDownloadedModel);
        emit enableCoverArtDisplay(true);
    }
}

void YouTubeFeature::slotDeferredLoadRequested(const YouTubeTrack& track,
        const QString& group,
        bool play) {
    if (m_pDownloader->isBusy()) {
        return;
    }
    m_pendingLoadGroup = group;
    m_pendingLoadPlay = play;
    m_lastAnnouncedDownloadMilestone = 0;
    // Show the fetch on the deck it is destined for, from 0 so the indicator
    // appears immediately rather than at the first progress line from yt-dlp.
    setDownloadProgress(0.0);
    m_pLibrary->announceText(tr("Downloading %1").arg(track.title));
    m_pDownloader->setYtDlpPath(ytDlpPath(m_pConfig));
    m_pDownloader->setCacheDir(cacheDir());
    m_pDownloader->download(track);
}

void YouTubeFeature::setDownloadProgress(double progress) {
    if (m_pendingLoadGroup.isEmpty()) {
        return;
    }
    ControlObject::set(
            ConfigKey(m_pendingLoadGroup, QStringLiteral("download_progress")),
            progress);
}

void YouTubeFeature::clearPendingLoad() {
    // Take the indicator off the deck before forgetting which deck it was on.
    setDownloadProgress(kNoDownloadProgress);
    m_pendingLoadGroup.clear();
    m_pendingLoadPlay = false;
    m_lastAnnouncedDownloadMilestone = 0;
}

void YouTubeFeature::slotDownloadProgress(const QString& videoId, int percent) {
    Q_UNUSED(videoId);
    setDownloadProgress(percent / 100.0);

    // The waveform bar is visual-only. Speak progress at 25% steps so a
    // longer fetch doesn't look (and sound) like nothing is happening,
    // without narrating every single percent yt-dlp reports.
    const int milestone = (percent / 25) * 25;
    if (milestone > m_lastAnnouncedDownloadMilestone && milestone < 100) {
        m_lastAnnouncedDownloadMilestone = milestone;
        m_pLibrary->announceText(tr("%1%").arg(milestone));
    }
}

void YouTubeFeature::slotDownloadSucceeded(const YouTubeTrack& track,
        const QString& localPath) {
    TrackCollectionManager* pTcm = m_pLibrary->trackCollectionManager();
    const QList<TrackId> ids = pTcm->resolveTrackIdsFromLocations({localPath});
    if (ids.isEmpty()) {
        clearPendingLoad();
        return;
    }
    TrackPointer pTrack = pTcm->getTrackById(ids.first());
    if (!pTrack) {
        clearPendingLoad();
        return;
    }

    // Fill in metadata from the YouTube result if the file itself carried none,
    // and always record attribution (Creative Commons requires it).
    if (pTrack->getArtist().trimmed().isEmpty()) {
        pTrack->setArtist(track.channelTitle);
    }
    if (pTrack->getTitle().trimmed().isEmpty()) {
        pTrack->setTitle(track.title);
    }
    pTrack->setComment(
            tr("Source: %1 | Uploader: %2 | License: Creative Commons (CC BY)")
                    .arg(track.watchUrl().toString(), track.channelTitle));

    // The Downloaded node gains a row.
    m_pDownloadedModel->select();

    if (m_pendingLoadGroup.isEmpty()) {
        emit loadTrack(pTrack);
    } else {
        emit loadTrackToPlayer(pTrack,
                m_pendingLoadGroup,
#ifdef __STEM__
                mixxx::StemChannelSelection(),
#endif
                m_pendingLoadPlay);
    }
    clearPendingLoad();
}

void YouTubeFeature::slotDownloadFailed(const QString& videoId, const QString& message) {
    Q_UNUSED(videoId);
    // A failed download is otherwise completely silent: the waveform
    // indicator just disappears.
    m_pLibrary->announceText(tr("Download failed: %1").arg(message));
    clearPendingLoad();
}

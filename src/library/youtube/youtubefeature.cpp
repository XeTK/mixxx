#include "library/youtube/youtubefeature.h"

#include "controllers/keyboard/keyboardeventfilter.h"
#include "library/library.h"
#include "library/trackcollectionmanager.h"
#include "library/treeitem.h"
#include "library/youtube/dlgyoutube.h"
#include "library/youtube/youtubetrackmodel.h"
#include "moc_youtubefeature.cpp"
#include "widget/wlibrary.h"

namespace {
const QString kSearchViewName = QStringLiteral("YouTubeCCSearch");
const QString kDownloadedNodeData = QStringLiteral("downloaded");
} // anonymous namespace

YouTubeFeature::YouTubeFeature(Library* pLibrary, UserSettingsPointer pConfig)
        : LibraryFeature(pLibrary, pConfig, QStringLiteral("computer")),
          m_pSidebarModel(make_parented<TreeItemModel>(this)),
          m_pSearchView(nullptr),
          m_pDownloadedModel(nullptr),
          m_title(tr("YouTube (CC)")) {
    // Sidebar: root ("YouTube (CC)") with a single "Downloaded" child.
    auto pRootItem = TreeItem::newRoot(this);
    pRootItem->appendChild(tr("Downloaded"), kDownloadedNodeData);
    m_pSidebarModel->setRootItem(std::move(pRootItem));

    // Native track table of downloaded tracks (files in the cache directory).
    const QString cacheDir =
            m_pConfig->getSettingsPath() + QStringLiteral("/youtube_cache");
    m_pDownloadedModel = new YouTubeTrackModel(this,
            m_pLibrary->trackCollectionManager(),
            cacheDir);
}

QVariant YouTubeFeature::title() {
    return m_title;
}

TreeItemModel* YouTubeFeature::sidebarModel() const {
    return m_pSidebarModel;
}

void YouTubeFeature::bindLibraryWidget(WLibrary* libraryWidget,
        KeyboardEventFilter* keyboard) {
    m_pSearchView = new DlgYouTube(libraryWidget, m_pConfig, m_pLibrary);
    connect(m_pSearchView,
            &DlgYouTube::loadTrack,
            this,
            &YouTubeFeature::loadTrack);
    connect(m_pSearchView,
            &DlgYouTube::loadTrackToPlayer,
            this,
            [this](TrackPointer pTrack, const QString& group, bool play) {
                emit loadTrackToPlayer(pTrack,
                        group,
#ifdef __STEM__
                        mixxx::StemChannelSelection(),
#endif
                        play);
            });
    connect(m_pSearchView,
            &DlgYouTube::trackSelected,
            this,
            &YouTubeFeature::trackSelected);
    connect(m_pSearchView,
            &DlgYouTube::downloaded,
            this,
            &YouTubeFeature::slotDownloaded);
    m_pSearchView->installEventFilter(keyboard);
    m_pSearchView->installKeyboardFilter(keyboard);
    libraryWidget->registerView(kSearchViewName, m_pSearchView);
}

void YouTubeFeature::activate() {
    // Root node: show the search view, driven by the main search bar.
    emit switchToView(kSearchViewName);
    emit restoreSearch(QString());
    emit enableCoverArtDisplay(false);
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

void YouTubeFeature::slotDownloaded() {
    m_pDownloadedModel->select();
}

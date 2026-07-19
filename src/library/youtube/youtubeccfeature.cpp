#include "library/youtube/youtubeccfeature.h"

#include "controllers/keyboard/keyboardeventfilter.h"
#include "library/library.h"
#include "library/trackcollectionmanager.h"
#include "library/treeitem.h"
#include "library/youtube/dlgyoutubecc.h"
#include "library/youtube/youtubecctrackmodel.h"
#include "moc_youtubeccfeature.cpp"
#include "widget/wlibrary.h"

namespace {
const QString kSearchViewName = QStringLiteral("YouTubeCCSearch");
const QString kDownloadedNodeData = QStringLiteral("downloaded");
} // anonymous namespace

YouTubeCcFeature::YouTubeCcFeature(Library* pLibrary, UserSettingsPointer pConfig)
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
            m_pConfig->getSettingsPath() + QStringLiteral("/youtube_cc_cache");
    m_pDownloadedModel = new YouTubeCcTrackModel(this,
            m_pLibrary->trackCollectionManager(),
            cacheDir);
}

QVariant YouTubeCcFeature::title() {
    return m_title;
}

TreeItemModel* YouTubeCcFeature::sidebarModel() const {
    return m_pSidebarModel;
}

void YouTubeCcFeature::bindLibraryWidget(WLibrary* libraryWidget,
        KeyboardEventFilter* keyboard) {
    m_pSearchView = new DlgYouTubeCc(libraryWidget, m_pConfig, m_pLibrary);
    connect(m_pSearchView,
            &DlgYouTubeCc::loadTrack,
            this,
            &YouTubeCcFeature::loadTrack);
    connect(m_pSearchView,
            &DlgYouTubeCc::trackSelected,
            this,
            &YouTubeCcFeature::trackSelected);
    connect(m_pSearchView,
            &DlgYouTubeCc::downloaded,
            this,
            &YouTubeCcFeature::slotDownloaded);
    m_pSearchView->installEventFilter(keyboard);
    libraryWidget->registerView(kSearchViewName, m_pSearchView);
}

void YouTubeCcFeature::activate() {
    // Root node: show the search view, driven by the main search bar.
    emit switchToView(kSearchViewName);
    emit restoreSearch(QString());
    emit enableCoverArtDisplay(false);
}

void YouTubeCcFeature::activateChild(const QModelIndex& index) {
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

void YouTubeCcFeature::slotDownloaded() {
    m_pDownloadedModel->select();
}

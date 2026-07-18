#include "library/youtube/youtubeccfeature.h"

#include "controllers/keyboard/keyboardeventfilter.h"
#include "library/youtube/dlgyoutubecc.h"
#include "moc_youtubeccfeature.cpp"
#include "widget/wlibrary.h"

namespace {
const QString kViewName = QStringLiteral("YouTubeCC");
} // anonymous namespace

YouTubeCcFeature::YouTubeCcFeature(Library* pLibrary, UserSettingsPointer pConfig)
        : LibraryFeature(pLibrary, pConfig, QStringLiteral("computer")),
          m_pSidebarModel(make_parented<TreeItemModel>(this)),
          m_pView(nullptr),
          m_title(tr("YouTube (CC)")) {
}

QVariant YouTubeCcFeature::title() {
    return m_title;
}

TreeItemModel* YouTubeCcFeature::sidebarModel() const {
    return m_pSidebarModel;
}

void YouTubeCcFeature::bindLibraryWidget(WLibrary* libraryWidget,
        KeyboardEventFilter* keyboard) {
    m_pView = new DlgYouTubeCc(libraryWidget, m_pConfig, m_pLibrary);
    connect(m_pView,
            &DlgYouTubeCc::loadTrack,
            this,
            &YouTubeCcFeature::loadTrack);
    connect(m_pView,
            &DlgYouTubeCc::trackSelected,
            this,
            &YouTubeCcFeature::trackSelected);
    m_pView->installEventFilter(keyboard);
    libraryWidget->registerView(kViewName, m_pView);
}

void YouTubeCcFeature::activate() {
    emit switchToView(kViewName);
    emit enableCoverArtDisplay(true);
}

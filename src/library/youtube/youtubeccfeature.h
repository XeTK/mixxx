#pragma once

#include <QModelIndex>
#include <QVariant>

#include "library/libraryfeature.h"
#include "library/treeitemmodel.h"
#include "preferences/usersettings.h"
#include "util/parented_ptr.h"

class DlgYouTubeCc;
class YouTubeCcTrackModel;
class Library;
class WLibrary;
class KeyboardEventFilter;

/// Sidebar feature for searching Creative Commons music on YouTube and loading
/// it into Mixxx. It follows the standard Mixxx feature layout: the root node
/// shows a search view (driven by the main library search bar), and a
/// "Downloaded" child node shows a native track table of previously fetched
/// tracks.
class YouTubeCcFeature : public LibraryFeature {
    Q_OBJECT
  public:
    YouTubeCcFeature(Library* pLibrary, UserSettingsPointer pConfig);
    ~YouTubeCcFeature() override = default;

    QVariant title() override;
    void bindLibraryWidget(WLibrary* libraryWidget,
            KeyboardEventFilter* keyboard) override;
    TreeItemModel* sidebarModel() const override;

  public slots:
    void activate() override;
    void activateChild(const QModelIndex& index) override;

  private slots:
    void slotDownloaded();

  private:
    parented_ptr<TreeItemModel> m_pSidebarModel;
    DlgYouTubeCc* m_pSearchView;
    YouTubeCcTrackModel* m_pDownloadedModel;
    const QString m_title;
};

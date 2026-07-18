#pragma once

#include <QVariant>

#include "library/libraryfeature.h"
#include "library/treeitemmodel.h"
#include "preferences/usersettings.h"
#include "util/parented_ptr.h"

class DlgYouTubeCc;
class Library;
class WLibrary;
class KeyboardEventFilter;

/// Sidebar feature that lets the user search Creative Commons music on YouTube
/// and load it into Mixxx. The heavy lifting lives in the registered view
/// (DlgYouTubeCc); this class just wires it into the library sidebar, mirroring
/// AnalysisFeature.
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

  private:
    parented_ptr<TreeItemModel> m_pSidebarModel;
    DlgYouTubeCc* m_pView;
    const QString m_title;
};

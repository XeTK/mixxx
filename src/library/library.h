#pragma once

#include <QFont>
#include <QList>
#include <QObject>
#include <QPointer>

#include "analyzer/trackanalysisscheduler.h"
#include "library/library_decl.h"
#ifdef __ENGINEPRIME__
#include "library/trackset/crate/crateid.h"
#endif
#include "preferences/usersettings.h"
#include "track/track_decl.h"
#include "util/db/dbconnectionpool.h"
#include "util/parented_ptr.h"

class AnalysisFeature;
class AutoDJFeature;
class AutoDJProcessor;
class BrowseFeature;
class ControlObject;
class CrateFeature;
class LibraryControl;
class LibraryFeature;
class LibraryTableModel;
class KeyboardEventFilter;
class MixxxLibraryFeature;
class PlayerManager;
class PlaylistFeature;
class RecordingManager;
class SidebarModel;
class TrackCollectionManager;
class WSearchLineEdit;
class WLibrarySidebar;
class WLibrary;
class QAbstractItemModel;
class QAction;
class QMenu;

#ifdef __ENGINEPRIME__
namespace mixxx {
class LibraryExporter;
} // namespace mixxx
#endif

// A Library class is a container for all the model-side aspects of the library.
// A library widget can be attached to the Library object by calling bindLibraryWidget.
class Library: public QObject {
    Q_OBJECT

  public:
    Library(QObject* parent,
            UserSettingsPointer pConfig,
            mixxx::DbConnectionPoolPtr pDbConnectionPool,
            TrackCollectionManager* pTrackCollectionManager,
            PlayerManager* pPlayerManager,
            RecordingManager* pRecordingManager);
    ~Library() override;

    void stopPendingTasks();

    const mixxx::DbConnectionPoolPtr& dbConnectionPool() const {
        return m_pDbConnectionPool;
    }

    TrackCollectionManager* trackCollectionManager() const;

    TrackAnalysisScheduler::Pointer createTrackAnalysisScheduler(
            int numWorkerThreads,
            AnalyzerModeFlags modeFlags) const;

    void bindSearchboxWidget(WSearchLineEdit* pSearchboxWidget);
    void bindSidebarWidget(WLibrarySidebar* sidebarWidget);
    void bindLibraryWidget(WLibrary* libraryWidget,
                    KeyboardEventFilter* pKeyboard);

    void addFeature(LibraryFeature* feature);

    /// Needed for exposing models to QML
    LibraryTableModel* trackTableModel() const;

    bool isTrackIdInCurrentLibraryView(const TrackId& trackId);

    int getTrackTableRowHeight() const {
        return m_iTrackTableRowHeight;
    }

    const QFont& getTrackTableFont() const {
        return m_trackTableFont;
    }

    bool selectedClickEnabled() const {
        return m_editMetadataSelectedClick;
    }

    //static Library* buildDefaultLibrary();

    static const int kDefaultRowHeightPx;

    void setFont(const QFont& font);
    void setRowHeight(int rowHeight);
    void setEditMetadataSelectedClick(bool enable);

    /// Switches to the internal track collection view
    /// and focuses the search box.
    void searchTracksInCollection();

    /// Triggers a new search in the internal track collection
    /// and shows the results by switching the view.
    void searchTracksInCollection(const QString& query);
    void showAutoDJ();

    static const QString kAutoDJViewName;

    bool requestAddDir(const QString& directory);
    bool requestRemoveDir(const QString& directory, LibraryRemovalType removalType);
    bool requestRelocateDir(const QString& previousDirectory, const QString& newDirectory);

#ifdef __ENGINEPRIME__
    std::unique_ptr<mixxx::LibraryExporter> makeLibraryExporter(QWidget* parent);
#endif

    /// Speaks `text` through the accessibility TTS system, with an optional
    /// list position ("3 of 12"). For on-demand pickers (e.g. quick add to
    /// crate/playlist) that aren't otherwise wired into Library's signals.
    void announceQuickPickerItem(const QString& text, int row = -1, int siblingCount = 0);

    /// Speaks `text` through the accessibility TTS system. For one-off
    /// events with no natural position, e.g. a create/rename dialog opening.
    void announceText(const QString& text);

    /// Reports how many tracks the just-applied library search matched, so
    /// the spoken search announcement can include the count. Called by the
    /// track table right after it applies a search.
    void announceSearchResultCount(int count);

    /// Non-owning access to the Auto DJ processor, for the accessibility
    /// layer's "what's next in Auto DJ" on-demand readout. Null only if
    /// AutoDJFeature somehow failed to construct.
    AutoDJProcessor* getAutoDJProcessor() const;

    /// Wires up TTS announcements for a context menu built outside
    /// WTrackTableView's quick-add picker (e.g. WTrackMenu's submenus or the
    /// library sidebar's playlist/crate right-click menu), so the fork's
    /// built-in TTS speaks each item as the user arrows through it with the
    /// keyboard, matching the quick-add picker's behavior. QMenu::hovered is
    /// per-menu-instance -- it does not bubble up from a submenu to its
    /// parent -- so call this once for every QMenu/QMenu-subclass instance
    /// that owns actions the user can hover, not just the top-level menu.
    /// The connection to `pMenu` is torn down automatically when `pMenu` is
    /// destroyed, so this is safe to call on a QMenu that's exec()'d off the
    /// stack and then dropped.
    void announceMenuHover(QMenu* pMenu);

    /// Extracted from announceMenuHover() for testability without needing a
    /// live Library/QMenu/hovered() signal. Returns the text that should be
    /// spoken for a hovered action, or an empty string if there's nothing
    /// sensible to say (e.g. a null action, or a QWidgetAction with neither
    /// data() nor text() set). Prefers data() over text(): dynamically named
    /// items (e.g. playlist/crate names) store their raw, unescaped name in
    /// data() because text() may hold a doubled "&&" that escapes it against
    /// QAction's mnemonic handling.
    static QString hoverAnnouncementTextForAction(const QAction* pAction);

  public slots:
    void slotShowTrackModel(QAbstractItemModel* model);
    void slotSwitchToView(const QString& view);
    void slotLoadTrack(TrackPointer pTrack);
#ifdef __STEM__
    void slotLoadTrackToPlayer(TrackPointer pTrack,
            const QString& group,
            mixxx::StemChannelSelection stemMask,
            bool play);
#else
    void slotLoadTrackToPlayer(TrackPointer pTrack, const QString& group, bool play);
#endif
    void slotLoadLocationToPlayer(const QString& location, const QString& group, bool play);
    void slotRefreshLibraryModels();
    void slotCreatePlaylist();
    void slotCreateCrate();
    void slotSearchInCurrentView();
    void slotSearchInAllTracks();
    void onSkinLoadFinished();
    void slotSaveCurrentViewState() const;
    void slotRestoreCurrentViewState() const;

  signals:
    void showTrackModel(QAbstractItemModel* model, bool restoreState = true);
    void switchToView(const QString& view);
    void loadTrack(TrackPointer pTrack);
#ifdef __STEM__
    void loadTrackToPlayer(TrackPointer pTrack,
            const QString& group,
            mixxx::StemChannelSelection stemMask,
            bool play = false);
#else
    void loadTrackToPlayer(TrackPointer pTrack,
            const QString& group,
            bool play = false);
#endif
    void restoreSearch(const QString&);
    void search(const QString& text);
    void disableSearch();
    void pasteFromSidebar();
    // emit this signal to enable/disable the cover art widget
    void enableCoverArtDisplay(bool);
    void selectTrack(const TrackId&);
    void trackSelected(TrackPointer pTrack);
    /// Full spoken description of a selected track-table row (artist/title
    /// plus rating, color, played state, etc. - see
    /// TrackModel::rowAccessibleText), with its position among the other
    /// rows. row is 0-based; rowCount is the total number of rows.
    void trackRowSelected(const QString& text, int row, int rowCount);
    // Sidebar navigation for spoken announcements. row/siblingCount give the
    // position among siblings ("3 of 12"); childCount and expanded describe
    // container items. row is -1 when position info is unavailable.
    void sidebarItemActivated(const QString& title,
            int row = -1,
            int siblingCount = 0,
            int childCount = 0,
            bool expanded = false);
    // A track was added to / removed from a playlist or crate, for spoken
    // confirmation. Counts are per-signal deltas.
    void playlistTracksEdited(const QString& name, int added, int removed);
    void crateTracksEdited(const QString& name, int added, int removed);
    // A transient on-demand picker (e.g. quick add to crate/playlist) moved
    // to a new item; row/siblingCount give position ("3 of 12") like
    // sidebarItemActivated. Always spoken, since the picker was invoked on
    // purpose so hearing its items isn't optional.
    void quickPickerItemHighlighted(const QString& text, int row, int siblingCount);
    // The just-applied library search matched `count` tracks; folded into the
    // spoken search announcement.
    void searchResultCountChanged(int count);
    void analyzeTracks(const QList<AnalyzerScheduledTrack>& tracks);
#ifdef __ENGINEPRIME__
    void exportLibrary();
    void exportCrate(CrateId crateId);
    void exportPlaylist(int playlistId);
#endif
    void saveModelState();
    void restoreModelState();

    void setTrackTableFont(const QFont& font);
    void setTrackTableRowHeight(int rowHeight);
    void setSelectedClick(bool enable);

    void setSidebarHoverExpandDelay(int delay);

    void onTrackAnalyzerProgress(TrackId trackId, AnalyzerProgress analyzerProgress);

  private slots:
      void onPlayerManagerTrackAnalyzerProgress(TrackId trackId, AnalyzerProgress analyzerProgress);
      void onPlayerManagerTrackAnalyzerIdle();

  private:
    const UserSettingsPointer m_pConfig;

    // The Mixxx database connection pool
    const mixxx::DbConnectionPoolPtr m_pDbConnectionPool;

    const QPointer<TrackCollectionManager> m_pTrackCollectionManager;

    parented_ptr<SidebarModel> m_pSidebarModel;
    parented_ptr<LibraryControl> m_pLibraryControl;

    QList<LibraryFeature*> m_features;
    const static QString m_sTrackViewName;
    WLibrary* m_pLibraryWidget;
    parented_ptr<MixxxLibraryFeature> m_pMixxxLibraryFeature;
    parented_ptr<AutoDJFeature> m_pAutoDJFeature;
    parented_ptr<PlaylistFeature> m_pPlaylistFeature;
    parented_ptr<CrateFeature> m_pCrateFeature;
    parented_ptr<BrowseFeature> m_pBrowseFeature;
    parented_ptr<AnalysisFeature> m_pAnalysisFeature;
    QFont m_trackTableFont;
    int m_iTrackTableRowHeight;
    bool m_editMetadataSelectedClick;
    std::unique_ptr<ControlObject> m_pKeyNotation;
};

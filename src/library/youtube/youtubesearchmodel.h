#pragma once

#include <QList>
#include <QStandardItemModel>
#include <QString>

#include "library/trackmodel.h"
#include "library/youtube/youtubetrack.h"
#include "preferences/usersettings.h"

class TrackCollectionManager;
class YouTubeDownloader;
class YouTubeSearchTask;

/// YouTube search results presented as a native Mixxx track table, so the
/// shared WTrackTableView renders them exactly like any other library view
/// (sortable columns, the standard keyboard shortcuts and context menu, and
/// the accessible names screen readers rely on).
///
/// Follows the BrowseTableModel pattern: a QStandardItemModel that also
/// implements TrackModel, for rows that are not library entries. Unlike Browse
/// there is no file behind a row until it has been downloaded, so:
///
/// - getTrack()/getTrackLocation() resolve only for results already in the
///   download cache, and return nothing otherwise;
/// - loading an undownloaded row goes through TrackModel's deferred-load hook,
///   which fetches the audio first and loads the deck when it arrives.
///
/// The main library search bar drives the model through search(), so typing in
/// the usual place runs a YouTube search - the view needs no search box of its
/// own.
class YouTubeSearchModel final : public QStandardItemModel, public virtual TrackModel {
    Q_OBJECT
  public:
    YouTubeSearchModel(QObject* parent,
            TrackCollectionManager* pTrackCollectionManager,
            UserSettingsPointer pConfig);
    ~YouTubeSearchModel() final = default;

    // Column order of the result table.
    enum Column {
        ColumnTitle = 0,
        ColumnChannel,
        ColumnDuration,
        ColumnCount,
    };

    /// The result backing a row, or an invalid track for an out-of-range row.
    YouTubeTrack resultAt(int row) const;

    // TrackModel
    TrackPointer getTrack(const QModelIndex& index) const final;
    TrackPointer getTrackByRef(const TrackRef& trackRef) const final;
    QUrl getTrackUrl(const QModelIndex& index) const final;
    QString getTrackLocation(const QModelIndex& index) const final;
    TrackId getTrackId(const QModelIndex& index) const final;
    CoverInfo getCoverInfo(const QModelIndex& index) const final;
    const QVector<int> getTrackRows(TrackId trackId) const final;
    void search(const QString& searchText) final;
    const QString currentSearch() const final;
    bool isColumnInternal(int column) final;
    bool isColumnHiddenByDefault(int column) final;
    SortColumnId sortColumnIdFromColumnIndex(int index) const final;
    int columnIndexFromSortColumnId(SortColumnId sortColumn) const final;
    QString modelKey(bool noSearch) const final;
    Capabilities getCapabilities() const final;
    bool updateTrackGenre(Track* pTrack, const QString& genre) const final;
    bool requestDeferredLoad(const QModelIndex& index,
            const QString& group,
            bool play) final;

    Qt::ItemFlags flags(const QModelIndex& index) const final;

  signals:
    /// A row whose audio is not cached yet was asked to load. The feature
    /// downloads it and then loads it into `group` (empty = generic preview
    /// load via loadTrack).
    void deferredLoadRequested(const YouTubeTrack& track, const QString& group, bool play);
    /// Status suitable for showing to the user ("Searching...", errors).
    void statusChanged(const QString& message);

  private slots:
    void slotSearchSucceeded(const QList<YouTubeTrack>& results);
    void slotSearchFailed(const QString& message);

  private:
    void setResults(const QList<YouTubeTrack>& results);
    QString cachedPathFor(const YouTubeTrack& track) const;

    TrackCollectionManager* const m_pTrackCollectionManager;
    UserSettingsPointer m_pConfig;
    YouTubeSearchTask* m_pSearchTask;
    QList<YouTubeTrack> m_results;
    QString m_currentSearch;
};

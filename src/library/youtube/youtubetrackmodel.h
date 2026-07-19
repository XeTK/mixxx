#pragma once

#include "library/basesqltablemodel.h"

/// A native Mixxx track table showing the tracks downloaded by the YouTube (CC)
/// feature, i.e. the library tracks whose files live in the feature's cache
/// directory. It behaves like the main "Tracks" table (sortable columns,
/// drag-to-deck, right-click actions, filtered by the main search box).
class YouTubeTrackModel final : public BaseSqlTableModel {
    Q_OBJECT
  public:
    YouTubeTrackModel(QObject* parent,
            TrackCollectionManager* pTrackCollectionManager,
            const QString& cacheDir);
    ~YouTubeTrackModel() final = default;

    bool isColumnInternal(int column) final;
    TrackModel::Capabilities getCapabilities() const final;

  private:
    void setTableModel(const QString& cacheDir);
};

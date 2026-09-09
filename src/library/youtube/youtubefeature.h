#pragma once

#include <QModelIndex>
#include <QString>
#include <QVariant>

#include "library/libraryfeature.h"
#include "library/treeitemmodel.h"
#include "library/youtube/youtubetrack.h"
#include "preferences/usersettings.h"
#include "util/parented_ptr.h"

class YouTubeDownloader;
class YouTubeSearchModel;
class YouTubeTrackModel;
class Library;
class WLibrary;
class KeyboardEventFilter;

/// Sidebar feature for searching Creative Commons music on YouTube and loading
/// it into Mixxx. It follows the standard Mixxx feature layout: both the root
/// (search results) and the "Downloaded" child node show native track tables,
/// driven by the main library search bar.
///
/// Search results are not playable files until fetched, so the feature owns the
/// downloader: the search model asks for a deferred load, the download runs
/// with its progress shown on the target deck's waveform overview, and the
/// track is loaded once it lands.
class YouTubeFeature : public LibraryFeature {
    Q_OBJECT
  public:
    YouTubeFeature(Library* pLibrary, UserSettingsPointer pConfig);
    ~YouTubeFeature() override = default;

    QVariant title() override;
    void bindLibraryWidget(WLibrary* libraryWidget,
            KeyboardEventFilter* keyboard) override;
    TreeItemModel* sidebarModel() const override;

  public slots:
    void activate() override;
    void activateChild(const QModelIndex& index) override;

  private slots:
    void slotDeferredLoadRequested(const YouTubeTrack& track,
            const QString& group,
            bool play);
    void slotDownloadProgress(const QString& videoId, int percent);
    void slotDownloadSucceeded(const YouTubeTrack& track, const QString& localPath);
    void slotDownloadFailed(const QString& videoId, const QString& message);

  private:
    QString cacheDir() const;

    parented_ptr<TreeItemModel> m_pSidebarModel;
    YouTubeSearchModel* m_pSearchModel;
    YouTubeTrackModel* m_pDownloadedModel;
    YouTubeDownloader* m_pDownloader;
    // Deck the in-flight download is destined for (empty = generic load), so
    // progress can be shown there and the track loaded when it arrives.
    QString m_pendingLoadGroup;
    bool m_pendingLoadPlay = false;
    const QString m_title;
};

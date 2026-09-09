#pragma once

#include <QList>
#include <QString>
#include <QWidget>
#include <memory>
#include <vector>

#include "library/libraryview.h"
#include "library/youtube/youtubecctrack.h"
#include "preferences/usersettings.h"
#include "track/track_decl.h"

class Library;
class WLibrary;
class KeyboardEventFilter;
class YouTubeCcSearchTask;
class YouTubeCcDownloader;
class ControlProxy;
class QTableWidget;
class QPushButton;
class QLabel;
class QProgressBar;

/// Search-results view for the YouTube (CC) feature. It has no search box of
/// its own: the main Mixxx library search bar drives it via onSearch(). Results
/// are Creative Commons videos (metadata only); double-clicking (or the Load
/// button) downloads the audio via yt-dlp, adds it to the library so it gets
/// analyzed like any other file, loads it, and notifies the feature so the
/// Downloaded table refreshes.
class DlgYouTubeCc : public QWidget, public virtual LibraryView {
    Q_OBJECT
  public:
    DlgYouTubeCc(WLibrary* parent, UserSettingsPointer pConfig, Library* pLibrary);
    ~DlgYouTubeCc() override;

    // LibraryView
    void onShow() override;
    bool hasFocus() const override;
    void setFocus() override;
    void onSearch(const QString& text) override;

    /// Install Mixxx's keyboard event filter on the results table so library
    /// keyboard shortcuts (e.g. Shift+Left/Right to load to a deck) work here.
    void installKeyboardFilter(KeyboardEventFilter* pKeyboard);

  signals:
    void loadTrack(TrackPointer pTrack);
    void loadTrackToPlayer(TrackPointer pTrack, const QString& group, bool play);
    void trackSelected(TrackPointer pTrack);
    /// Emitted after a track has been downloaded and added to the library.
    void downloaded();

  private slots:
    void slotSearchSucceeded(const QList<YouTubeCcTrack>& results);
    void slotSearchFailed(const QString& message);
    void slotResultActivated(int row, int column);
    void slotLoadSelected();
    void slotSelectionChanged();
    void slotDownloadProgress(const QString& videoId, int percent);
    void slotDownloadSucceeded(const YouTubeCcTrack& track, const QString& localPath);
    void slotDownloadFailed(const QString& videoId, const QString& message);

  private:
    void setupUi();
    /// Observe the [ChannelN],LoadSelectedTrack(AndPlay) controls so the deck
    /// load shortcuts (Shift+Left/Right by default) work on search results.
    void setupLoadShortcuts();
    /// Download the selected result and load it to the given deck group. Called
    /// from the deck-load shortcuts; ignored unless this view is active.
    void loadSelectedToGroup(const QString& group, bool play);
    QString ytDlpPath() const;
    QString cacheDir() const;
    /// Download row and, on success, load it. An empty group loads via the
    /// generic loadTrack() path; a non-empty group loads to that deck.
    void startDownload(int row, const QString& group = QString(), bool play = false);
    void setStatus(const QString& message);

    UserSettingsPointer m_pConfig;
    Library* const m_pLibrary;

    QTableWidget* m_pResults;
    QPushButton* m_pLoadButton;
    QLabel* m_pStatus;
    QProgressBar* m_pProgress;

    YouTubeCcSearchTask* m_pSearchTask;
    YouTubeCcDownloader* m_pDownloader;
    QList<YouTubeCcTrack> m_currentResults;
    std::vector<std::unique_ptr<ControlProxy>> m_loadControls;
    // Deck to load the current download into (empty = generic preview load).
    QString m_pendingLoadGroup;
    bool m_pendingLoadPlay = false;
};

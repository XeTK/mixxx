#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>
#include <vector>

#include "library/library_decl.h"
#include "preferences/accessibilitysettings.h"
#include "preferences/usersettings.h"
#include "track/track_decl.h"

class Library;
class PlayerManagerInterface;
class TtsEngine;
class EngineTts;
class ControlObject;
class ControlProxy;

class AnnouncementManager : public QObject {
    Q_OBJECT
  public:
    // Production factory: creates the platform TtsEngine internally.
    static std::unique_ptr<AnnouncementManager> create(Library* pLibrary,
            PlayerManagerInterface* pPlayerManager,
            UserSettingsPointer pConfig,
            EngineTts* pTtsSink,
            QObject* parent = nullptr);

    // Single constructor. Tests inject a spy TtsEngine; pass nullptr for
    // pTtsSink when no engine-level sink is needed.
    AnnouncementManager(Library* pLibrary,
            PlayerManagerInterface* pPlayerManager,
            UserSettingsPointer pConfig,
            std::unique_ptr<TtsEngine> pTts,
            EngineTts* pTtsSink,
            QObject* parent = nullptr);

    ~AnnouncementManager() override;

    // Exposed as public so tests can drive the slots directly without needing
    // a real Library or live signal connections.
  public slots:
    void slotTrackSelected(TrackPointer pTrack);
    void slotAnnounceSelectedTrack();
    void slotNewTrackLoaded(TrackPointer pTrack, int deckIndex);
    void slotNumberOfDecksChanged(int decks);
    void slotSkinLoaded();
    void slotLibraryFocusChanged(double value);
    void slotSidebarItemActivated(const QString& title);
    void slotSearchTextChanged(const QString& text);
    void slotAnnounceSearch();

    // Static helpers are public so tests can verify formatting independently.
    static QString formatForBrowsing(TrackPointer pTrack);
    static QString formatForLoad(TrackPointer pTrack, int deckIndex);

    // Spoken summary of a deck's state (playback, time remaining, BPM,
    // pitch), used by the on-demand [ChannelN],tts_status hotkey. Public so
    // tests can verify the formatting.
    QString formatDeckStatus(const QString& group, int deckIndex) const;

    // Test helpers: allow tests to wire up CO observers for a synthetic group
    // without needing a real BaseTrackPlayer.
    void connectGroupControls(const QString& group, int deckIndex = -1);
    void setDeckHasTrack(const QString& group, bool value);

  private:
    void connectDeck(int deckIndex);
    void init(Library* pLibrary, PlayerManagerInterface* pPlayerManager);
    void speak(const QString& text);

    std::unique_ptr<TtsEngine> m_pTts;
    // Engine sink the synthesized speech is rendered into. Null in unit tests,
    // where a spy TtsEngine is injected instead.
    EngineTts* m_pTtsSink{nullptr};
    std::unique_ptr<ControlProxy> m_pSampleRate;
    AccessibilitySettings m_settings;
    QString m_currentTtsVoiceId;
    int m_currentTtsRate{0};
    int m_currentTtsRoute{-1};
    PlayerManagerInterface* m_pPlayerManager;
    QTimer m_selectionDebounce;
    TrackPointer m_pendingTrack;
    int m_connectedDecks{0};

    // Library focus tracking: updated in slotLibraryFocusChanged.
    FocusWidget m_lastFocusWidget{FocusWidget::None};

    // Deduplication for sidebar announcements.
    QString m_lastAnnouncedSidebarItem;

    // Debounced search announcement.
    QTimer m_searchDebounce;
    QString m_pendingSearch;

    // Per-deck playback state tracking. Keyed by deck group (e.g. "[Channel1]").
    QHash<QString, bool> m_deckHasTrack;
    QHash<QString, bool> m_deckIsPlaying;

    // On-demand announcement buttons: [ChannelN],tts_status per deck and the
    // global [Tts],repeat. Owned here; mapped from the keyboard like any CO.
    std::vector<std::unique_ptr<ControlObject>> m_pStatusButtons;
    std::unique_ptr<ControlObject> m_pRepeatButton;
    QString m_lastSpoken;
};

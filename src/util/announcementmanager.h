#pragma once

#include <memory>

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>

#include "preferences/accessibilitysettings.h"
#include "preferences/usersettings.h"
#include "track/track_decl.h"

class Library;
class PlayerManagerInterface;
class TtsEngine;

class AnnouncementManager : public QObject {
    Q_OBJECT
  public:
    AnnouncementManager(Library* pLibrary,
            PlayerManagerInterface* pPlayerManager,
            UserSettingsPointer pConfig,
            QObject* parent = nullptr);

    // Constructor for testing: accepts a pre-built TtsEngine so tests can
    // inject a spy without going through TtsEngine::create().
    AnnouncementManager(Library* pLibrary,
            PlayerManagerInterface* pPlayerManager,
            UserSettingsPointer pConfig,
            std::unique_ptr<TtsEngine> pTts,
            QObject* parent = nullptr);

    ~AnnouncementManager() override;

    // Exposed as public so tests can drive the slots directly without needing
    // a real Library or live signal connections.
  public slots:
    void slotTrackSelected(TrackPointer pTrack);
    void slotAnnounceSelectedTrack();
    void slotNewTrackLoaded(TrackPointer pTrack);
    void slotNumberOfDecksChanged(int decks);

    // Static helpers are public so tests can verify formatting independently.
    static QString formatForBrowsing(TrackPointer pTrack);
    static QString formatForLoad(TrackPointer pTrack);

  private:
    void connectDeck(int deckIndex);
    void init(Library* pLibrary, PlayerManagerInterface* pPlayerManager);

    std::unique_ptr<TtsEngine> m_pTts;
    AccessibilitySettings m_settings;
    PlayerManagerInterface* m_pPlayerManager;
    QTimer m_selectionDebounce;
    TrackPointer m_pendingTrack;
    int m_connectedDecks{0};

    // Per-deck playback state tracking. Keyed by deck group (e.g. "[Channel1]").
    QHash<QString, bool> m_deckHasTrack;
    QHash<QString, bool> m_deckIsPlaying;
};

#pragma once

#include <memory>

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
    ~AnnouncementManager() override;

  private slots:
    void slotTrackSelected(TrackPointer pTrack);
    void slotAnnounceSelectedTrack();
    void slotNewTrackLoaded(TrackPointer pTrack);
    void slotNumberOfDecksChanged(int decks);

  private:
    void connectDeck(int deckIndex);

    static QString formatForBrowsing(TrackPointer pTrack);
    static QString formatForLoad(TrackPointer pTrack);

    std::unique_ptr<TtsEngine> m_pTts;
    AccessibilitySettings m_settings;
    PlayerManagerInterface* m_pPlayerManager;
    QTimer m_selectionDebounce;
    TrackPointer m_pendingTrack;
    int m_connectedDecks{0};
};

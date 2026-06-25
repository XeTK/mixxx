#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>

#include "library/library_decl.h"
#include "preferences/accessibilitysettings.h"
#include "preferences/usersettings.h"
#include "track/track_decl.h"

class Library;
class PlayerManagerInterface;
class TtsEngine;
class EngineTts;
class ControlProxy;

class EnhancedAnnouncementManager : public QObject {
    Q_OBJECT
  public:
    // Production factory: creates the platform TtsEngine internally.
    static std::unique_ptr<EnhancedAnnouncementManager> create(Library* pLibrary,
            PlayerManagerInterface* pPlayerManager,
            UserSettingsPointer pConfig,
            EngineTts* pTtsSink,
            QObject* parent = nullptr);

    // Single constructor. Tests inject a spy TtsEngine; pass nullptr for
    // pTtsSink when no engine-level sink is needed.
    EnhancedAnnouncementManager(Library* pLibrary,
            PlayerManagerInterface* pPlayerManager,
            UserSettingsPointer pConfig,
            std::unique_ptr<TtsEngine> pTts,
            EngineTts* pTtsSink,
            QObject* parent = nullptr);

    ~EnhancedAnnouncementManager() override;

    // Public slots for enhanced announcement handling
  public slots:
    // New enhanced announcements
    void slotAnnounceEqParamChanged(const QString& group, const QString& paramName, double value);
    void slotAnnounceFilterParamChanged(
            const QString& group, const QString& paramName, double value);
    void slotAnnounceTrimParamChanged(const QString& group, const QString& paramName, double value);
    void slotAnnounceMasterParamChanged(
            const QString& group, const QString& paramName, double value);
    void slotAnnounceMixParamChanged(const QString& group, const QString& paramName, double value);
    void slotAnnounceEffectSelected(const QString& effectId);
    void slotAnnounceSyncStatusChanged(bool enabled);
    void slotAnnounceTempoSliderChanged(double value);
    void slotAnnounceCrossFaderChanged(double value);
    void slotAnnounceFaderStateChanged(const QString& group, double value);
    void slotAnnouncePreventJoggingStateChanged(bool enabled);
    void slotAnnounceTouchSurfaceState(bool enabled);

    // For TTS toggle handling
    void slotTtsToggled(bool enabled);

    // Override existing slots to enhance them
    void slotTrackSelected(TrackPointer pTrack);
    void slotNewTrackLoaded(TrackPointer pTrack, int deckIndex);
    void slotAnnounceSelectedTrack();
    void slotAnnounceSearch();
    void slotLibraryFocusChanged(double value);

    // Static helpers for formatting
    static QString formatForBrowsing(TrackPointer pTrack);
    static QString formatForLoad(TrackPointer pTrack, int deckIndex);

    // Test helpers
    void connectGroupControls(const QString& group);
    void setDeckHasTrack(const QString& group, bool value);

  private:
    void init(Library* pLibrary, PlayerManagerInterface* pPlayerManager);
    void speak(const QString& text);
    void connectDeck(int deckIndex);
    QString formatEqParam(const QString& group, const QString& paramName, double value);
    QString formatFilterParam(const QString& group, const QString& paramName, double value);
    QString formatTrimParam(const QString& group, const QString& paramName, double value);
    QString formatMasterParam(const QString& group, const QString& paramName, double value);
    QString formatMixParam(const QString& group, const QString& paramName, double value);
    QString formatTempoSlider(double value);
    QString formatCrossFader(double value);
    QString formatFaderState(const QString& group, double value);

    std::unique_ptr<TtsEngine> m_pTts;
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

    // Library focus tracking
    FocusWidget m_lastFocusWidget{FocusWidget::None};

    // Deduplication for sidebar announcements
    QString m_lastAnnouncedSidebarItem;

    // Debounced search announcement
    QTimer m_searchDebounce;
    QString m_pendingSearch;

    // Per-deck playback state tracking
    QHash<QString, bool> m_deckHasTrack;
    QHash<QString, bool> m_deckIsPlaying;

    // Enhanced tracking
    QHash<QString, bool> m_deckIsPlayingState;
    QHash<QString, double> m_lastFaderValues;
    QHash<QString, QString> m_lastEffectIds;
    QHash<QString, QString> m_lastEqParamNames;
    QHash<QString, QString> m_lastFilterParamNames;
    QHash<QString, QString> m_lastTrimParamNames;
    QHash<QString, QString> m_lastMasterParamNames;
    QHash<QString, QString> m_lastMixParamNames;

    // New state tracking
    bool m_preventJoggingEnabled{true}; // Enabled by default as requested
    bool m_touchSurfaceEnabled{true};
};

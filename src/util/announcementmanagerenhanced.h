#pragma once

#include "util/announcementmanager.h"
#include "widget/keyboardnavigation.h"

#include <QKeyEvent>
#include <QWidget>

/// Enhanced AnnouncementManager with keyboard navigation accessibility features
class EnhancedAnnouncementManager : public AnnouncementManager {
    Q_OBJECT

public:
    /// Enhanced constructor with keyboard navigation support
    EnhancedAnnouncementManager(Library* pLibrary,
            PlayerManagerInterface* pPlayerManager,
            UserSettingsPointer pConfig,
            std::unique_ptr<TtsEngine> pTts,
            EngineTts* pTtsSink,
            QObject* parent = nullptr);

    /// Handle keyboard events for accessibility
    bool handleKeyboardEvent(QKeyEvent* event, QWidget* focusedWidget);

    /// Announce keyboard navigation state
    void announceKeyboardNavigationState();

    /// Announce widget accessibility information
    void announceWidgetAccessibility(QWidget* widget);

    /// Check if keyboard navigation is enabled
    bool isKeyboardNavigationEnabled() const;

    /// Enable/disable keyboard navigation announcements
    void setKeyboardNavigationEnabled(bool enabled);

    // Enhanced TTS announcement methods
    void announceCueStateChanged(int deckIndex, bool isCueActive);
    void announceTrackLoaded(int deckIndex, TrackPointer pTrack);
    void announceTrackSelected(TrackPointer pTrack);
    void announcePlaybackStateChanged(int deckIndex, bool isPlaying);
    void announceEndOfTrack(int deckIndex);
    void announceTtsToggle(bool enabled);
    void announceLibraryFocus(const QString& focusWidget, const QString& activeItem);

    /// Set deck naming convention (Deck 1 vs Deck A)
    void setDeckNamingConvention(const QString& convention);
    
    /// Get current deck naming convention
    QString getDeckNamingConvention() const;

private:
    bool m_keyboardNavigationEnabled;
    QWidget* m_lastFocusedWidget;
    QString m_deckNamingConvention;  // "Deck1" or "DeckA"
};
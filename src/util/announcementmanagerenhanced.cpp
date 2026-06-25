#include "util/announcementmanagerenhanced.h"

#include <QKeyEvent>
#include <QWidget>
#include <QDebug>
#include <QRegularExpression>

EnhancedAnnouncementManager::EnhancedAnnouncementManager(
        Library* pLibrary,
        PlayerManagerInterface* pPlayerManager,
        UserSettingsPointer pConfig,
        std::unique_ptr<TtsEngine> pTts,
        EngineTts* pTtsSink,
        QObject* parent)
        : AnnouncementManager(pLibrary, pPlayerManager, std::move(pConfig), std::move(pTts), pTtsSink, parent),
          m_keyboardNavigationEnabled(false),
          m_lastFocusedWidget(nullptr),
          m_deckNamingConvention("Deck1") {  // Default to "Deck 1" format
}

bool EnhancedAnnouncementManager::handleKeyboardEvent(QKeyEvent* event, QWidget* focusedWidget) {
    if (!event || !focusedWidget) {
        return false;
    }
    
    if (!m_keyboardNavigationEnabled) {
        return false;
    }
    
    // Handle Tab key for focus changes
    if (event->key() == Qt::Key_Tab) {
        // Announce when focus moves between elements
        announceWidgetAccessibility(focusedWidget);
        return false; // Allow normal tab behavior
    }
    
    // Handle arrow keys for navigation
    switch (event->key()) {
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down:
            // Announce directional navigation
            if (focusedWidget) {
                QString name = KeyboardNavigation::getAccessibleName(focusedWidget);
                speak(QString("Navigated to %1").arg(name));
            }
            return false;
        default:
            return false;
    }
}

void EnhancedAnnouncementManager::announceKeyboardNavigationState() {
    if (m_keyboardNavigationEnabled) {
        speak("Keyboard navigation enabled");
    } else {
        speak("Keyboard navigation disabled");
    }
}

void EnhancedAnnouncementManager::announceWidgetAccessibility(QWidget* widget) {
    if (!widget) {
        return;
    }
    
    QString name = KeyboardNavigation::getAccessibleName(widget);
    QString description = widget->accessibleDescription();
    
    if (!name.isEmpty()) {
        QString announcement = name;
        if (!description.isEmpty()) {
            announcement += QString(" - %1").arg(description);
        }
        speak(announcement);
    } else {
        // Fallback to object name or generic announcement
        QString objName = widget->objectName();
        if (!objName.isEmpty()) {
            speak(QString("Focus on %1").arg(objName));
        } else {
            speak("Focus changed");
        }
    }
}

bool EnhancedAnnouncementManager::isKeyboardNavigationEnabled() const {
    return m_keyboardNavigationEnabled;
}

void EnhancedAnnouncementManager::setKeyboardNavigationEnabled(bool enabled) {
    m_keyboardNavigationEnabled = enabled;
    if (enabled) {
        announceKeyboardNavigationState();
    }
}

// Enhanced announcement methods specifically for TTS improvements
void EnhancedAnnouncementManager::announceCueStateChanged(int deckIndex, bool isCueActive) {
    QString deckName = m_deckNamingConvention;
    if (deckName == "Deck1") {
        deckName = QString("Deck %1").arg(deckIndex);
    } else if (deckName == "DeckA") {
        deckName = QString("Deck %1").arg(QChar('A' + deckIndex - 1));
    }
    
    if (isCueActive) {
        speak(QString("%1 cue on").arg(deckName));
    } else {
        speak(QString("%1 cue off").arg(deckName));
    }
}

void EnhancedAnnouncementManager::announceTrackLoaded(int deckIndex, TrackPointer pTrack) {
    QString deckName = m_deckNamingConvention;
    if (deckName == "Deck1") {
        deckName = QString("Deck %1").arg(deckIndex);
    } else if (deckName == "DeckA") {
        deckName = QString("Deck %1").arg(QChar('A' + deckIndex - 1));
    }
    
    if (pTrack && !pTrack->getName().isEmpty()) {
        QString trackName = pTrack->getName();
        QString artist = pTrack->getArtist();
        QString bpm = pTrack->getBPM().toString();
        QString key = pTrack->getKey().toString();
        
        // Include deck information in the announcement
        if (!artist.isEmpty()) {
            speak(QString("%1 loaded: %2 by %3").arg(deckName).arg(trackName).arg(artist));
        } else {
            speak(QString("%1 loaded: %2").arg(deckName).arg(trackName));
        }
    } else {
        speak(QString("%1 track loaded").arg(deckName));
    }
}

void EnhancedAnnouncementManager::announceTrackSelected(TrackPointer pTrack) {
    if (pTrack && !pTrack->getName().isEmpty()) {
        QString trackName = pTrack->getName();
        QString artist = pTrack->getArtist();
        
        if (!artist.isEmpty()) {
            speak(QString("Selected: %1 by %2").arg(trackName).arg(artist));
        } else {
            speak(QString("Selected: %1").arg(trackName));
        }
    } else {
        speak("Track selected");
    }
}

void EnhancedAnnouncementManager::announcePlaybackStateChanged(int deckIndex, bool isPlaying) {
    QString deckName = m_deckNamingConvention;
    if (deckName == "Deck1") {
        deckName = QString("Deck %1").arg(deckIndex);
    } else if (deckName == "DeckA") {
        deckName = QString("Deck %1").arg(QChar('A' + deckIndex - 1));
    }
    
    if (isPlaying) {
        speak(QString("%1 playing").arg(deckName));
    } else {
        speak(QString("%1 stopped").arg(deckName));
    }
}

void EnhancedAnnouncementManager::announceEndOfTrack(int deckIndex) {
    QString deckName = m_deckNamingConvention;
    if (deckName == "Deck1") {
        deckName = QString("Deck %1").arg(deckIndex);
    } else if (deckName == "DeckA") {
        deckName = QString("Deck %1").arg(QChar('A' + deckIndex - 1));
    }
    
    speak(QString("%1 end of track").arg(deckName));
}

void EnhancedAnnouncementManager::announceTtsToggle(bool enabled) {
    if (enabled) {
        speak("Text-to-speech enabled");
    } else {
        speak("Text-to-speech disabled");
    }
}

void EnhancedAnnouncementManager::announceLibraryFocus(const QString& focusWidget, const QString& activeItem) {
    if (!focusWidget.isEmpty() && !activeItem.isEmpty()) {
        speak(QString("Library focus: %1, active item: %2").arg(focusWidget).arg(activeItem));
    } else if (!focusWidget.isEmpty()) {
        speak(QString("Library focus: %1").arg(focusWidget));
    } else {
        speak("Library focus changed");
    }
}

void EnhancedAnnouncementManager::setDeckNamingConvention(const QString& convention) {
    if (convention == "Deck1" || convention == "DeckA") {
        m_deckNamingConvention = convention;
    }
}

QString EnhancedAnnouncementManager::getDeckNamingConvention() const {
    return m_deckNamingConvention;
}
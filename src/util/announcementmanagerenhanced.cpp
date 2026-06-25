#include "util/announcementmanagerenhanced.h"

#include <QKeyEvent>
#include <QWidget>
#include <QDebug>

EnhancedAnnouncementManager::EnhancedAnnouncementManager(
        Library* pLibrary,
        PlayerManagerInterface* pPlayerManager,
        UserSettingsPointer pConfig,
        std::unique_ptr<TtsEngine> pTts,
        EngineTts* pTtsSink,
        QObject* parent)
        : AnnouncementManager(pLibrary, pPlayerManager, std::move(pConfig), std::move(pTts), pTtsSink, parent),
          m_keyboardNavigationEnabled(false),
          m_lastFocusedWidget(nullptr) {
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
#include "preferences/dialog/dlgprefaccessibilityenhanced.h"

#include <QKeyEvent>
#include <QCheckBox>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QApplication>
#include <QDebug>

DlgPrefAccessibilityEnhanced::DlgPrefAccessibilityEnhanced(
        QWidget* parent, UserSettingsPointer pConfig, EngineTts* pTtsSink)
        : DlgPrefAccessibility(parent, pConfig, pTtsSink),
          m_keyboardNavigationEnabled(false) {
}

void DlgPrefAccessibilityEnhanced::keyPressEvent(QKeyEvent* event) {
    if (handleAccessibilityNavigation(event)) {
        return;
    }
    
    // Pass through to parent class for standard behavior
    DlgPrefAccessibility::keyPressEvent(event);
}

bool DlgPrefAccessibilityEnhanced::handleKeyboardEvent(QKeyEvent* event) {
    if (!event) {
        return false;
    }
    
    return handleAccessibilityNavigation(event);
}

bool DlgPrefAccessibilityEnhanced::handleAccessibilityNavigation(QKeyEvent* event) {
    if (!event) {
        return false;
    }
    
    if (!m_keyboardNavigationEnabled) {
        return false;
    }
    
    // Handle keyboard navigation within the dialog
    switch (event->key()) {
        case Qt::Key_Tab:
            // Announce the current control
            if (focusWidget()) {
                QString name = KeyboardNavigation::getAccessibleName(focusWidget());
                if (!name.isEmpty()) {
                    // Would speak announcement here - implementation depends on TTS system
                    qDebug() << "Accessibility focus on:" << name;
                }
            }
            return false; // Allow normal tab navigation
            
        case Qt::Key_Space:
            // Handle space key for checkboxes
            if (focusWidget() && focusWidget()->inherits("QCheckBox")) {
                QCheckBox* checkBox = qobject_cast<QCheckBox*>(focusWidget());
                if (checkBox) {
                    bool checked = !checkBox->isChecked();
                    checkBox->setChecked(checked);
                    announceSetting(checkBox->text(), checked);
                    return true;
                }
            }
            return false;
            
        default:
            return false;
    }
}

void DlgPrefAccessibilityEnhanced::announceSetting(const QString& settingName, bool enabled) {
    // In a real implementation this would use TTS to announce settings changes
    QString announcement = enabled 
        ? QString("%1 enabled").arg(settingName)
        : QString("%1 disabled").arg(settingName);
    
    qDebug() << "Accessibility announcement:" << announcement;
    // Would call TTS system here
}
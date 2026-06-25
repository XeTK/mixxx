#pragma once

#include "preferences/dialog/dlgprefaccessibility.h"
#include "widget/keyboardnavigation.h"

#include <QKeyEvent>

/// Enhanced accessibility preferences dialog with keyboard navigation support
class DlgPrefAccessibilityEnhanced : public DlgPrefAccessibility {
    Q_OBJECT

public:
    DlgPrefAccessibilityEnhanced(QWidget* parent, 
                                 UserSettingsPointer pConfig, 
                                 EngineTts* pTtsSink);

    /// Handle keyboard events for accessibility
    bool handleKeyboardEvent(QKeyEvent* event);

protected:
    /// Override keyPressEvent to add keyboard accessibility support
    void keyPressEvent(QKeyEvent* event) override;

private:
    /// Announce current accessibility setting
    void announceSetting(const QString& settingName, bool enabled);

    /// Handle accessibility-specific keyboard navigation
    bool handleAccessibilityNavigation(QKeyEvent* event);

    bool m_keyboardNavigationEnabled;
};
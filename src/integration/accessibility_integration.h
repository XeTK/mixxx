#pragma once

/// Accessibility Integration Header
/// This file shows the integration approach for accessibility features in Mixxx
///
/// Integration Points:
/// 1. Main window keyboard navigation support
/// 2. Preference dialog enhancement  
/// 3. Announcement system integration
/// 4. High-contrast theme support

#include "widget/keyboardnavigation.h"
#include "widget/highcontrastutils.h"
#include "util/announcementmanagerenhanced.h"

// Example integration methods (would be in actual implementation files)

namespace AccessibilityIntegration {
    
/// Initialize keyboard navigation support for Mixxx UI
void initializeKeyboardNavigation();
    
/// Enable high-contrast mode in application
void enableHighContrastMode();
    
/// Connect accessibility events to announcement manager
void connectAccessibilityEvents();
    
/// Setup enhanced preference dialog
void setupEnhancedPreferences();
    
/// Initialize accessibility settings
void initializeAccessibilitySettings();
}
# Mixxx Accessibility Integration Guide

This document outlines how to integrate the accessibility components we've created into the actual Mixxx codebase.

## Integration Points

### 1. Preference Dialog Integration

**File**: `src/preferences/dialog/dlgpreferences.cpp`

**Change**: Replace the basic accessibility dialog with our enhanced version:

```cpp
// Find this line in dlgpreferences.cpp:
new DlgPrefAccessibility(this, m_pConfig, pSoundManager->getTtsSink()),

// Replace with:
new DlgPrefAccessibilityEnhanced(this, m_pConfig, pSoundManager->getTtsSink()),
```

**Include**: Make sure to add the header to the includes section:

```cpp
#include "preferences/dialog/dlgprefaccessibilityenhanced.h"
```

### 2. Main Window Keyboard Navigation

**File**: `src/mixxxmainwindow.cpp`

**Add keyboard event handling** to support global keyboard navigation:

```cpp
// In MixxxMainWindow class, override keyPressEvent:
protected:
    void keyPressEvent(QKeyEvent* event) override;

// Implementation in .cpp file:
void MixxxMainWindow::keyPressEvent(QKeyEvent* event) {
    // Handle global keyboard accessibility events
    if (event->key() == Qt::Key_F6) {
        // Toggle accessibility features
        KeyboardNavigation::setNavigationHintsEnabled(
            !KeyboardNavigation::navigationHintsEnabled());
        return;
    }
    
    // Pass through to base class
    QMainWindow::keyPressEvent(event);
}
```

### 3. Announcement Manager Integration

**File**: `src/coreservices.cpp` (where announcement manager is initialized)

**Integration**: Connect to the enhanced announcement manager:

```cpp
// In CoreServices::initialize():
m_pAnnouncementManager = std::make_unique<EnhancedAnnouncementManager>(
        m_pLibrary.get(),
        m_pPlayerManager.get(),
        getSettings(),
        m_pEngine->getTts(),
        this);
```

### 4. High Contrast Theme Integration

**File**: `src/mixxxmainwindow.cpp` or skin loading code

**Add theme management** to support high-contrast modes:

```cpp
// In initialize() or skin loading:
void MixxxMainWindow::initialize() {
    // ... existing code ...
    
    // Enable high-contrast if configured
    if (HighContrastUtils::isHighContrastModeEnabled()) {
        // Apply high-contrast theme to all widgets
        for (auto widget : this->findChildren<QWidget*>()) {
            HighContrastUtils::applyHighContrastTheme(widget);
        }
    }
}
```

## Key Integration Files

### Widget Integration Points:
1. **src/widget/keyboardnavigation.cpp** - Core navigation utilities
2. **src/widget/highcontrastutils.cpp** - Theme management
3. **src/util/announcementmanagerenhanced.cpp** - Enhanced announcements

### Preference Integration Points:
1. **src/preferences/dialog/dlgpreferences.cpp** - Preference dialog replacement
2. **src/preferences/dialog/dlgprefaccessibilityenhanced.cpp** - Enhanced dialog implementation

### Main Application Integration Points:
1. **src/mixxxmainwindow.cpp** - Global keyboard navigation
2. **src/coreservices.cpp** - Enhanced announcement manager

## Testing Integration

### Unit Tests
```bash
# Run existing tests to ensure no regressions
cd build && make test
```

### Integration Testing
1. **Keyboard Navigation Test**: 
   - Verify tab navigation works through all controls
   - Check focus indicators
   - Test accessibility announcements

2. **High Contrast Test**:
   - Verify theme switching works
   - Check color contrast ratios
   - Test readability

3. **Announcement System Test**:
   - Verify TTS announcements are made for keyboard navigation
   - Test announcement manager integration

## Key Considerations

1. **Backward Compatibility**: All existing features must continue working
2. **Performance**: Accessibility features should have minimal performance impact
3. **Configuration**: Accessibility settings should be configurable
4. **Platform Support**: Follow platform-specific accessibility guidelines
5. **Testing**: Comprehensive testing with accessibility tools

## Build System Integration

### CMake Integration (if needed):
```cmake
# In CMakeLists.txt, add to appropriate targets:
target_sources(mixxx PRIVATE
    src/widget/keyboardnavigation.cpp
    src/widget/highcontrastutils.cpp
    src/util/announcementmanagerenhanced.cpp
    src/preferences/dialog/dlgprefaccessibilityenhanced.cpp
    src/integration/accessibility_integration.cpp
)
```

## Quality Assurance

### Pre-Integration Checks:
1. All existing tests pass
2. No memory leaks or crashes
3. Build system compiles without errors
4. Performance remains acceptable
5. UI remains responsive

### Post-Integration Checks:
1. Accessibility features work as expected
2. Keyboard navigation is complete and logical
3. High-contrast themes function properly
4. Announcement system provides useful feedback
5. No regressions in existing functionality

## Recommended Implementation Order

1. **Preferences Integration** - Most straightforward
2. **Main Window Keyboard Navigation** - Critical for accessibility
3. **Announcement System Enhancement** - Core functionality
4. **High-Contrast Themes** - Visual accessibility
5. **Skin Integration** - UI rendering layer (most complex)

This guide provides the roadmap to properly integrate all accessibility components we've developed into the Mixxx application.
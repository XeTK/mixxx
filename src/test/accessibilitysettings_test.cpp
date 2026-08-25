#include "preferences/accessibilitysettings.h"

#include <gtest/gtest.h>

#include "test/mixxxtest.h"

namespace {

class AccessibilitySettingsTest : public MixxxTest {};

// Off by default: enabling controller-driven library navigation while Mixxx
// lacks OS window focus is purely opt-in, so existing users see no behavior
// change unless they turn it on (issue #64).
TEST_F(AccessibilitySettingsTest, ControllerNavigationWithoutFocus_DefaultsOff) {
    AccessibilitySettings settings(config());
    EXPECT_FALSE(settings.getControllerNavigationWithoutFocusDefault());
    EXPECT_FALSE(settings.getControllerNavigationWithoutFocus());
}

TEST_F(AccessibilitySettingsTest, ControllerNavigationWithoutFocus_ReadWrite) {
    AccessibilitySettings settings(config());

    settings.setControllerNavigationWithoutFocus(true);
    EXPECT_TRUE(settings.getControllerNavigationWithoutFocus());

    settings.setControllerNavigationWithoutFocus(false);
    EXPECT_FALSE(settings.getControllerNavigationWithoutFocus());
}

TEST_F(AccessibilitySettingsTest, ControllerNavigationWithoutFocus_ResetToDefault) {
    AccessibilitySettings settings(config());

    settings.setControllerNavigationWithoutFocus(true);
    ASSERT_TRUE(settings.getControllerNavigationWithoutFocus());

    settings.setControllerNavigationWithoutFocusToDefault();
    EXPECT_FALSE(settings.getControllerNavigationWithoutFocus());
}

} // namespace

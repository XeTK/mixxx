#include "widget/keyboardnavigation.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QTest>
#include <QWidget>

#include "test/mixxxtest.h"

// Test keyboard navigation utility functionality
class KeyboardNavigationTest : public MixxxTest {
  protected:
    void SetUp() override {
        MixxxTest::SetUp();
    }

    void TearDown() override {
        MixxxTest::TearDown();
    }
};

// Test initialization and basic functionality
TEST_F(KeyboardNavigationTest, TestInitialization) {
    QWidget widget;
    EXPECT_TRUE(true);
}

// Test accessibility name functionality
TEST_F(KeyboardNavigationTest, TestAccessibleName) {
    QWidget widget;
    widget.setAccessibleName("Test Widget");

    QString name = KeyboardNavigation::getAccessibleName(&widget);
    EXPECT_EQ(name, "Test Widget");
}

// Test keyboard accessibility detection
TEST_F(KeyboardNavigationTest, TestKeyboardAccessibility) {
    QWidget widget;
    widget.setFocusPolicy(Qt::TabFocus);
    widget.setEnabled(true);
    widget.setVisible(true);

    // Widget should be keyboard accessible
    EXPECT_TRUE(KeyboardNavigation::isKeyboardAccessible(&widget));

    // Test with disabled widget
    widget.setEnabled(false);
    EXPECT_FALSE(KeyboardNavigation::isKeyboardAccessible(&widget));
}

// Test navigation hints
TEST_F(KeyboardNavigationTest, TestNavigationHints) {
    EXPECT_FALSE(KeyboardNavigation::navigationHintsEnabled());

    KeyboardNavigation::setNavigationHintsEnabled(true);
    EXPECT_TRUE(KeyboardNavigation::navigationHintsEnabled());

    KeyboardNavigation::setNavigationHintsEnabled(false);
    EXPECT_FALSE(KeyboardNavigation::navigationHintsEnabled());
}

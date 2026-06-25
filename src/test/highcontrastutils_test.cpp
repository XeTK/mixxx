#include "widget/highcontrastutils.h"

#include <gtest/gtest.h>

#include <QColor>

#include "test/mixxxtest.h"

// Test high-contrast utility functionality
class HighContrastUtilsTest : public MixxxTest {
protected:
    void SetUp() override {
        MixxxTest::SetUp();
    }

    void TearDown() override {
        MixxxTest::TearDown();
    }
};

// Test high-contrast color retrieval
TEST_F(HighContrastUtilsTest, TestGetHighContrastColor) {
    QColor color = HighContrastUtils::getHighContrastColor("background");
    EXPECT_FALSE(color.isValid() || color == QColor(0, 0, 0)); // Default black
}

// Test high-contrast mode status
TEST_F(HighContrastUtilsTest, TestHighContrastMode) {
    bool enabled = HighContrastUtils::isHighContrastModeEnabled();
    EXPECT_FALSE(enabled); // Should be disabled by default
    
    HighContrastUtils::setHighContrastModeEnabled(true);
    EXPECT_TRUE(HighContrastUtils::isHighContrastModeEnabled());
    
    HighContrastUtils::setHighContrastModeEnabled(false);
    EXPECT_FALSE(HighContrastUtils::isHighContrastModeEnabled());
}

// Test contrast ratio calculations
TEST_F(HighContrastUtilsTest, TestContrastRatio) {
    QColor white(255, 255, 255);
    QColor black(0, 0, 0);
    
    double ratio = HighContrastUtils::calculateContrastRatio(white, black);
    EXPECT_GT(ratio, 20.0); // Should have excellent contrast
    
    QColor red(255, 0, 0);
    QColor green(0, 255, 0);
    
    double ratio2 = HighContrastUtils::calculateContrastRatio(red, green);
    EXPECT_GT(ratio2, 4.5); // Should meet AA standard
}

// Test color conversion
TEST_F(HighContrastUtilsTest, TestColorConversion) {
    QColor original(128, 128, 128);
    QColor grayscale = HighContrastUtils::toGrayscale(original);
    
    // Grayscale should have equal RGB values
    EXPECT_EQ(grayscale.red(), grayscale.green());
    EXPECT_EQ(grayscale.green(), grayscale.blue());
    
    // Should be in the middle range
    EXPECT_GT(grayscale.red(), 0);
    EXPECT_LT(grayscale.red(), 255);
}

// Test readable text color
TEST_F(HighContrastUtilsTest, TestReadableTextColor) {
    QColor lightBackground(255, 255, 255);
    QColor darkBackground(0, 0, 0);
    
    QColor lightTextColor = HighContrastUtils::getReadableTextColor(lightBackground);
    QColor darkTextColor = HighContrastUtils::getReadableTextColor(darkBackground);
    
    // Light background should get dark text
    EXPECT_EQ(darkTextColor.red(), 0);
    EXPECT_EQ(darkTextColor.green(), 0);
    EXPECT_EQ(darkTextColor.blue(), 0);
    
    // Dark background should get light text  
    EXPECT_EQ(lightTextColor.red(), 255);
    EXPECT_EQ(lightTextColor.green(), 255);
    EXPECT_EQ(lightTextColor.blue(), 255);
}

// Test theme conversion
TEST_F(HighContrastUtilsTest, TestThemeConversion) {
    QString themeString = HighContrastUtils::themeToString(HighContrastUtils::Theme::HighContrast);
    EXPECT_EQ(themeString, "high-contrast");
    
    HighContrastUtils::Theme theme = HighContrastUtils::stringToTheme("black-on-white");
    EXPECT_EQ(theme, HighContrastUtils::Theme::BlackOnWhite);
    
    HighContrastUtils::Theme defaultTheme = HighContrastUtils::stringToTheme("invalid-theme");
    EXPECT_EQ(defaultTheme, HighContrastUtils::Theme::Default);
}
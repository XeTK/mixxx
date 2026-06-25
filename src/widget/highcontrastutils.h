#pragma once

#include <QObject>
#include <QColor>
#include <QMap>

/// Utility class for high-contrast theme support in Mixxx
class HighContrastUtils : public QObject {
    Q_OBJECT

public:
    explicit HighContrastUtils(QObject* parent = nullptr);
    ~HighContrastUtils() override;

    /// Get high-contrast color for a given role
    static QColor getHighContrastColor(const QString& role);

    /// Check if high-contrast mode is enabled
    static bool isHighContrastModeEnabled();

    /// Enable/disable high-contrast mode
    static void setHighContrastModeEnabled(bool enabled);

    /// Get high-contrast theme name
    static QString getHighContrastTheme();

    /// Apply high-contrast settings to a widget
    static void applyHighContrastTheme(QWidget* widget);

    /// Calculate contrast ratio between two colors
    static double calculateContrastRatio(const QColor& color1, const QColor& color2);

    /// Check if colors provide sufficient contrast
    static bool hasSufficientContrast(const QColor& foreground, const QColor& background);

    /// Convert color to grayscale for low vision users
    static QColor toGrayscale(const QColor& color);

    /// Get readable text color for a background
    static QColor getReadableTextColor(const QColor& background);

    /// Available high-contrast themes
    enum class Theme {
        Default,
        HighContrast,
        BlackOnWhite,
        WhiteOnBlack,
        BlueOnYellow
    };

    /// Get theme name as string
    static QString themeToString(Theme theme);

    /// Get theme from string
    static Theme stringToTheme(const QString& themeName);

private:
    static bool s_highContrastEnabled;
    static Theme s_currentTheme;
    static QMap<QString, QColor> s_highContrastColors;
};
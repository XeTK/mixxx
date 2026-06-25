#include "widget/highcontrastutils.h"

#include <QWidget>
#include <QColor>
#include <QMap>
#include <QApplication>
#include <QDebug>

// Static member definitions
bool HighContrastUtils::s_highContrastEnabled = false;
HighContrastUtils::Theme HighContrastUtils::s_currentTheme = HighContrastUtils::Theme::Default;
QMap<QString, QColor> HighContrastUtils::s_highContrastColors;

HighContrastUtils::HighContrastUtils(QObject* parent)
    : QObject(parent) {
    
    // Initialize high-contrast color palette
    s_highContrastColors["background"] = QColor(0, 0, 0);  // Black
    s_highContrastColors["foreground"] = QColor(255, 255, 255); // White
    s_highContrastColors["highlight"] = QColor(255, 255, 0);  // Yellow
    s_highContrastColors["selection"] = QColor(0, 0, 255);   // Blue
    s_highContrastColors["warning"] = QColor(255, 0, 0);     // Red
    s_highContrastColors["success"] = QColor(0, 255, 0);     // Green
    s_highContrastColors["info"] = QColor(0, 255, 255);      // Cyan
}

HighContrastUtils::~HighContrastUtils() = default;

QColor HighContrastUtils::getHighContrastColor(const QString& role) {
    if (s_highContrastColors.contains(role)) {
        return s_highContrastColors[role];
    }
    
    // Return default colors if not found
    return QColor(0, 0, 0); // Default black
}

bool HighContrastUtils::isHighContrastModeEnabled() {
    return s_highContrastEnabled;
}

void HighContrastUtils::setHighContrastModeEnabled(bool enabled) {
    s_highContrastEnabled = enabled;
    qDebug() << "High contrast mode" << (enabled ? "enabled" : "disabled");
}

QString HighContrastUtils::getHighContrastTheme() {
    return themeToString(s_currentTheme);
}

void HighContrastUtils::applyHighContrastTheme(QWidget* widget) {
    if (!widget || !s_highContrastEnabled) {
        return;
    }
    
    // Apply high-contrast colors to widget
    widget->setAutoFillBackground(true);
    widget->setPalette(QPalette(getHighContrastColor("background"), 
                               getHighContrastColor("foreground"),
                               getHighContrastColor("highlight"),
                               getHighContrastColor("selection"),
                               getHighContrastColor("warning"),
                               getHighContrastColor("success"),
                               getHighContrastColor("info")));
}

double HighContrastUtils::calculateContrastRatio(const QColor& color1, const QColor& color2) {
    // Based on WCAG 2.1 specification
    double l1 = (0.2126 * color1.red() + 0.7152 * color1.green() + 0.0722 * color1.blue()) / 255.0;
    double l2 = (0.2126 * color2.red() + 0.7152 * color2.green() + 0.0722 * color2.blue()) / 255.0;
    
    if (l1 > l2) {
        return (l1 + 0.05) / (l2 + 0.05);
    } else {
        return (l2 + 0.05) / (l1 + 0.05);
    }
}

bool HighContrastUtils::hasSufficientContrast(const QColor& foreground, const QColor& background) {
    double contrast = calculateContrastRatio(foreground, background);
    return contrast >= 4.5; // WCAG AA standard
}

QColor HighContrastUtils::toGrayscale(const QColor& color) {
    int gray = static_cast<int>(0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue());
    return QColor(gray, gray, gray);
}

QColor HighContrastUtils::getReadableTextColor(const QColor& background) {
    if (background.lightness() > 128) {
        // Light background -> dark text
        return QColor(0, 0, 0);
    } else {
        // Dark background -> light text
        return QColor(255, 255, 255);
    }
}

QString HighContrastUtils::themeToString(Theme theme) {
    switch (theme) {
        case Theme::Default:
            return "default";
        case Theme::HighContrast:
            return "high-contrast";
        case Theme::BlackOnWhite:
            return "black-on-white";
        case Theme::WhiteOnBlack:
            return "white-on-black";
        case Theme::BlueOnYellow:
            return "blue-on-yellow";
        default:
            return "default";
    }
}

HighContrastUtils::Theme HighContrastUtils::stringToTheme(const QString& themeName) {
    if (themeName == "high-contrast") {
        return Theme::HighContrast;
    } else if (themeName == "black-on-white") {
        return Theme::BlackOnWhite;
    } else if (themeName == "white-on-black") {
        return Theme::WhiteOnBlack;
    } else if (themeName == "blue-on-yellow") {
        return Theme::BlueOnYellow;
    } else {
        return Theme::Default;
    }
}
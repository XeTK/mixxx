#pragma once

#include <QKeyEvent>
#include <QMap>
#include <QObject>

class QWidget;

/// Utility class for handling keyboard navigation accessibility improvements
class KeyboardNavigation : public QObject {
    Q_OBJECT

  public:
    explicit KeyboardNavigation(QObject* parent = nullptr);
    ~KeyboardNavigation() override;

    /// Set up keyboard accessibility for a widget
    static void setupAccessibility(QWidget* widget);

    /// Check if a widget is accessible via keyboard navigation
    static bool isKeyboardAccessible(QWidget* widget);

    /// Get a human-readable name for a widget for accessibility
    static QString getAccessibleName(QWidget* widget);

    /// Handle keyboard navigation focus
    static bool handleFocusNavigation(QKeyEvent* event, QWidget* currentWidget);

    /// Enable/disable keyboard navigation hints
    static void setNavigationHintsEnabled(bool enabled);

    /// Check if navigation hints are enabled
    static bool navigationHintsEnabled();

  private:
    static bool s_navigationHintsEnabled;
    static QMap<QWidget*, QString> s_widgetNames;
};

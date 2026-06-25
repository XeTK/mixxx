#include "widget/keyboardnavigation.h"

#include <QApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QWidget>

bool KeyboardNavigation::s_navigationHintsEnabled = false;
QMap<QWidget*, QString> KeyboardNavigation::s_widgetNames;

KeyboardNavigation::KeyboardNavigation(QObject* parent)
        : QObject(parent) {
}

KeyboardNavigation::~KeyboardNavigation() = default;

void KeyboardNavigation::setupAccessibility(QWidget* widget) {
    if (!widget) {
        return;
    }

    // Set up accessible name if not already set
    if (widget->accessibleName().isEmpty()) {
        widget->setAccessibleName(widget->windowTitle());
    }

    // Connect to focus events for accessibility support
    QObject::connect(widget, &QWidget::focusReceived, [=]() {
        // Could add accessibility announcements here
        if (s_navigationHintsEnabled) {
            qDebug() << "Widget focused:" << widget->accessibleName();
        }
    });
}

bool KeyboardNavigation::isKeyboardAccessible(QWidget* widget) {
    if (!widget) {
        return false;
    }

    // Check if widget can receive focus
    if (!widget->focusPolicy() & (Qt::TabFocus | Qt::StrongFocus)) {
        return false;
    }

    // Check if widget is enabled
    if (!widget->isEnabled()) {
        return false;
    }

    // Check if widget is visible
    if (!widget->isVisible()) {
        return false;
    }

    // Check if widget has accessibility attributes
    return !widget->accessibleName().isEmpty() ||
            !widget->accessibleDescription().isEmpty();
}

QString KeyboardNavigation::getAccessibleName(QWidget* widget) {
    if (!widget) {
        return QString();
    }

    QString name = widget->accessibleName();
    if (!name.isEmpty()) {
        return name;
    }

    // Try to derive a name from window title or object name
    name = widget->windowTitle();
    if (!name.isEmpty()) {
        return name;
    }

    name = widget->objectName();
    if (!name.isEmpty()) {
        return name;
    }

    return QString("Unnamed Widget");
}

bool KeyboardNavigation::handleFocusNavigation(QKeyEvent* event, QWidget* currentWidget) {
    if (!currentWidget || !event) {
        return false;
    }

    // Handle tab navigation
    if (event->key() == Qt::Key_Tab) {
        // Default tab behavior should be preserved
        return false;
    }

    // Handle arrow keys for directional navigation
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down:
        // These might trigger custom handling in certain UI elements
        return false;
    default:
        return false;
    }
}

void KeyboardNavigation::setNavigationHintsEnabled(bool enabled) {
    s_navigationHintsEnabled = enabled;
}

bool KeyboardNavigation::navigationHintsEnabled() {
    return s_navigationHintsEnabled;
}

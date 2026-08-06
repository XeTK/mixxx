#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>
#include <memory>
#include <vector>

class ControlObject;
class ControlPushButton;
class ControlEncoder;

/// Drives a spoken popup menu for the non-skin UI (menus + preferences) from
/// a hardware controller (the Pioneer DDJ-400). A controller mapping drives
/// the `[AccessMenu]` control objects; this class owns them, keeps the
/// navigation state, speaks each highlighted item through the injected speak
/// callback, and emits `actionTriggered` for the main window to fire the real
/// action (issue #3).
///
/// The speak callback is injected (rather than a hard dependency on Library)
/// so unit tests can install a spy and the controller stays decoupled from
/// the library. In production CoreServices passes a lambda that calls
/// `Library::announceText`.
class AccessMenuController : public QObject {
    Q_OBJECT
  public:
    // Item types in the spoken menu tree.
    enum class ItemType {
        Submenu, // descends into a child menu
        Toggle,  // stay-open action (menu stays open after firing)
        Action,  // leaf action that closes the menu after firing
    };

    // A single menu item. `actionId` is a stable identifier emitted via
    // actionTriggered; "back" is reserved for the Back item every menu starts
    // with.
    struct Item {
        Item(ItemType type,
                const QString& label,
                const QString& actionId = QString(),
                std::vector<Item> children = {})
                : type(type),
                  label(label),
                  actionId(actionId),
                  children(std::move(children)) {
        }
        ItemType type;
        QString label;
        QString actionId;
        std::vector<Item> children;
    };

    // `speak` is called for every spoken utterance. May be empty (silent).
    AccessMenuController(std::function<void(const QString&)> speak,
            QObject* parent = nullptr);
    ~AccessMenuController() override;

    // Override the inactivity timeout (ms). Public so tests can shorten it
    // instead of waiting the full 30 s.
    void setTimeoutMs(int ms);

    // Public slots so tests can drive the controller directly without a live
    // controller mapping.
  public slots:
    void slotOpen();
    void slotClose();
    void slotNavigate(double value);
    void slotActivate();
    void slotBack();
    void slotConfirm();

  signals:
    // Emitted when a toggle/leaf action is confirmed, with a stable action id
    // (e.g. "toggleRecording", "pref_sound_hardware", "quit"). The main
    // window connects this to fire the real action (issue #3).
    void actionTriggered(const QString& actionId);

  private:
    struct MenuState {
        const std::vector<Item>* menu;
        int index;
    };

    void buildMenuTree();
    void speak(const QString& text);
    void openMenu();
    void closeMenu();
    void restartTimeout();
    void speakCurrentItem();
    void activateCurrentItem();
    void goBack();

    const std::vector<Item>* currentMenu() const;
    const Item* currentItem() const;

    std::function<void(const QString&)> m_speak;

    std::unique_ptr<ControlPushButton> m_pOpen;
    std::unique_ptr<ControlPushButton> m_pClose;
    std::unique_ptr<ControlEncoder> m_pNavigate;
    std::unique_ptr<ControlPushButton> m_pActivate;
    std::unique_ptr<ControlPushButton> m_pBack;
    std::unique_ptr<ControlPushButton> m_pConfirm;
    std::unique_ptr<ControlPushButton> m_pActive;

    std::vector<Item> m_root;
    // Path of open menus from root to the current menu. The current menu and
    // highlighted index are always m_stack.back().
    std::vector<MenuState> m_stack;
    bool m_open{false};
    QTimer m_timeout;
};

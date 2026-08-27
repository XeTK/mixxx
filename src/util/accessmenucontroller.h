#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>
#include <memory>
#include <vector>

#include "preferences/usersettings.h"

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
/// the library. In production MixxxMainWindow passes a lambda that calls
/// `Library::announceText`.
class AccessMenuController : public QObject {
    Q_OBJECT
  public:
    // Item types in the spoken menu tree.
    enum class ItemType {
        Submenu, // descends into a child menu
        Toggle,  // stay-open action (menu stays open after firing)
        Action,  // leaf action that closes the menu after firing
        Value,   // enters value-edit mode when activated (issue #32)
    };

    // How a Value item's current value is stepped and spoken.
    enum class ValueFormat {
        Boolean, // "on"/"off"
        Percent, // 0..1 -> "N percent"
        Integer, // raw integer
    };

    // A value-editable setting: either a control object (group + item) or a
    // config key (configGroup + configItem), plus how to step/format it. The
    // controller stays decoupled from the preferences dialog by operating on
    // these directly, so the same model can drive any numeric/boolean setting
    // that has a control or a config key.
    struct ValueItem {
        ValueItem() = default;
        // Control-backed value.
        ValueItem(const QString& label,
                const QString& group,
                const QString& item,
                double min,
                double max,
                double step,
                ValueFormat format)
                : label(label),
                  group(group),
                  item(item),
                  min(min),
                  max(max),
                  step(step),
                  format(format) {
        }
        // Config-backed value (no control object; e.g. TTS rate).
        ValueItem(const QString& label,
                const QString& configGroup,
                const QString& configItem,
                double min,
                double max,
                double step,
                ValueFormat format,
                bool configBacked)
                : label(label),
                  configGroup(configGroup),
                  configItem(configItem),
                  min(min),
                  max(max),
                  step(step),
                  format(format),
                  configBacked(configBacked) {
        }
        QString label;
        QString group;
        QString item;
        QString configGroup;
        QString configItem;
        double min{0.0};
        double max{1.0};
        double step{1.0};
        ValueFormat format{ValueFormat::Boolean};
        bool configBacked{false};
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
                  value(),
                  children(std::move(children)) {
        }
        Item(ItemType type, const ValueItem& value)
                : type(type),
                  label(value.label),
                  value(value) {
        }
        // A Toggle item that also carries a state descriptor (issue #57):
        // reusing the ValueItem's control/config plumbing lets the menu speak
        // the toggle's current on/off state the same way Value items speak
        // their current value, without a second read/format code path.
        Item(ItemType type,
                const QString& label,
                const QString& actionId,
                const ValueItem& stateValue)
                : type(type),
                  label(label),
                  actionId(actionId),
                  value(stateValue) {
        }
        ItemType type;
        QString label;
        QString actionId;
        ValueItem value;
        std::vector<Item> children;
    };

    // `speak` is called for every spoken utterance. May be empty (silent).
    // `pConfig` is optional; when provided, config-backed ValueItems (e.g.
    // TTS rate) can be read and written. Without it those items are read-only
    // at their default.
    AccessMenuController(std::function<void(const QString&)> speak,
            UserSettingsPointer pConfig = nullptr,
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
    // Opens the menu if closed, closes it if open. Used by the always-on
    // keyboard chord (issue #57), which has no notion of "menu focus" to
    // decide open vs. close the way a hardware hold-gesture does.
    void slotToggle();
    // Fullscreen has no ControlObject or config key backing its live state
    // (it's tracked purely as QWidget state on MixxxMainWindow), so the main
    // window pushes changes here for the Fullscreen toggle item to speak
    // (issue #57).
    void setFullScreenState(bool fullscreen);

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
    // Text that speakCurrentItem() would speak for the highlighted item,
    // without speaking it. Used to fold the highlighted item into the same
    // utterance as a preceding announcement (e.g. "Main menu") so the two
    // don't race as separate speak() calls -- see speakCurrentItem() and
    // issue #48 (case 2/3: a second speak() call fired synchronously right
    // after the first silently supersedes it before it can render).
    QString currentItemText() const;
    void speakCurrentItem();
    void activateCurrentItem();
    void goBack();
    // Formats the spoken state suffix for a Toggle item (e.g. "on"/"off"),
    // or an empty string if the item has no known state source.
    QString toggleStateText(const Item& item) const;

    // Value-edit mode (issue #32). Enter when a Value item is activated;
    // navigate steps the value, confirm/activate commits and exits,
    // back cancels.
    void enterValueEdit(const Item* item);
    void exitValueEdit();
    void stepValue(double delta);
    void commitValue();
    // Reads the live control value, clamped to [min, max]; writes a new value
    // back through the same control. These operate on an explicit ValueItem so
    // they work both while editing and when just announcing the current value
    // of a highlighted Value item.
    double readItemValue(const ValueItem& value) const;
    void writeItemValue(const ValueItem& value, double v) const;
    QString formatItemValue(const ValueItem& value, double v) const;
    // Convenience wrappers bound to the item currently being edited.
    double readValue() const;
    void writeValue(double value);

    const std::vector<Item>* currentMenu() const;
    const Item* currentItem() const;
    const Item* currentEditingItem() const;

    std::function<void(const QString&)> m_speak;
    UserSettingsPointer m_pConfig;

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
    // Live fullscreen state, pushed by MixxxMainWindow via
    // setFullScreenState() since there is no CO/config key to read it from.
    bool m_fullscreenState{false};

    // Value-edit state.
    bool m_editing{false};
    // Index within m_stack.back().menu of the Value item being edited. Only
    // valid while m_editing.
    int m_editingIndex{-1};
    // Value captured on entering edit mode, restored on cancel.
    double m_editStartValue{0.0};
};

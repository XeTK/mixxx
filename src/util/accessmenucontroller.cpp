#include "util/accessmenucontroller.h"

#include "control/controlencoder.h"
#include "control/controlpushbutton.h"
#include "moc_accessmenucontroller.cpp"

namespace {
constexpr int kDefaultTimeoutMs = 30000;
} // namespace

AccessMenuController::AccessMenuController(
        std::function<void(const QString&)> speak, QObject* parent)
        : QObject(parent),
          m_speak(std::move(speak)) {
    buildMenuTree();

    m_timeout.setSingleShot(true);
    m_timeout.setInterval(kDefaultTimeoutMs);
    connect(&m_timeout, &QTimer::timeout, this, &AccessMenuController::slotClose);

    // [AccessMenu],open: open the menu and speak "Main menu".
    m_pOpen = std::make_unique<ControlPushButton>(
            ConfigKey(QStringLiteral("[AccessMenu]"), QStringLiteral("open")));
    m_pOpen->setButtonMode(mixxx::control::ButtonMode::Trigger);
    connect(m_pOpen.get(),
            &ControlObject::valueChanged,
            this,
            [this](double value) {
                if (value > 0.0) {
                    slotOpen();
                }
            });

    // [AccessMenu],close: close the menu and speak "Menu closed".
    m_pClose = std::make_unique<ControlPushButton>(
            ConfigKey(QStringLiteral("[AccessMenu]"), QStringLiteral("close")));
    m_pClose->setButtonMode(mixxx::control::ButtonMode::Trigger);
    connect(m_pClose.get(),
            &ControlObject::valueChanged,
            this,
            [this](double value) {
                if (value > 0.0) {
                    slotClose();
                }
            });

    // [AccessMenu],navigate: +1 / -1 scrolls through the current menu.
    m_pNavigate = std::make_unique<ControlEncoder>(
            ConfigKey(QStringLiteral("[AccessMenu]"), QStringLiteral("navigate")));
    connect(m_pNavigate.get(),
            &ControlObject::valueChanged,
            this,
            &AccessMenuController::slotNavigate);

    // [AccessMenu],activate: descend into a submenu or fire a leaf action.
    m_pActivate = std::make_unique<ControlPushButton>(
            ConfigKey(QStringLiteral("[AccessMenu]"), QStringLiteral("activate")));
    m_pActivate->setButtonMode(mixxx::control::ButtonMode::Trigger);
    connect(m_pActivate.get(),
            &ControlObject::valueChanged,
            this,
            [this](double value) {
                if (value > 0.0) {
                    slotActivate();
                }
            });

    // [AccessMenu],back: up one level, or close at the top level.
    m_pBack = std::make_unique<ControlPushButton>(
            ConfigKey(QStringLiteral("[AccessMenu]"), QStringLiteral("back")));
    m_pBack->setButtonMode(mixxx::control::ButtonMode::Trigger);
    connect(m_pBack.get(),
            &ControlObject::valueChanged,
            this,
            [this](double value) {
                if (value > 0.0) {
                    slotBack();
                }
            });

    // [AccessMenu],confirm: fire the highlighted item (the LOAD buttons).
    m_pConfirm = std::make_unique<ControlPushButton>(
            ConfigKey(QStringLiteral("[AccessMenu]"), QStringLiteral("confirm")));
    m_pConfirm->setButtonMode(mixxx::control::ButtonMode::Trigger);
    connect(m_pConfirm.get(),
            &ControlObject::valueChanged,
            this,
            [this](double value) {
                if (value > 0.0) {
                    slotConfirm();
                }
            });

    // [AccessMenu],active: read-only state, 1 while the menu is open.
    m_pActive = std::make_unique<ControlPushButton>(
            ConfigKey(QStringLiteral("[AccessMenu]"), QStringLiteral("active")));
}

AccessMenuController::~AccessMenuController() = default;

void AccessMenuController::setTimeoutMs(int ms) {
    m_timeout.setInterval(ms);
}

void AccessMenuController::buildMenuTree() {
    // Preferences submenu. Action ids match the WMainMenuBar signal names so
    // issue #3 can wire them straight to the existing handlers.
    std::vector<Item> preferences;
    preferences.emplace_back(ItemType::Action, tr("Back"), QStringLiteral("back"));
    preferences.emplace_back(ItemType::Action,
            tr("Sound Hardware"),
            QStringLiteral("pref_sound_hardware"));
    preferences.emplace_back(ItemType::Action,
            tr("MIDI Controllers"),
            QStringLiteral("pref_midi_controllers"));
    preferences.emplace_back(ItemType::Action,
            tr("Accessibility"),
            QStringLiteral("pref_accessibility"));
    preferences.emplace_back(ItemType::Action,
            tr("Interface"),
            QStringLiteral("pref_interface"));
    preferences.emplace_back(ItemType::Action,
            tr("Decks"),
            QStringLiteral("pref_decks"));
    preferences.emplace_back(ItemType::Action,
            tr("Effects"),
            QStringLiteral("pref_effects"));
    preferences.emplace_back(ItemType::Action,
            tr("Library"),
            QStringLiteral("pref_library"));
    preferences.emplace_back(ItemType::Action,
            tr("Recording"),
            QStringLiteral("pref_recording"));
    preferences.emplace_back(ItemType::Action,
            tr("Broadcasting"),
            QStringLiteral("pref_broadcasting"));
    preferences.emplace_back(ItemType::Action,
            tr("Mixer"),
            QStringLiteral("pref_mixer"));
    preferences.emplace_back(ItemType::Action,
            tr("Waveform"),
            QStringLiteral("pref_waveform"));
    preferences.emplace_back(ItemType::Action,
            tr("Vinyl Control"),
            QStringLiteral("pref_vinyl_control"));

    // Every menu starts with a Back item.
    m_root.emplace_back(ItemType::Action, tr("Back"), QStringLiteral("back"));
    m_root.emplace_back(ItemType::Submenu,
            tr("Preferences"),
            QString(),
            std::move(preferences));
    m_root.emplace_back(ItemType::Toggle,
            tr("Recording"),
            QStringLiteral("toggleRecording"));
    m_root.emplace_back(ItemType::Toggle,
            tr("Broadcasting"),
            QStringLiteral("toggleBroadcasting"));
    m_root.emplace_back(ItemType::Toggle,
            tr("Speech on/off"),
            QStringLiteral("toggleTts"));
    m_root.emplace_back(ItemType::Toggle,
            tr("Fullscreen"),
            QStringLiteral("toggleFullScreen"));
    m_root.emplace_back(ItemType::Toggle,
            tr("Keyboard shortcuts"),
            QStringLiteral("toggleKeyboardShortcuts"));
    m_root.emplace_back(ItemType::Action,
            tr("Reload skin"),
            QStringLiteral("reloadSkin"));
    m_root.emplace_back(ItemType::Action,
            tr("Rescan library"),
            QStringLiteral("rescanLibrary"));
    m_root.emplace_back(ItemType::Action, tr("About"), QStringLiteral("showAbout"));
    m_root.emplace_back(ItemType::Action, tr("Quit"), QStringLiteral("quit"));
}

void AccessMenuController::speak(const QString& text) {
    if (m_speak) {
        m_speak(text);
    }
}

void AccessMenuController::openMenu() {
    if (m_open) {
        return;
    }
    m_open = true;
    m_pActive->set(1.0);
    m_stack.clear();
    m_stack.push_back({&m_root, 0});
    speak(tr("Main menu"));
    speakCurrentItem();
    restartTimeout();
}

void AccessMenuController::closeMenu() {
    if (!m_open) {
        return;
    }
    m_open = false;
    m_pActive->set(0.0);
    m_stack.clear();
    m_timeout.stop();
    speak(tr("Menu closed"));
}

void AccessMenuController::slotOpen() {
    openMenu();
}

void AccessMenuController::slotClose() {
    closeMenu();
}

void AccessMenuController::slotNavigate(double value) {
    if (!m_open) {
        return;
    }
    MenuState& state = m_stack.back();
    const int count = static_cast<int>(state.menu->size());
    if (count == 0) {
        return;
    }
    int delta = value > 0.0 ? 1 : (value < 0.0 ? -1 : 0);
    if (delta == 0) {
        return;
    }
    state.index = (state.index + delta + count) % count;
    speakCurrentItem();
    restartTimeout();
}

void AccessMenuController::slotActivate() {
    if (!m_open) {
        return;
    }
    activateCurrentItem();
    restartTimeout();
}

void AccessMenuController::slotBack() {
    if (!m_open) {
        return;
    }
    goBack();
    restartTimeout();
}

void AccessMenuController::slotConfirm() {
    if (!m_open) {
        return;
    }
    activateCurrentItem();
    restartTimeout();
}

void AccessMenuController::restartTimeout() {
    m_timeout.start();
}

void AccessMenuController::speakCurrentItem() {
    const Item* item = currentItem();
    if (!item) {
        return;
    }
    if (item->type == ItemType::Submenu) {
        speak(tr("%1, submenu").arg(item->label));
    } else {
        speak(item->label);
    }
}

void AccessMenuController::activateCurrentItem() {
    const Item* item = currentItem();
    if (!item) {
        return;
    }
    switch (item->type) {
    case ItemType::Submenu:
        // Descend into the submenu, highlighting its first (Back) item.
        m_stack.push_back({&item->children, 0});
        speakCurrentItem();
        break;
    case ItemType::Toggle:
        // Stay-open: fire the action but keep the menu open.
        emit actionTriggered(item->actionId);
        break;
    case ItemType::Action:
        if (item->actionId == QStringLiteral("back")) {
            goBack();
            return;
        }
        emit actionTriggered(item->actionId);
        closeMenu();
        break;
    }
}

void AccessMenuController::goBack() {
    if (m_stack.size() > 1) {
        m_stack.pop_back();
        speakCurrentItem();
    } else {
        closeMenu();
    }
}

const std::vector<AccessMenuController::Item>* AccessMenuController::currentMenu() const {
    if (m_stack.empty()) {
        return nullptr;
    }
    return m_stack.back().menu;
}

const AccessMenuController::Item* AccessMenuController::currentItem() const {
    const auto* menu = currentMenu();
    if (!menu || menu->empty()) {
        return nullptr;
    }
    const MenuState& state = m_stack.back();
    if (state.index < 0 || state.index >= static_cast<int>(menu->size())) {
        return nullptr;
    }
    return &(*menu)[state.index];
}

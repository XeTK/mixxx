#include "util/accessmenucontroller.h"

#include <algorithm>
#include <cmath>

#include "control/controlencoder.h"
#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "control/controlpushbutton.h"
#include "moc_accessmenucontroller.cpp"
#include "util/ttsengine.h"

namespace {
constexpr int kDefaultTimeoutMs = 30000;

// Control objects / config keys the value editor reads/writes. These are the
// same keys the engine and preference pages use, so edits take effect
// immediately and are persisted by the owning engine. Kept in one place so
// the menu tree and the restore-on-cancel logic stay in sync.
struct ValueControl {
    const char* group;      // control group, or config group when configBacked
    const char* item;       // control item, or config item when configBacked
    double min;
    double max;
    double step;
    AccessMenuController::ValueFormat format;
    bool configBacked{false};
};

const ValueControl kValueControls[] = {
        // Speech on/off. [Tts],enabled is a Toggle push button; writing 0/1
        // flips it to the requested state.
        {"[Tts]", "enabled", 0.0, 1.0, 1.0,
                AccessMenuController::ValueFormat::Boolean},
        // Speech rate, -10..10, 0 = normal. Stored as a config key
        // ([Accessibility],TtsRate); AnnouncementManager reads it on every
        // utterance, so writing the config key takes effect on the next speak.
        {"[Accessibility]", "TtsRate", -10.0, 10.0, 1.0,
                AccessMenuController::ValueFormat::Integer, true},
        // Music ducking strength while speech is spoken, 0..1. [Tts],
        // duckStrength is a plain control; the engine persists it.
        {"[Tts]", "duckStrength", 0.0, 1.0, 0.05,
                AccessMenuController::ValueFormat::Percent},
        // Beat-click metronome volume, 0..1.
        {"[BeatClick]", "volume", 0.0, 1.0, 0.05,
                AccessMenuController::ValueFormat::Percent},
};

// Default TTS voice list (issue #128): queries the live platform speech
// backend, same as DlgPrefAccessibility::populateVoiceCombo(). Wrapped in a
// VoiceListProvider so tests can inject a fixed list instead (see the header
// comment on VoiceListProvider).
std::vector<AccessMenuController::ValueItem::ValueOption> defaultVoiceList() {
    std::vector<AccessMenuController::ValueItem::ValueOption> options;
    for (const TtsEngine::Voice& voice : TtsEngine::enumerateVoices()) {
        options.push_back({voice.id, voice.displayName});
    }
    return options;
}
} // namespace

AccessMenuController::AccessMenuController(
        std::function<void(const QString&)> speak,
        UserSettingsPointer pConfig,
        QObject* parent,
        VoiceListProvider voiceListProvider)
        : QObject(parent),
          m_speak(std::move(speak)),
          m_pConfig(std::move(pConfig)),
          m_voiceListProvider(voiceListProvider ? std::move(voiceListProvider)
                                                 : &defaultVoiceList) {
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
    //
    // bIgnoreNops must be false here (issue #106): the real DDJ-400 browse
    // knob always reports a detent as a flat +1/-1 (see browseRotate() in
    // Pioneer-DDJ-400-script.js, which clamps the decoded delta to exactly
    // +/-1 before writing it), so turning the knob several ticks in the same
    // direction writes the *same* value repeatedly. With the ControlEncoder
    // default (bIgnoreNops = true), every write after the first same-value
    // one is silently dropped as a no-op and slotNavigate() never runs --
    // multi-tick navigation (and therefore wrap-around, which requires
    // multiple same-direction ticks to reach) stops working after a single
    // step. The unit tests below dodge this by growing the magnitude of each
    // tick so it is never equal to the last (see the `navigate()` test
    // helper's comment), which is why they passed while this was broken on
    // real hardware.
    m_pNavigate = std::make_unique<ControlEncoder>(
            ConfigKey(QStringLiteral("[AccessMenu]"), QStringLiteral("navigate")),
            /*bIgnoreNops=*/false);
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

    // Value editor submenu (issue #32): numeric/boolean settings the browse
    // knob can change and hear. Each item enters value-edit mode when
    // activated.
    std::vector<Item> values;
    values.emplace_back(ItemType::Action, tr("Back"), QStringLiteral("back"));
    for (const ValueControl& vc : kValueControls) {
        if (vc.configBacked) {
            values.emplace_back(ItemType::Value,
                    ValueItem(QString(),
                            QString::fromLatin1(vc.group),
                            QString::fromLatin1(vc.item),
                            vc.min,
                            vc.max,
                            vc.step,
                            vc.format,
                            true));
        } else {
            values.emplace_back(ItemType::Value,
                    ValueItem(QString(),
                            QString::fromLatin1(vc.group),
                            QString::fromLatin1(vc.item),
                            vc.min,
                            vc.max,
                            vc.step,
                            vc.format));
        }
    }
    // Replace the placeholder labels with the real spoken names.
    values[1].label = tr("Speech on/off");
    values[2].label = tr("Speech rate");
    values[3].label = tr("Ducking strength");
    values[4].label = tr("Beat click volume");

    // TTS voice (issue #128): an Options-format entry, since a voice is
    // picked by name from a platform-reported list rather than stepped
    // through a numeric range. "Default (system voice)" always comes first
    // (index 0, stored value ""), matching
    // DlgPrefAccessibility::refreshFilteredVoiceCombo()'s combo box, so
    // clamping/defaulting behaves the same way in both places.
    std::vector<ValueItem::ValueOption> voiceOptions;
    voiceOptions.push_back({QString(), tr("Default (system voice)")});
    if (m_voiceListProvider) {
        for (ValueItem::ValueOption& option : m_voiceListProvider()) {
            voiceOptions.push_back(std::move(option));
        }
    }
    values.emplace_back(ItemType::Value,
            ValueItem(tr("TTS voice"),
                    QStringLiteral("[Accessibility]"),
                    QStringLiteral("TtsVoice"),
                    std::move(voiceOptions)));

    // Every menu starts with a Back item.
    m_root.emplace_back(ItemType::Action, tr("Back"), QStringLiteral("back"));
    m_root.emplace_back(ItemType::Submenu,
            tr("Preferences"),
            QString(),
            std::move(preferences));
    m_root.emplace_back(ItemType::Submenu,
            tr("Values"),
            QString(),
            std::move(values));
    // Toggle items carry a state descriptor (issue #57) so speakCurrentItem()
    // can append the current on/off state, e.g. "Recording, on". They reuse
    // the same control/config-backed ValueItem plumbing the Values submenu
    // uses to read and format Boolean values.
    m_root.emplace_back(ItemType::Toggle,
            tr("Recording"),
            QStringLiteral("toggleRecording"),
            ValueItem(QString(),
                    QStringLiteral("[Recording]"),
                    QStringLiteral("status"),
                    0.0,
                    2.0,
                    1.0,
                    ValueFormat::Boolean));
    m_root.emplace_back(ItemType::Toggle,
            tr("Broadcasting"),
            QStringLiteral("toggleBroadcasting"),
            ValueItem(QString(),
                    QStringLiteral("[Shoutcast]"),
                    QStringLiteral("enabled"),
                    0.0,
                    1.0,
                    1.0,
                    ValueFormat::Boolean));
    m_root.emplace_back(ItemType::Toggle,
            tr("Speech on/off"),
            QStringLiteral("toggleTts"),
            ValueItem(QString(),
                    QStringLiteral("[Tts]"),
                    QStringLiteral("enabled"),
                    0.0,
                    1.0,
                    1.0,
                    ValueFormat::Boolean));
    // Fullscreen has no CO/config key; its state is pushed in from
    // MixxxMainWindow via setFullScreenState() and read from m_fullscreenState
    // directly in toggleStateText(), so it gets no ValueItem here.
    m_root.emplace_back(ItemType::Toggle,
            tr("Fullscreen"),
            QStringLiteral("toggleFullScreen"));
    m_root.emplace_back(ItemType::Toggle,
            tr("Keyboard shortcuts"),
            QStringLiteral("toggleKeyboardShortcuts"),
            ValueItem(QString(),
                    QStringLiteral("[Keyboard]"),
                    QStringLiteral("Enabled"),
                    0.0,
                    1.0,
                    1.0,
                    ValueFormat::Boolean,
                    /*configBacked=*/true));
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
    // Speak "Main menu" and the highlighted item as one utterance: two
    // separate speak() calls here let the second silently supersede the
    // first before it can render (issue #48, case 2).
    const QString itemText = currentItemText();
    speak(itemText.isEmpty() ? tr("Main menu") : tr("Main menu. %1").arg(itemText));
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
    int delta = value > 0.0 ? 1 : (value < 0.0 ? -1 : 0);
    if (delta == 0) {
        return;
    }
    if (m_editing) {
        // In value-edit mode the browse knob changes the value, not the
        // selection, and each change is spoken (issue #32).
        stepValue(delta);
        restartTimeout();
        return;
    }
    MenuState& state = m_stack.back();
    const int count = static_cast<int>(state.menu->size());
    if (count == 0) {
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
    if (m_editing) {
        // In value-edit mode activate commits the current value and exits.
        commitValue();
        restartTimeout();
        return;
    }
    activateCurrentItem();
    restartTimeout();
}

void AccessMenuController::slotBack() {
    if (!m_open) {
        return;
    }
    if (m_editing) {
        // Back cancels the edit, restoring the value captured on entry.
        exitValueEdit();
        restartTimeout();
        return;
    }
    goBack();
    restartTimeout();
}

void AccessMenuController::slotConfirm() {
    if (!m_open) {
        return;
    }
    if (m_editing) {
        // Confirm commits the current value and exits, like activate.
        commitValue();
        restartTimeout();
        return;
    }
    activateCurrentItem();
    restartTimeout();
}

void AccessMenuController::restartTimeout() {
    m_timeout.start();
}

void AccessMenuController::slotToggle() {
    if (m_open) {
        closeMenu();
    } else {
        openMenu();
    }
}

void AccessMenuController::setFullScreenState(bool fullscreen) {
    m_fullscreenState = fullscreen;
}

QString AccessMenuController::currentItemText() const {
    const Item* item = currentItem();
    if (!item) {
        return QString();
    }
    if (item->type == ItemType::Submenu) {
        return tr("%1, submenu").arg(item->label);
    } else if (item->type == ItemType::Value) {
        // Label and the current value, e.g. "Speech rate, 0".
        return tr("%1, %2")
                .arg(item->label,
                        formatItemValue(item->value, readItemValue(item->value)));
    } else if (item->type == ItemType::Toggle) {
        // Label and the current on/off state, e.g. "Recording, on" (issue
        // #57), so the DJ doesn't have to already know the state or trigger
        // it and listen for a side effect.
        const QString state = toggleStateText(*item);
        if (state.isEmpty()) {
            return item->label;
        }
        return tr("%1, %2").arg(item->label, state);
    }
    return item->label;
}

void AccessMenuController::speakCurrentItem() {
    const QString text = currentItemText();
    if (!text.isEmpty()) {
        speak(text);
    }
}

QString AccessMenuController::toggleStateText(const Item& item) const {
    if (item.actionId == QStringLiteral("toggleFullScreen")) {
        return m_fullscreenState ? tr("on") : tr("off");
    }
    if (item.value.group.isEmpty() && !item.value.configBacked) {
        // No known state source for this toggle; speak the label alone.
        return QString();
    }
    return formatItemValue(item.value, readItemValue(item.value));
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
        // Warn BEFORE disabling keyboard shortcuts (issue #57): once they're
        // off, kbd.cfg-driven bindings go dead, so this is the last moment a
        // keyboard-only DJ can hear that it's about to happen. (The
        // AccessMenu's own keyboard chords are wired as application-wide Qt
        // shortcuts specifically so they survive this and stay usable
        // afterwards to turn shortcuts back on.)
        if (item->actionId == QStringLiteral("toggleKeyboardShortcuts") &&
                readItemValue(item->value) > 0.0) {
            speak(tr("Keyboard shortcuts now off. Use this menu or the "
                     "mouse to re-enable them."));
        }
        // Stay-open: fire the action but keep the menu open.
        emit actionTriggered(item->actionId);
        break;
    case ItemType::Value:
        // Enter value-edit mode: the browse knob now changes the value and
        // each change is spoken (issue #32).
        enterValueEdit(item);
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

void AccessMenuController::enterValueEdit(const Item* item) {
    if (!item || item->type != ItemType::Value) {
        return;
    }
    m_editing = true;
    m_editingIndex = m_stack.back().index;
    m_editStartValue = readItemValue(item->value);
    // Announce the mode and the starting value in one utterance so the DJ
    // knows the knob now edits instead of scrolling; two separate speak()
    // calls here let the value silently supersede the instructions before
    // they can render (issue #48, case 3).
    speak(tr("%1. Turn to change, confirm to set, back to cancel. %2")
                    .arg(item->label, formatItemValue(item->value, m_editStartValue)));
}

void AccessMenuController::exitValueEdit() {
    if (!m_editing) {
        return;
    }
    // Cancel: restore the value captured on entry.
    writeValue(m_editStartValue);
    m_editing = false;
    m_editingIndex = -1;
    // One utterance for the confirmation and the re-highlighted item (issue
    // #48, case 3): calling speak() separately for each let the item name
    // silently supersede "Cancelled" before it could render.
    const QString itemText = currentItemText();
    speak(itemText.isEmpty() ? tr("Cancelled") : tr("Cancelled. %1").arg(itemText));
}

void AccessMenuController::stepValue(double delta) {
    const Item* item = currentEditingItem();
    if (!item) {
        return;
    }
    const double current = readItemValue(item->value);
    double next;
    if (item->value.format == ValueFormat::Boolean) {
        // Booleans toggle on each tick: a blink DJ hears "off"/"on" with
        // every navigate.
        next = current > 0.0 ? item->value.min : item->value.max;
        (void)delta;
    } else {
        next = current + delta * item->value.step;
        if (next < item->value.min) {
            next = item->value.min;
        } else if (next > item->value.max) {
            next = item->value.max;
        }
    }
    if (next == current) {
        // Already at a boundary; still say where we are so the DJ knows the
        // knob is doing something.
        speak(formatItemValue(item->value, next));
        return;
    }
    writeItemValue(item->value, next);
    speak(formatItemValue(item->value, next));
}

void AccessMenuController::commitValue() {
    if (!m_editing) {
        return;
    }
    m_editing = false;
    m_editingIndex = -1;
    // One utterance for the confirmation and the re-highlighted item; see
    // exitValueEdit() above (issue #48, case 3).
    const QString itemText = currentItemText();
    speak(itemText.isEmpty() ? tr("Set") : tr("Set. %1").arg(itemText));
}

const AccessMenuController::Item* AccessMenuController::currentEditingItem() const {
    if (!m_editing) {
        return nullptr;
    }
    const auto* menu = currentMenu();
    if (!menu || m_editingIndex < 0 ||
            m_editingIndex >= static_cast<int>(menu->size())) {
        return nullptr;
    }
    const Item& item = (*menu)[m_editingIndex];
    return item.type == ItemType::Value ? &item : nullptr;
}

double AccessMenuController::readValue() const {
    const Item* item = currentEditingItem();
    if (!item) {
        return 0.0;
    }
    return readItemValue(item->value);
}

void AccessMenuController::writeValue(double value) {
    const Item* item = currentEditingItem();
    if (!item) {
        return;
    }
    writeItemValue(item->value, value);
}

double AccessMenuController::readItemValue(const ValueItem& value) const {
    if (value.format == ValueFormat::Options) {
        // Options values are stored as a QString (e.g. a TTS voice ID), not
        // a double, so they can't go through the generic getValue<double>()
        // path below. Read the stored string and translate it back to its
        // option's index; an unrecognized or missing string (including the
        // empty string used for "no explicit voice chosen yet") reads as
        // index 0, the "Default" option that's always first (issue #128).
        if (value.options.empty()) {
            return 0.0;
        }
        const QString stored = value.configBacked && m_pConfig
                ? m_pConfig->getValue<QString>(
                          ConfigKey(value.configGroup, value.configItem), QString())
                : QString();
        for (std::size_t i = 0; i < value.options.size(); ++i) {
            if (value.options[i].storedValue == stored) {
                return static_cast<double>(i);
            }
        }
        return 0.0;
    }
    double v;
    if (value.configBacked) {
        v = m_pConfig ? m_pConfig->getValue<double>(
                                ConfigKey(value.configGroup, value.configItem),
                                value.min)
                      : value.min;
    } else {
        // Use a proxy so a control that does not exist yet (e.g. before the
        // engine creates it) reads as the clamped default instead of asserting.
        ControlProxy proxy(value.group,
                value.item,
                nullptr,
                ControlFlag::NoWarnIfMissing);
        v = proxy.valid() ? proxy.get() : value.min;
    }
    return std::clamp(v, value.min, value.max);
}

void AccessMenuController::writeItemValue(const ValueItem& value, double v) const {
    if (value.format == ValueFormat::Options) {
        if (value.options.empty() || !value.configBacked || !m_pConfig) {
            return;
        }
        const int index = std::clamp(static_cast<int>(std::lround(v)),
                0,
                static_cast<int>(value.options.size()) - 1);
        m_pConfig->setValue(ConfigKey(value.configGroup, value.configItem),
                value.options[static_cast<std::size_t>(index)].storedValue);
        return;
    }
    if (value.configBacked) {
        if (m_pConfig) {
            m_pConfig->setValue(ConfigKey(value.configGroup, value.configItem), v);
        }
        return;
    }
    ControlProxy proxy(value.group,
            value.item,
            nullptr,
            ControlFlag::NoWarnIfMissing);
    if (proxy.valid()) {
        proxy.set(v);
    }
}

QString AccessMenuController::formatItemValue(
        const ValueItem& value, double v) const {
    switch (value.format) {
    case ValueFormat::Boolean:
        return v > 0.0 ? tr("on") : tr("off");
    case ValueFormat::Percent:
        return tr("%1 percent").arg(QString::number(
                static_cast<int>(std::lround(v * 100.0))));
    case ValueFormat::Integer:
        return QString::number(static_cast<int>(std::lround(v)));
    case ValueFormat::Options: {
        if (value.options.empty()) {
            return QString();
        }
        const int index = std::clamp(static_cast<int>(std::lround(v)),
                0,
                static_cast<int>(value.options.size()) - 1);
        return value.options[static_cast<std::size_t>(index)].label;
    }
    }
    return QString();
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

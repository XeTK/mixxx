#include "util/accessmenucontroller.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QStringList>
#include <QTest>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "test/mixxxtest.h"

namespace {

// Spy speak callback: records every utterance.
class SpeakSpy {
  public:
    void operator()(const QString& text) {
        m_texts.append(text);
    }
    QStringList m_texts;
};

} // namespace

class AccessMenuControllerTest : public MixxxTest {
  protected:
    void SetUp() override {
        m_pSpy = std::make_unique<SpeakSpy>();
        // The spy must outlive the controller, which holds a copy of the
        // std::function pointing at it.
        m_pController = std::make_unique<AccessMenuController>(
                [this](const QString& text) { (*m_pSpy)(text); },
                config());
        m_pController->setTimeoutMs(100);
    }

    // Drive a trigger control like the DDJ-400 script does.
    void press(const QString& control) {
        ControlProxy proxy(QStringLiteral("[AccessMenu]"),
                control,
                nullptr,
                ControlFlag::AllowMissingOrInvalid);
        proxy.set(1.0);
        QCoreApplication::processEvents();
    }

    void navigate(double value) {
        ControlProxy proxy(QStringLiteral("[AccessMenu]"),
                QStringLiteral("navigate"),
                nullptr,
                ControlFlag::AllowMissingOrInvalid);
        // The encoder ignores no-ops, so each tick must be a distinct value
        // (a real controller sends +1/-1 ticks). Keep the sign but grow the
        // magnitude so consecutive same-direction ticks are never equal.
        m_tick++;
        proxy.set(value > 0.0 ? m_tick : -m_tick);
        QCoreApplication::processEvents();
    }

    double active() const {
        ControlProxy proxy(QStringLiteral("[AccessMenu]"),
                QStringLiteral("active"),
                nullptr,
                ControlFlag::AllowMissingOrInvalid);
        return proxy.get();
    }

    void clearSpy() {
        m_pSpy->m_texts.clear();
    }

    std::unique_ptr<SpeakSpy> m_pSpy;
    std::unique_ptr<AccessMenuController> m_pController;
    int m_tick{0};
};

TEST_F(AccessMenuControllerTest, Open_SpeaksMainMenuAndSetsActive) {
    press(QStringLiteral("open"));

    EXPECT_EQ(1.0, active());
    // "Main menu" and the first highlighted item (Back) are spoken as one
    // utterance (issue #48): two separate speak() calls here used to let the
    // second silently supersede the first before it could render.
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Main menu. Back", m_pSpy->m_texts.at(0));
}

TEST_F(AccessMenuControllerTest, Navigate_ScrollsAndSpeaksEachItem) {
    press(QStringLiteral("open"));
    clearSpy();

    navigate(1.0); // Preferences
    navigate(1.0); // Values
    navigate(1.0); // Recording
    navigate(1.0); // Broadcasting

    ASSERT_GE(m_pSpy->m_texts.size(), 4);
    EXPECT_QSTRING_EQ("Preferences, submenu", m_pSpy->m_texts.at(0));
    EXPECT_QSTRING_EQ("Values, submenu", m_pSpy->m_texts.at(1));
    // Toggle items speak their current state (issue #57); neither control
    // exists in this test so both read as off.
    EXPECT_QSTRING_EQ("Recording, off", m_pSpy->m_texts.at(2));
    EXPECT_QSTRING_EQ("Broadcasting, off", m_pSpy->m_texts.at(3));

    // Negative scroll goes back up.
    clearSpy();
    navigate(-1.0);
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Recording, off", m_pSpy->m_texts.at(0));
}

TEST_F(AccessMenuControllerTest, Navigate_WrapsAround) {
    press(QStringLiteral("open"));
    clearSpy();

    // The root menu has 12 items; 12 ticks wraps back to the first (Back).
    for (int i = 0; i < 12; ++i) {
        navigate(1.0);
    }
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Back", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));
}

// Regression test for issue #106: drive [AccessMenu],navigate the way the
// real DDJ-400 browse knob actually does -- every same-direction detent
// writes the *identical* raw value (+1 or -1; see browseRotate() in
// Pioneer-DDJ-400-script.js, which clamps the decoded delta to exactly
// +/-1), not a monotonically growing magnitude. The `navigate()` helper
// above deliberately dodges this by growing the tick magnitude so consecutive
// same-direction ticks are never equal -- which is exactly why the tests
// above kept passing while turning the real browse knob multiple detents in
// the same direction did nothing past the first tick on real hardware
// (ControlEncoder's default bIgnoreNops = true silently drops every
// same-value write after the first).
TEST_F(AccessMenuControllerTest, Navigate_RepeatedIdenticalTicksLikeRealHardware) {
    press(QStringLiteral("open"));
    clearSpy();

    ControlProxy proxy(QStringLiteral("[AccessMenu]"),
            QStringLiteral("navigate"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);

    // Three consecutive down-ticks, each writing the exact same value (1),
    // exactly as the real hardware does. All three must move the selection.
    proxy.set(1.0);
    QCoreApplication::processEvents();
    proxy.set(1.0);
    QCoreApplication::processEvents();
    proxy.set(1.0);
    QCoreApplication::processEvents();

    ASSERT_GE(m_pSpy->m_texts.size(), 3);
    EXPECT_QSTRING_EQ("Preferences, submenu", m_pSpy->m_texts.at(0));
    EXPECT_QSTRING_EQ("Values, submenu", m_pSpy->m_texts.at(1));
    EXPECT_QSTRING_EQ("Recording, off", m_pSpy->m_texts.at(2));

    // Twelve identical down-ticks wrap all the way back to the first item.
    clearSpy();
    for (int i = 0; i < 9; ++i) {
        proxy.set(1.0);
        QCoreApplication::processEvents();
    }
    ASSERT_GE(m_pSpy->m_texts.size(), 9);
    EXPECT_QSTRING_EQ("Back", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));
}

TEST_F(AccessMenuControllerTest, Activate_DescendsIntoSubmenu) {
    press(QStringLiteral("open"));
    clearSpy();

    navigate(1.0); // Preferences
    clearSpy();
    press(QStringLiteral("activate"));

    // Descends and speaks the first item of the submenu (Back).
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Back", m_pSpy->m_texts.at(0));
}

TEST_F(AccessMenuControllerTest, Back_GoesUpOneLevel) {
    press(QStringLiteral("open"));
    clearSpy();

    navigate(1.0); // Preferences
    press(QStringLiteral("activate")); // into submenu
    clearSpy();

    press(QStringLiteral("back"));
    // Back up to the top level, speaking the current (Preferences) item.
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Preferences, submenu", m_pSpy->m_texts.at(0));
    EXPECT_EQ(1.0, active());
}

TEST_F(AccessMenuControllerTest, Back_AtTopClosesMenu) {
    press(QStringLiteral("open"));
    clearSpy();

    press(QStringLiteral("back"));
    EXPECT_EQ(0.0, active());
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Menu closed", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));
}

TEST_F(AccessMenuControllerTest, Confirm_FiresAction) {
    QStringList triggered;
    QObject::connect(m_pController.get(),
            &AccessMenuController::actionTriggered,
            m_pController.get(),
            [&triggered](const QString& id) { triggered.append(id); });

    press(QStringLiteral("open"));
    clearSpy();

    navigate(1.0); // Preferences
    navigate(1.0); // Values
    navigate(1.0); // Recording
    press(QStringLiteral("confirm"));

    ASSERT_EQ(1, triggered.size());
    EXPECT_QSTRING_EQ("toggleRecording", triggered.at(0));
}

TEST_F(AccessMenuControllerTest, ToggleActions_StayOpen) {
    QStringList triggered;
    QObject::connect(m_pController.get(),
            &AccessMenuController::actionTriggered,
            m_pController.get(),
            [&triggered](const QString& id) { triggered.append(id); });

    press(QStringLiteral("open"));
    clearSpy();

    navigate(1.0); // Preferences
    navigate(1.0); // Values
    navigate(1.0); // Recording
    press(QStringLiteral("confirm"));

    // Toggle stays open.
    EXPECT_EQ(1.0, active());
    EXPECT_EQ(1, triggered.size());
    EXPECT_QSTRING_EQ("toggleRecording", triggered.at(0));
}

TEST_F(AccessMenuControllerTest, LeafAction_ClosesMenu) {
    QStringList triggered;
    QObject::connect(m_pController.get(),
            &AccessMenuController::actionTriggered,
            m_pController.get(),
            [&triggered](const QString& id) { triggered.append(id); });

    press(QStringLiteral("open"));
    clearSpy();

    // Navigate to Quit (last item, index 11 of 12).
    for (int i = 0; i < 11; ++i) {
        navigate(1.0);
    }
    press(QStringLiteral("confirm"));

    EXPECT_EQ(0.0, active());
    ASSERT_EQ(1, triggered.size());
    EXPECT_QSTRING_EQ("quit", triggered.at(0));
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Menu closed", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));
}

TEST_F(AccessMenuControllerTest, BackItem_FiresBackNotAction) {
    QStringList triggered;
    QObject::connect(m_pController.get(),
            &AccessMenuController::actionTriggered,
            m_pController.get(),
            [&triggered](const QString& id) { triggered.append(id); });

    press(QStringLiteral("open"));
    clearSpy();

    // First item is Back; confirming it should go back (close at top), not
    // emit an action.
    press(QStringLiteral("confirm"));
    EXPECT_EQ(0.0, active());
    EXPECT_EQ(0, triggered.size());
}

TEST_F(AccessMenuControllerTest, Timeout_ClosesMenu) {
    press(QStringLiteral("open"));
    clearSpy();

    // Wait past the (shortened) timeout.
    QTest::qWait(200);
    QCoreApplication::processEvents();

    EXPECT_EQ(0.0, active());
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Menu closed", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));
}

TEST_F(AccessMenuControllerTest, Close_SpeaksMenuClosedAndClearsActive) {
    press(QStringLiteral("open"));
    clearSpy();

    press(QStringLiteral("close"));
    EXPECT_EQ(0.0, active());
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Menu closed", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));
}

TEST_F(AccessMenuControllerTest, NavigateWhenClosed_IsIgnored) {
    clearSpy();
    navigate(1.0);
    EXPECT_EQ(0, m_pSpy->m_texts.size());
    EXPECT_EQ(0.0, active());
}

// -- slotToggle() (issue #57) --------------------------------------------

TEST_F(AccessMenuControllerTest, SlotToggle_OpensThenCloses) {
    // Drives the always-on keyboard chord directly, bypassing the
    // [AccessMenu] control objects (the way the app-wide QAction wiring in
    // MixxxMainWindow does), since there's no dedicated CO for a combined
    // open/close toggle.
    m_pController->slotToggle();
    EXPECT_EQ(1.0, active());

    m_pController->slotToggle();
    EXPECT_EQ(0.0, active());
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Menu closed", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));
}

// -- Toggle items speak their state (issue #57) ---------------------------

class ToggleStateTest : public AccessMenuControllerTest {
  protected:
    // Navigate from a freshly-opened root menu to the toggle item at
    // `itemIndex` (3 = Recording, 4 = Broadcasting, 5 = Speech on/off,
    // 6 = Fullscreen, 7 = Keyboard shortcuts).
    void openRootItem(int itemIndex) {
        press(QStringLiteral("open"));
        clearSpy();
        for (int i = 0; i < itemIndex; ++i) {
            navigate(1.0);
        }
    }

    // Make the controller speak the currently highlighted item again. A single
    // tick always moves the selection (there is no "repeat" control), so step
    // to the neighbouring item and back; the spy is cleared in between, so only
    // the second announcement of the original item is recorded.
    void respeakCurrentItem() {
        navigate(1.0);
        clearSpy();
        navigate(-1.0);
    }
};

TEST_F(ToggleStateTest, Recording_SpeaksOnState) {
    auto pStatus = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Recording]"), QStringLiteral("status")));
    pStatus->set(2.0); // RECORD_ON
    openRootItem(3);

    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Recording, on", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));
}

TEST_F(ToggleStateTest, Tts_SpeaksOffThenOnState) {
    auto pEnabled = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Tts]"), QStringLiteral("enabled")));
    pEnabled->set(0.0);
    openRootItem(5);

    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Speech on/off, off", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));

    // The state is read live on every announcement, so flipping the control
    // and re-speaking the item reports the new state.
    pEnabled->set(1.0);
    respeakCurrentItem();
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Speech on/off, on", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));
}

TEST_F(ToggleStateTest, Fullscreen_SpeaksStatePushedFromMainWindow) {
    // No CO backs fullscreen; MixxxMainWindow pushes it via
    // setFullScreenState() (issue #57).
    m_pController->setFullScreenState(true);
    openRootItem(6);

    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Fullscreen, on", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));

    m_pController->setFullScreenState(false);
    respeakCurrentItem();
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Fullscreen, off", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));
}

TEST_F(ToggleStateTest, KeyboardShortcuts_SpeaksConfigBackedState) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Keyboard]"), QStringLiteral("Enabled")), 1);
    openRootItem(7);

    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Keyboard shortcuts, on", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));
}

// -- Keyboard-shortcuts-off warning (issue #57) ----------------------------

TEST_F(ToggleStateTest, DisablingKeyboardShortcuts_SpeaksWarningBeforeToggling) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Keyboard]"), QStringLiteral("Enabled")), 1);
    QStringList triggered;
    QObject::connect(m_pController.get(),
            &AccessMenuController::actionTriggered,
            m_pController.get(),
            [&triggered](const QString& id) { triggered.append(id); });

    openRootItem(7); // Keyboard shortcuts
    clearSpy();
    press(QStringLiteral("confirm"));

    ASSERT_EQ(1, triggered.size());
    EXPECT_QSTRING_EQ("toggleKeyboardShortcuts", triggered.at(0));
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_TRUE(m_pSpy->m_texts.at(0).contains("Keyboard shortcuts", Qt::CaseInsensitive));
    EXPECT_TRUE(m_pSpy->m_texts.at(0).contains("off", Qt::CaseInsensitive));
    // The warning is spoken before the toggle is emitted, so a stranded
    // keyboard-only DJ hears it before shortcuts actually die.
    EXPECT_EQ(1, m_pSpy->m_texts.size());
}

TEST_F(ToggleStateTest, EnablingKeyboardShortcuts_NoWarning) {
    // Currently off; confirming toggles it ON, which needs no warning.
    config()->setValue(
            ConfigKey(QStringLiteral("[Keyboard]"), QStringLiteral("Enabled")), 0);
    openRootItem(7); // Keyboard shortcuts
    clearSpy();
    press(QStringLiteral("confirm"));

    EXPECT_EQ(0, m_pSpy->m_texts.size());
}

// -- Value editor (issue #32) -------------------------------------------

// TTS rate is config-backed ([Accessibility],TtsRate), so these tests drive it
// through the injected UserSettings rather than a control object.
class ValueEditorTest : public AccessMenuControllerTest {
  protected:
    void setRate(double v) {
        config()->setValue(
                ConfigKey(QStringLiteral("[Accessibility]"),
                        QStringLiteral("TtsRate")),
                v);
    }

    double getRate() const {
        return config()->getValue<double>(
                ConfigKey(QStringLiteral("[Accessibility]"),
                        QStringLiteral("TtsRate")),
                0.0);
    }

    // Open the Values submenu and highlight the Value item at `itemIndex`
    // (0 = Back, 1 = Speech on/off, 2 = Speech rate, 3 = Ducking,
    // 4 = Beat click).
    void openValuesItem(int itemIndex) {
        press(QStringLiteral("open"));
        clearSpy();

        navigate(1.0); // Preferences
        navigate(1.0); // Values
        press(QStringLiteral("activate")); // descend into Values
        clearSpy();

        for (int i = 0; i < itemIndex; ++i) {
            navigate(1.0);
        }
        clearSpy();
    }
};

TEST_F(ValueEditorTest, EnterValueEditMode_SpeaksLabelAndStartValue) {
    setRate(3.0);
    openValuesItem(2); // Speech rate

    press(QStringLiteral("activate"));

    // Mode announcement and the current value are one utterance (issue #48,
    // case 3): two separate speak() calls here used to let the value
    // silently supersede the instructions before they could render.
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ(
            "Speech rate. Turn to change, confirm to set, back to cancel. 3",
            m_pSpy->m_texts.at(0));
}

TEST_F(ValueEditorTest, Navigate_ChangesValueAndSpeaksIt) {
    setRate(0.0);
    openValuesItem(2); // Speech rate
    press(QStringLiteral("activate"));
    clearSpy();

    navigate(1.0); // 1
    navigate(1.0); // 2
    navigate(-1.0); // 1

    ASSERT_GE(m_pSpy->m_texts.size(), 3);
    EXPECT_QSTRING_EQ("1", m_pSpy->m_texts.at(0));
    EXPECT_QSTRING_EQ("2", m_pSpy->m_texts.at(1));
    EXPECT_QSTRING_EQ("1", m_pSpy->m_texts.at(2));

    // The controller wrote each step to the config.
    EXPECT_EQ(1.0, getRate());
}

TEST_F(ValueEditorTest, Navigate_ClampsAtMinAndMax) {
    setRate(0.0);
    openValuesItem(2); // Speech rate, range -10..10 step 1
    press(QStringLiteral("activate"));
    clearSpy();

    // 20 ticks up would reach 20, but the range clamps at 10.
    for (int i = 0; i < 20; ++i) {
        navigate(1.0);
    }
    EXPECT_EQ(10.0, getRate());
    // The last spoken value is the clamped maximum.
    EXPECT_QSTRING_EQ("10", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));

    clearSpy();
    for (int i = 0; i < 25; ++i) {
        navigate(-1.0);
    }
    EXPECT_EQ(-10.0, getRate());
    EXPECT_QSTRING_EQ("-10", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));
}

TEST_F(ValueEditorTest, Confirm_CommitsAndExitsEditMode) {
    setRate(0.0);
    openValuesItem(2);
    press(QStringLiteral("activate"));
    clearSpy();

    navigate(1.0); // 1
    navigate(1.0); // 2
    clearSpy();
    press(QStringLiteral("confirm"));

    // The value stays at the committed value.
    EXPECT_EQ(2.0, getRate());

    // Exiting edit mode speaks the confirmation and the current menu item
    // (the same Value item with its new value) as one utterance (issue #48,
    // case 3).
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Set. Speech rate, 2", m_pSpy->m_texts.at(0));

    // The browse knob now scrolls the menu again, not the value.
    auto pDuck = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Tts]"), QStringLiteral("duckStrength")));
    pDuck->set(0.5);
    clearSpy();
    navigate(1.0); // next item: Ducking strength
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Ducking strength, 50 percent", m_pSpy->m_texts.at(0));
}

TEST_F(ValueEditorTest, Back_CancelsEditAndRestoresValue) {
    setRate(4.0);
    openValuesItem(2);
    press(QStringLiteral("activate"));
    clearSpy();

    navigate(1.0); // 5
    navigate(1.0); // 6
    clearSpy();
    press(QStringLiteral("back"));

    // The original value is restored.
    EXPECT_EQ(4.0, getRate());

    // Back exits edit mode without navigating the menu; the cancellation and
    // the re-highlighted item are one utterance (issue #48, case 3).
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Cancelled. Speech rate, 4", m_pSpy->m_texts.at(0));
}

TEST_F(ValueEditorTest, Boolean_StepsOnOff) {
    // Speech on/off is item 1 of the Values submenu, backed by [Tts],enabled.
    // It does not exist by default in a unit test, so create it; the value
    // editor writes the absolute state, which is what a Toggle button's
    // ControlObject::set does too.
    auto pEnabled = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Tts]"), QStringLiteral("enabled")));
    pEnabled->set(1.0);

    openValuesItem(1);
    press(QStringLiteral("activate"));
    clearSpy();

    navigate(1.0); // off
    EXPECT_EQ(0.0, pEnabled->get());
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("off", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));

    navigate(1.0); // on
    EXPECT_EQ(1.0, pEnabled->get());
    EXPECT_QSTRING_EQ("on", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));
}

TEST_F(ValueEditorTest, Percent_FormatsAsPercent) {
    // Without any Ducking/BeatClick controls present, step against a created
    // [Tts],duckStrength control (range 0..1 step 0.05 -> percent format).
    auto pDuck = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Tts]"), QStringLiteral("duckStrength")));
    pDuck->set(0.5);

    openValuesItem(3); // Ducking strength
    press(QStringLiteral("activate"));
    clearSpy();

    navigate(1.0); // +0.05 -> 0.55
    EXPECT_EQ(0.55, pDuck->get());
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("55 percent", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));

    clearSpy();
    navigate(-1.0); // back to 0.5
    EXPECT_EQ(0.5, pDuck->get());
    EXPECT_QSTRING_EQ("50 percent", m_pSpy->m_texts.at(m_pSpy->m_texts.size() - 1));
}

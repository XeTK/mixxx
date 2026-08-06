#include "util/accessmenucontroller.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QStringList>
#include <QTest>

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
                [this](const QString& text) { (*m_pSpy)(text); });
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
    ASSERT_GE(m_pSpy->m_texts.size(), 2);
    EXPECT_QSTRING_EQ("Main menu", m_pSpy->m_texts.at(0));
    // First item (Back) is spoken on open.
    EXPECT_QSTRING_EQ("Back", m_pSpy->m_texts.at(1));
}

TEST_F(AccessMenuControllerTest, Navigate_ScrollsAndSpeaksEachItem) {
    press(QStringLiteral("open"));
    clearSpy();

    navigate(1.0); // Preferences
    navigate(1.0); // Recording
    navigate(1.0); // Broadcasting

    ASSERT_GE(m_pSpy->m_texts.size(), 3);
    EXPECT_QSTRING_EQ("Preferences, submenu", m_pSpy->m_texts.at(0));
    EXPECT_QSTRING_EQ("Recording", m_pSpy->m_texts.at(1));
    EXPECT_QSTRING_EQ("Broadcasting", m_pSpy->m_texts.at(2));

    // Negative scroll goes back up.
    clearSpy();
    navigate(-1.0);
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
    EXPECT_QSTRING_EQ("Recording", m_pSpy->m_texts.at(0));
}

TEST_F(AccessMenuControllerTest, Navigate_WrapsAround) {
    press(QStringLiteral("open"));
    clearSpy();

    // The root menu has 11 items; 11 ticks wraps back to the first (Back).
    for (int i = 0; i < 11; ++i) {
        navigate(1.0);
    }
    ASSERT_GE(m_pSpy->m_texts.size(), 1);
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

    // Navigate to Quit (last item, index 10 of 11).
    for (int i = 0; i < 10; ++i) {
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

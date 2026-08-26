// Unit tests for DlgKeywheel's spoken notation-name helper (issue #63).
//
// DlgKeywheel itself needs a real QDialog/QSvgWidget and a UserSettingsPointer
// to construct, so — following the same pattern as
// bootdialog_speech_test.cpp — the text spoken when the key notation changes
// is factored into a small, pure, static helper (notationDisplayName())
// that is tested directly here instead of driving the whole dialog.

#include "dialog/dlgkeywheel.h"

#include <gtest/gtest.h>

#include "test/mixxxtest.h"

namespace {

class DlgKeywheelSpeechTest : public MixxxTest {
};

TEST_F(DlgKeywheelSpeechTest, NotationDisplayName_Traditional) {
    EXPECT_QSTRING_EQ(QStringLiteral("Traditional notation"),
            DlgKeywheel::notationDisplayName(KeyUtils::KeyNotation::Traditional));
}

TEST_F(DlgKeywheelSpeechTest, NotationDisplayName_Lancelot) {
    EXPECT_QSTRING_EQ(QStringLiteral("Lancelot notation"),
            DlgKeywheel::notationDisplayName(KeyUtils::KeyNotation::Lancelot));
}

TEST_F(DlgKeywheelSpeechTest, NotationDisplayName_OpenKey) {
    EXPECT_QSTRING_EQ(QStringLiteral("OpenKey notation"),
            DlgKeywheel::notationDisplayName(KeyUtils::KeyNotation::OpenKey));
}

TEST_F(DlgKeywheelSpeechTest, NotationDisplayName_Custom) {
    EXPECT_QSTRING_EQ(QStringLiteral("Custom notation"),
            DlgKeywheel::notationDisplayName(KeyUtils::KeyNotation::Custom));
}

TEST_F(DlgKeywheelSpeechTest, NotationDisplayName_InvalidIsEmpty) {
    EXPECT_TRUE(DlgKeywheel::notationDisplayName(KeyUtils::KeyNotation::Invalid).isEmpty());
}

} // namespace

// Unit tests for the spoken-text helpers used by the Confirm Exit dialogs in
// MixxxMainWindow::confirmExit() (issue #115).
//
// confirmExit() blocks on QMessageBox::question(), so like the boot-time
// dialogs (see bootdialog_speech_test.cpp) the spoken text is factored into
// small, pure, static helper functions (MixxxMainWindow::confirmExit*Speech())
// that are unit tested here without instantiating the full
// MixxxMainWindow/CoreServices machinery. confirmExit() calls these helpers
// and passes the result to Library::announceText(); the dialog behavior
// (which QMessageBox is shown, its buttons/default) is unchanged.
//
// Before this fix, none of the three Confirm Exit QMessageBoxes were wired
// into the fork's speech system at all -- a blind user quitting Mixxx with a
// deck or sampler playing (or with Preferences still open) got a silent
// modal dialog with no indication it had even appeared.

#include "mixxxmainwindow.h"

#include <gtest/gtest.h>

#include <QString>

#include "test/mixxxtest.h"

namespace {

class ExitDialogSpeechTest : public MixxxTest {
};

TEST_F(ExitDialogSpeechTest, ConfirmExitDeckPlayingSpeech_IsComplete) {
    const QString speech = MixxxMainWindow::confirmExitDeckPlayingSpeech();

    EXPECT_TRUE(speech.contains(QStringLiteral("A deck is currently playing")));
    EXPECT_TRUE(speech.contains(QStringLiteral("Exit Mixxx?")));
    // The dialog's default button is No; a stray Enter/Escape must never
    // exit Mixxx out from under a playing deck.
    EXPECT_TRUE(speech.contains(QStringLiteral("No is selected by default")));
}

TEST_F(ExitDialogSpeechTest, ConfirmExitSamplerPlayingSpeech_IsComplete) {
    const QString speech = MixxxMainWindow::confirmExitSamplerPlayingSpeech();

    EXPECT_TRUE(speech.contains(QStringLiteral("A sampler is currently playing")));
    EXPECT_TRUE(speech.contains(QStringLiteral("Exit Mixxx?")));
    EXPECT_TRUE(speech.contains(QStringLiteral("No is selected by default")));
}

TEST_F(ExitDialogSpeechTest, ConfirmExitPreferencesOpenSpeech_IsComplete) {
    const QString speech = MixxxMainWindow::confirmExitPreferencesOpenSpeech();

    EXPECT_TRUE(speech.contains(QStringLiteral("preferences window is still open")));
    EXPECT_TRUE(speech.contains(QStringLiteral("Discard any changes and exit Mixxx?")));
    EXPECT_TRUE(speech.contains(QStringLiteral("No is selected by default")));
}

TEST_F(ExitDialogSpeechTest, AllThreeConfirmExitSpeechesAreDistinct) {
    const QString deck = MixxxMainWindow::confirmExitDeckPlayingSpeech();
    const QString sampler = MixxxMainWindow::confirmExitSamplerPlayingSpeech();
    const QString prefs = MixxxMainWindow::confirmExitPreferencesOpenSpeech();

    EXPECT_NE(deck, sampler);
    EXPECT_NE(deck, prefs);
    EXPECT_NE(sampler, prefs);
}

} // namespace

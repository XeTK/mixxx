// Unit tests for the spoken-text helpers used by the boot-time dialogs in
// MixxxMainWindow.
//
// The dialog methods themselves (alwaysHideMenuBarDlg(), soundDeviceBusyDlg(),
// etc.) block on msgBox.exec() and depend on the full MixxxMainWindow /
// CoreServices machinery, so they are not unit tested directly. Instead, the
// text that is spoken via Library::announceText() is factored into small, pure,
// static helper functions (MixxxMainWindow::*Speech()) that are tested here.
// The dialog methods call these helpers and pass the result to announceText();
// the dialog behavior is unchanged.

#include "mixxxmainwindow.h"

#include <gtest/gtest.h>

#include <QString>

#include "test/mixxxtest.h"

namespace {

class BootDialogSpeechTest : public MixxxTest {
};

TEST_F(BootDialogSpeechTest, MenuBarHideSpeech_ContainsButtonLabels) {
    const QString speech = MixxxMainWindow::menuBarHideSpeech(
            QStringLiteral("Hide"), QStringLiteral("Always show"));

    EXPECT_QSTRING_EQ(
            QStringLiteral("Allow Mixxx to hide the menu bar? The menu bar can be "
                           "toggled with a single press of the Alt key. Press Hide "
                           "to agree, or Always show to disable that and always "
                           "show the menu bar."),
            speech);
}

TEST_F(BootDialogSpeechTest, SoundDeviceBusySpeech_ContainsDeviceName) {
    const QString speech = MixxxMainWindow::soundDeviceBusySpeech(
            QStringLiteral("Built-in Output"));

    EXPECT_QSTRING_EQ(
            QStringLiteral("Mixxx was unable to open all the configured sound "
                           "devices. Built-in Output is used by another "
                           "application or not plugged in. Press Retry to try "
                           "again, Reconfigure to change the sound hardware "
                           "settings, get Help from the Mixxx wiki, or Exit to "
                           "quit Mixxx."),
            speech);
}

TEST_F(BootDialogSpeechTest, SoundDeviceErrorSpeech_ContainsErrorMessage) {
    const QString speech = MixxxMainWindow::soundDeviceErrorSpeech(
            QStringLiteral("Device not found"));

    EXPECT_QSTRING_EQ(
            QStringLiteral("Mixxx was unable to open all the configured sound "
                           "devices. Device not found Press Retry to try again, "
                           "Reconfigure to change the sound hardware settings, "
                           "get Help from the Mixxx wiki, or Exit to quit Mixxx."),
            speech);
}

TEST_F(BootDialogSpeechTest, NoOutputSpeech_IsComplete) {
    const QString speech = MixxxMainWindow::noOutputSpeech();

    EXPECT_QSTRING_EQ(
            QStringLiteral("Mixxx was configured without any output sound "
                           "devices. Audio processing will be disabled without a "
                           "configured output device. Press Continue to continue "
                           "without any outputs, Reconfigure to change the sound "
                           "hardware settings, or Exit to quit Mixxx."),
            speech);
}

TEST_F(BootDialogSpeechTest, NoVinylControlInputSpeech_IsComplete) {
    const QString speech = MixxxMainWindow::noVinylControlInputSpeech();

    EXPECT_QSTRING_EQ(
            QStringLiteral("There is no input device selected for this vinyl "
                           "control. Please select an input device in the sound "
                           "hardware preferences first."),
            speech);
}

TEST_F(BootDialogSpeechTest, NoPassthroughInputSpeech_IsComplete) {
    const QString speech = MixxxMainWindow::noPassthroughInputSpeech();

    EXPECT_QSTRING_EQ(
            QStringLiteral("There is no input device selected for this "
                           "passthrough control. Please select an input device in "
                           "the sound hardware preferences first."),
            speech);
}

TEST_F(BootDialogSpeechTest, NoMicrophoneInputSpeech_IsComplete) {
    const QString speech = MixxxMainWindow::noMicrophoneInputSpeech();

    EXPECT_QSTRING_EQ(
            QStringLiteral("There is no input device selected for this "
                           "microphone. Do you want to select an input device?"),
            speech);
}

TEST_F(BootDialogSpeechTest, NoAuxiliaryInputSpeech_IsComplete) {
    const QString speech = MixxxMainWindow::noAuxiliaryInputSpeech();

    EXPECT_QSTRING_EQ(
            QStringLiteral("There is no input device selected for this auxiliary. "
                           "Do you want to select an input device?"),
            speech);
}

TEST_F(BootDialogSpeechTest, DirectRenderingSpeech_IsComplete) {
    const QString speech = MixxxMainWindow::directRenderingSpeech();

    EXPECT_QSTRING_EQ(
            QStringLiteral("Direct rendering is not enabled on your machine. This "
                           "means that the waveform displays will be very slow and "
                           "may tax your CPU heavily. Either update your "
                           "configuration to enable direct rendering, or disable "
                           "the waveform displays in the Mixxx preferences by "
                           "selecting Empty as the waveform display in the "
                           "Interface section."),
            speech);
}

TEST_F(BootDialogSpeechTest, LibraryScanSummarySpeech_StripsHtml) {
    const QString speech = MixxxMainWindow::libraryScanSummarySpeech(
            QStringLiteral("Scan took 2s<br><br><b>100 tracks in total</b>"));

    EXPECT_QSTRING_EQ(
            QStringLiteral("Library scan finished. Scan took 2s100 tracks in "
                           "total"),
            speech);
}

TEST_F(BootDialogSpeechTest, LibraryScanSummarySpeech_EmptyHtml) {
    const QString speech = MixxxMainWindow::libraryScanSummarySpeech(
            QStringLiteral(""));

    EXPECT_QSTRING_EQ(QStringLiteral("Library scan finished. "), speech);
}

} // namespace

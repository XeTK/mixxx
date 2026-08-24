// Accessibility (issue #52): ErrorDialogHandler is the single choke point
// every error/warning/info dialog request funnels through (broadcast
// connection failures, controller script errors, recording disk-full, etc.),
// and several of those dialogs are non-modal and may not even take focus. It
// emits errorDialogAnnouncement() right as a dialog is about to be shown so a
// blind, TTS-only user hears the same information a sighted user reads,
// without every call site needing its own speech call. These tests verify
// that signal in isolation, the same way bootdialog_speech_test.cpp verifies
// the boot-time dialogs' spoken text without needing the full
// MixxxMainWindow/CoreServices machinery. The CoreServices wiring that turns
// this signal into actual speech (connecting it to Library::announceText())
// is not exercised here.

#include "errordialoghandler.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>
#include <QSignalSpy>
#include <QString>
#include <QThread>

#include "test/mixxxtest.h"

namespace {

// Unique dedup key per call so a dialog from one test (or a previous test
// earlier in this same process) never suppresses a dialog in another test --
// ErrorDialogHandler::requestErrorDialog() silently drops a request whose key
// is already displayed.
QString uniqueKey(const char* label) {
    static int counter = 0;
    return QStringLiteral("ErrorDialogHandlerTest::%1::%2").arg(label).arg(++counter);
}

class ErrorDialogHandlerTest : public MixxxTest {
  protected:
    void SetUp() override {
        // errordialoghandler.cpp only acts when called from the thread named
        // "Main" (see the objectName() check in errorDialog()); production
        // code sets this in main.cpp, but the test binary's main() (see
        // src/test/main.cpp) does not, so tests that exercise this handler
        // must set it themselves (see also
        // controllerscriptenginelegacy_test.cpp).
        QThread::currentThread()->setObjectName("Main");

        // src/test/main.cpp disables error dialogs globally so an unrelated
        // failure elsewhere never pops up a blocking QMessageBox mid-test-run.
        // Re-enable for the duration of this test.
        ErrorDialogHandler::setEnabled(true);
    }

    void TearDown() override {
        // Non-modal dialogs left open by requestErrorDialog() in this test
        // are WA_DeleteOnClose; close them so their dedup key is released via
        // ErrorDialogHandler::boxClosed() and they don't linger into the next
        // test or the next test binary run within this process.
        for (QWidget* pWidget : QApplication::topLevelWidgets()) {
            if (auto* pBox = qobject_cast<QMessageBox*>(pWidget)) {
                pBox->close();
            }
        }
        QCoreApplication::processEvents();

        ErrorDialogHandler::setEnabled(false);
    }
};

} // namespace

TEST_F(ErrorDialogHandlerTest, ShowingDialogAnnouncesTitleAndText) {
    ErrorDialogProperties* props = ErrorDialogHandler::instance()->newDialogProperties();
    props->setType(DLG_CRITICAL);
    props->setTitle(QStringLiteral("Test Failure"));
    props->setText(QStringLiteral("Something important broke."));
    props->setKey(uniqueKey("ShowingDialogAnnouncesTitleAndText"));
    // Non-modal so requestErrorDialog() below returns immediately instead of
    // blocking on QMessageBox::exec() waiting for a user click that will
    // never come.
    props->setModal(false);

    QSignalSpy spy(ErrorDialogHandler::instance(),
            &ErrorDialogHandler::errorDialogAnnouncement);

    ASSERT_TRUE(ErrorDialogHandler::instance()->requestErrorDialog(props));

    ASSERT_EQ(spy.count(), 1);
    const QString spoken = spy.takeFirst().at(0).toString();
    EXPECT_TRUE(spoken.contains(QStringLiteral("Test Failure")));
    EXPECT_TRUE(spoken.contains(QStringLiteral("Something important broke.")));
}

TEST_F(ErrorDialogHandlerTest, AnnouncementStripsHtmlMarkup) {
    // Mirrors how ShoutConnection::errorDialog() builds its message: HTML
    // formatting embedded directly in the primary text.
    ErrorDialogProperties* props = ErrorDialogHandler::instance()->newDialogProperties();
    props->setType(DLG_WARNING);
    props->setTitle(QStringLiteral("Connection error"));
    props->setText(QStringLiteral(
            "<b>Error with connection 'Foo':</b><br>Timed out"));
    props->setKey(uniqueKey("AnnouncementStripsHtmlMarkup"));
    props->setModal(false);

    QSignalSpy spy(ErrorDialogHandler::instance(),
            &ErrorDialogHandler::errorDialogAnnouncement);

    ASSERT_TRUE(ErrorDialogHandler::instance()->requestErrorDialog(props));

    ASSERT_EQ(spy.count(), 1);
    const QString spoken = spy.takeFirst().at(0).toString();
    EXPECT_FALSE(spoken.contains(QChar('<')));
    EXPECT_TRUE(spoken.contains(QStringLiteral("Error with connection 'Foo':")));
    EXPECT_TRUE(spoken.contains(QStringLiteral("Timed out")));
}

TEST_F(ErrorDialogHandlerTest, AnnouncementTruncatesVeryLongMessages) {
    const QString longText = QString("x").repeated(1000);

    ErrorDialogProperties* props = ErrorDialogHandler::instance()->newDialogProperties();
    props->setType(DLG_WARNING);
    props->setTitle(QStringLiteral("Long error"));
    props->setText(longText);
    props->setKey(uniqueKey("AnnouncementTruncatesVeryLongMessages"));
    props->setModal(false);

    QSignalSpy spy(ErrorDialogHandler::instance(),
            &ErrorDialogHandler::errorDialogAnnouncement);

    ASSERT_TRUE(ErrorDialogHandler::instance()->requestErrorDialog(props));

    ASSERT_EQ(spy.count(), 1);
    const QString spoken = spy.takeFirst().at(0).toString();
    EXPECT_LT(spoken.length(), longText.length());
}

TEST_F(ErrorDialogHandlerTest, DisabledHandlerNeitherShowsNorAnnounces) {
    // setEnabled(false) is the global test-suite default (see SetUp/TearDown
    // above and src/test/main.cpp); this test asserts that behavior
    // explicitly rather than relying on it implicitly.
    ErrorDialogHandler::setEnabled(false);

    ErrorDialogProperties* props = ErrorDialogHandler::instance()->newDialogProperties();
    props->setType(DLG_WARNING);
    props->setTitle(QStringLiteral("Should not appear"));
    props->setText(QStringLiteral("Should not be spoken"));
    props->setKey(uniqueKey("DisabledHandlerNeitherShowsNorAnnounces"));
    props->setModal(false);

    QSignalSpy spy(ErrorDialogHandler::instance(),
            &ErrorDialogHandler::errorDialogAnnouncement);

    EXPECT_FALSE(ErrorDialogHandler::instance()->requestErrorDialog(props));
    EXPECT_EQ(spy.count(), 0);
}

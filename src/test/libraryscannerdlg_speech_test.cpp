// Unit tests for LibraryScannerDlg's spoken show announcement (issue #63).
//
// The dialog appears silently ~2s into a library scan and steals focus with
// no announcement. setAnnounceCallback() injects a speak function (the same
// pattern as AccessMenuController; see accessmenucontroller_test.cpp) so this
// can be tested without needing the real Library/AnnouncementManager.

#include "library/scanner/libraryscannerdlg.h"

#include <gtest/gtest.h>

#include <QStringList>

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

class LibraryScannerDlgSpeechTest : public MixxxTest {
  protected:
    void SetUp() override {
        m_pSpy = std::make_unique<SpeakSpy>();
        m_pDlg = std::make_unique<LibraryScannerDlg>();
        // The spy must outlive the dialog, which holds a copy of the
        // std::function pointing at it.
        m_pDlg->setAnnounceCallback([this](const QString& text) { (*m_pSpy)(text); });
    }

    std::unique_ptr<SpeakSpy> m_pSpy;
    std::unique_ptr<LibraryScannerDlg> m_pDlg;
};

TEST_F(LibraryScannerDlgSpeechTest, AnnouncesOnFirstShow) {
    m_pDlg->show();

    ASSERT_EQ(1, m_pSpy->m_texts.size());
    EXPECT_QSTRING_EQ(QStringLiteral("Scanning library"), m_pSpy->m_texts.at(0));
}

TEST_F(LibraryScannerDlgSpeechTest, DoesNotRepeatOnRepeatedShowsWithinOneScan) {
    m_pDlg->show();
    m_pDlg->hide();
    // The dialog is re-shown as slotUpdate()/slotUpdateCover() keep firing
    // past the 2s threshold; per-file churn must not spam more announcements.
    m_pDlg->show();
    m_pDlg->hide();
    m_pDlg->show();

    EXPECT_EQ(1, m_pSpy->m_texts.size());
}

TEST_F(LibraryScannerDlgSpeechTest, AnnouncesAgainOnNextScan) {
    m_pDlg->show();
    m_pDlg->hide();

    // A new scan starting resets the per-scan announcement guard.
    m_pDlg->slotScanStarted();
    m_pDlg->show();

    ASSERT_EQ(2, m_pSpy->m_texts.size());
    EXPECT_QSTRING_EQ(QStringLiteral("Scanning library"), m_pSpy->m_texts.at(1));
}

} // namespace

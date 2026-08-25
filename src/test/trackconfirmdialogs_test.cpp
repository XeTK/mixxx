// Tests for the accessibility confirmation dialogs added for issue #53:
// Purge previously ran with zero confirmation, and the existing
// hide/remove/delete-from-disk confirmations were plain, TTS-silent
// QMessageBoxes. See src/widget/trackconfirmdialogs.h.
//
// The *Announcement() functions are pure text builders, tested directly
// (mirroring AnnouncementManager's own static-helper tests). confirmPurge()
// additionally drives a real modal QMessageBox via the offscreen QPA
// platform, using the standard Qt test technique of interacting with the
// dialog from a QTimer::singleShot() fired while QMessageBox::exec() is
// blocking.

#include "widget/trackconfirmdialogs.h"

#include <gtest/gtest.h>

#include <QAbstractButton>
#include <QApplication>
#include <QMessageBox>
// QMessageBox::defaultButton() returns QPushButton*, which qmessagebox.h only
// forward-declares. The complete type is needed to convert it to its
// QAbstractButton base.
#include <QPushButton>
#include <QTimer>

#include <functional>

#include "test/mixxxtest.h"

class TrackConfirmDialogsTest : public MixxxTest {};

// ---------------------------------------------------------------------------
// purgeAnnouncement
// ---------------------------------------------------------------------------

TEST_F(TrackConfirmDialogsTest, PurgeAnnouncement_MentionsCountAndNoDefault) {
    const QString text = mixxx::trackconfirm::purgeAnnouncement(3);
    EXPECT_TRUE(text.contains(QStringLiteral("3")));
    EXPECT_TRUE(text.contains(QStringLiteral("No is selected by default")));
    // The distinction that matters most for a TTS-only user: purge doesn't
    // touch the file on disk.
    EXPECT_TRUE(text.contains(QStringLiteral("does not delete the file")));
}

TEST_F(TrackConfirmDialogsTest, PurgeAnnouncement_SingularVsPlural) {
    const QString singular = mixxx::trackconfirm::purgeAnnouncement(1);
    const QString plural = mixxx::trackconfirm::purgeAnnouncement(2);
    EXPECT_NE(singular, plural);
}

// ---------------------------------------------------------------------------
// hideOrRemoveAnnouncement
// ---------------------------------------------------------------------------

TEST_F(TrackConfirmDialogsTest, HideAnnouncement_MentionsHideAndNoDefault) {
    const QString text = mixxx::trackconfirm::hideOrRemoveAnnouncement(
            TrackModel::Capability::Hide, 5);
    EXPECT_TRUE(text.contains(QStringLiteral("hide")));
    EXPECT_TRUE(text.contains(QStringLiteral("5")));
    EXPECT_TRUE(text.contains(QStringLiteral("No is selected by default")));
}

TEST_F(TrackConfirmDialogsTest, RemoveFromAutoDjAnnouncement_MentionsAutoDj) {
    const QString text = mixxx::trackconfirm::hideOrRemoveAnnouncement(
            TrackModel::Capability::Remove, 1);
    EXPECT_TRUE(text.contains(QStringLiteral("AutoDJ")));
    EXPECT_TRUE(text.contains(QStringLiteral("No is selected by default")));
}

TEST_F(TrackConfirmDialogsTest, RemoveFromCrateAnnouncement_MentionsCrate) {
    const QString text = mixxx::trackconfirm::hideOrRemoveAnnouncement(
            TrackModel::Capability::RemoveCrate, 1);
    EXPECT_TRUE(text.contains(QStringLiteral("crate")));
}

TEST_F(TrackConfirmDialogsTest, RemoveFromPlaylistAnnouncement_MentionsPlaylist) {
    const QString text = mixxx::trackconfirm::hideOrRemoveAnnouncement(
            TrackModel::Capability::RemovePlaylist, 1);
    EXPECT_TRUE(text.contains(QStringLiteral("playlist")));
}

// ---------------------------------------------------------------------------
// deleteFromDiskAnnouncement
// ---------------------------------------------------------------------------

TEST_F(TrackConfirmDialogsTest, DeleteFromDiskAnnouncement_MentionsCancelDefault) {
    const QString text = mixxx::trackconfirm::deleteFromDiskAnnouncement(2);
    EXPECT_TRUE(text.contains(QStringLiteral("2")));
    EXPECT_TRUE(text.contains(QStringLiteral("Cancel is selected by default")));
}

// ---------------------------------------------------------------------------
// confirmPurge: drives a real modal QMessageBox.
// ---------------------------------------------------------------------------

namespace {

// Waits for the confirm dialog to become the active modal widget, then
// invokes `interact` with it. Scheduled via QTimer::singleShot(0, ...) so it
// runs once QMessageBox::exec() has started its nested event loop.
void interactWithNextModal(const std::function<void(QMessageBox*)>& interact) {
    QTimer::singleShot(0, [interact]() {
        auto* pBox = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        ASSERT_NE(pBox, nullptr);
        interact(pBox);
    });
}

} // namespace

TEST_F(TrackConfirmDialogsTest, ConfirmPurge_DefaultButtonIsNo) {
    interactWithNextModal([](QMessageBox* pBox) {
        // A bare Enter/Escape must never purge: click whichever button is
        // the dialog's default.
        QPushButton* pDefault = pBox->defaultButton();
        ASSERT_NE(pDefault, nullptr);
        EXPECT_EQ(pBox->buttonRole(pDefault), QMessageBox::NoRole);
        pDefault->click();
    });
    EXPECT_FALSE(mixxx::trackconfirm::confirmPurge(nullptr, nullptr, 1));
}

TEST_F(TrackConfirmDialogsTest, ConfirmPurge_YesConfirms) {
    interactWithNextModal([](QMessageBox* pBox) {
        QAbstractButton* pYes = pBox->button(QMessageBox::Yes);
        ASSERT_NE(pYes, nullptr);
        pYes->click();
    });
    EXPECT_TRUE(mixxx::trackconfirm::confirmPurge(nullptr, nullptr, 1));
}

// Regression test for issue #53: TrackCollection::hideTracks() shows a
// QMessageBox::question() when the tracks being hidden are still in one or
// more playlists, warning that hiding will remove them from those
// playlists. That dialog used to be created with no explicit default
// button, which meant Qt picked one on its own -- on the Ok|Cancel button
// set used here that resolved to Ok, so a stray Enter press proceeded with
// the destructive action. The dialog now explicitly defaults to Cancel.
//
// This drives the real modal QMessageBox via the offscreen QPA platform,
// interacting with it from a QTimer::singleShot() fired while
// QMessageBox::question() is blocking -- the same technique used in
// trackconfirmdialogs_test.cpp.

#include "library/trackcollection.h"

#include <gtest/gtest.h>

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QTimer>

#include <functional>

#include "library/dao/playlistdao.h"
#include "test/librarytest.h"
#include "track/track.h"

namespace {

// Waits for the "Hiding tracks" confirmation to become the active modal
// widget, then invokes `interact` with it.
void interactWithNextModal(const std::function<void(QMessageBox*)>& interact) {
    QTimer::singleShot(0, [interact]() {
        auto* pBox = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        ASSERT_NE(pBox, nullptr);
        interact(pBox);
    });
}

} // namespace

class TrackCollectionHideTracksTest : public LibraryTest {
  protected:
    // Adds a temporary track to the collection and to a fresh playlist, so
    // hideTracks() on it is guaranteed to hit the playlist-membership
    // warning dialog.
    TrackId addTrackInPlaylist() {
        const mixxx::FileInfo fileInfo(
                QDir(QDir::tempPath()), QStringLiteral("hide-tracks-test.mp3"));
        TrackPointer pTrack = Track::newTemporary(mixxx::FileAccess(fileInfo));
        pTrack->setDuration(135);
        const TrackId trackId = internalCollection()->addTrack(pTrack, false);
        EXPECT_TRUE(trackId.isValid());

        PlaylistDAO& playlistDao = internalCollection()->getPlaylistDAO();
        const int playlistId = playlistDao.createPlaylist(
                QStringLiteral("hide-tracks-test-playlist"));
        EXPECT_NE(playlistId, -1);
        EXPECT_TRUE(playlistDao.insertTrackIntoPlaylist(trackId, playlistId, 0));

        return trackId;
    }
};

TEST_F(TrackCollectionHideTracksTest, DefaultButtonIsCancel) {
    const TrackId trackId = addTrackInPlaylist();

    interactWithNextModal([](QMessageBox* pBox) {
        // A bare Enter/Escape must never proceed with the destructive
        // action: click whichever button is the dialog's default.
        QAbstractButton* pDefault = pBox->defaultButton();
        ASSERT_NE(pDefault, nullptr);
        EXPECT_EQ(pBox->buttonRole(pDefault), QMessageBox::RejectRole);
        pDefault->click();
    });

    EXPECT_FALSE(internalCollection()->hideTracks({trackId}));
}

TEST_F(TrackCollectionHideTracksTest, ExplicitOkStillProceeds) {
    const TrackId trackId = addTrackInPlaylist();

    interactWithNextModal([](QMessageBox* pBox) {
        QAbstractButton* pOk = pBox->button(QMessageBox::Ok);
        ASSERT_NE(pOk, nullptr);
        pOk->click();
    });

    EXPECT_TRUE(internalCollection()->hideTracks({trackId}));
}

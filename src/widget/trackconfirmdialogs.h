#pragma once

#include <QString>

#include "library/trackmodel.h"

class QWidget;
class Library;

// Accessibility (issue #53): confirmation dialogs for destructive
// track-list actions (purge / hide / remove / delete-from-disk) used to be
// either missing entirely (Purge) or plain QMessageBoxes that only a screen
// reader would announce. For a TTS-only user with an unlabeled context menu,
// that's not discoverable in the moment.
//
// These helpers echo back what's about to happen through
// Library::announceText(), matching the pattern already used by the
// playlist/crate delete confirmations (see
// BasePlaylistFeature::slotDeletePlaylist / CrateFeature::slotDeleteCrate):
// state the action, name the count, and spell out which button is the
// default so a stray Enter/Escape is never destructive.
//
// The *Announcement() functions are pure text builders so they can be unit
// tested without instantiating any widget or Library. The confirm*()
// functions are the thin (untested) glue that speaks the announcement and
// shows the actual dialog.
namespace mixxx {
namespace trackconfirm {

// Spoken confirmation for permanently removing trackCount track(s) from the
// library via Purge. Purge does not delete the file from disk -- only the
// library entry -- but that distinction isn't discoverable from an
// unnarrated context menu, so the confirmation calls it out explicitly.
QString purgeAnnouncement(int trackCount);

// Speaks purgeAnnouncement() through pLibrary (if non-null) and shows the
// Purge confirmation dialog (Yes/No, No is the default so a stray Enter
// never purges). Returns true if the user confirmed.
bool confirmPurge(QWidget* pParent, Library* pLibrary, int trackCount);

// Spoken confirmation mirroring the existing hide/remove QMessageBox (see
// WTrackTableView::hideOrRemoveSelectedTracks). Capability must be one of
// Hide, Remove, RemoveCrate, or RemovePlaylist.
QString hideOrRemoveAnnouncement(TrackModel::Capability cap, int trackCount);

// Spoken confirmation for the delete/move-to-trash-from-disk dialog (see
// WTrackMenu::slotRemoveFromDisk).
QString deleteFromDiskAnnouncement(int trackCount);

} // namespace trackconfirm
} // namespace mixxx

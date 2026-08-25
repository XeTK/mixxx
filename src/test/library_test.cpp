#include "library/library.h"

#include <gtest/gtest.h>

#include <QAction>

#include "test/mixxxtest.h"
#include "util/qt.h"

// Library::hoverAnnouncementTextForAction() is the text-selection logic
// behind Library::announceMenuHover(), which wires WTrackMenu's submenus and
// the library sidebar's playlist/crate context menu up to the fork's
// built-in TTS the same way WTrackTableView::showQuickAddPickerMenu() already
// does for the quick-add-to-crate/playlist picker (see
// AnnouncementManagerTest's QuickPicker_* tests for coverage of the spoken
// text once it reaches AnnouncementManager).
//
// announceMenuHover() itself isn't exercised end-to-end here: constructing a
// real Library requires a full TrackCollectionManager/PlayerManager/
// RecordingManager stack (see PlayerManagerTest), which is disproportionate
// for what is, past this text-selection step, just a connect() to an
// already-tested announceQuickPickerItem() call.
class LibraryHoverAnnouncementTest : public MixxxTest {};

TEST_F(LibraryHoverAnnouncementTest, NullActionIsSilent) {
    EXPECT_TRUE(Library::hoverAnnouncementTextForAction(nullptr).isEmpty());
}

TEST_F(LibraryHoverAnnouncementTest, PlainTextActionUsesText) {
    QAction action(QStringLiteral("Rename"), nullptr);
    EXPECT_QSTRING_EQ("Rename",
            Library::hoverAnnouncementTextForAction(&action));
}

TEST_F(LibraryHoverAnnouncementTest, EmptyActionIsSilent) {
    QAction action(nullptr);
    EXPECT_TRUE(Library::hoverAnnouncementTextForAction(&action).isEmpty());
}

TEST_F(LibraryHoverAnnouncementTest, DataPreferredOverEscapedText) {
    // WTrackMenu::slotPopulatePlaylistMenu / slotPopulateCrateMenu escape
    // "&" as "&&" in the action's display text so it isn't swallowed as a
    // QAction mnemonic, and store the raw name in data() for cases like
    // this. A crate named "R&B" must be announced as "R&B", not "R&&B".
    QAction action(nullptr);
    action.setText(mixxx::escapeTextPropertyWithoutShortcuts(QStringLiteral("R&B")));
    action.setData(QStringLiteral("R&B"));
    EXPECT_QSTRING_EQ("R&B", Library::hoverAnnouncementTextForAction(&action));
}

TEST_F(LibraryHoverAnnouncementTest, ValidButEmptyDataWinsOverText) {
    // data() is preferred whenever it's valid, even if that means an empty
    // string wins over a non-empty text(). None of this fork's call sites
    // set an empty data() deliberately (they either leave it unset or set it
    // to the real item name), so this documents the actual precedence rule
    // rather than prescribing it as desirable.
    QAction action(QStringLiteral("Lock"), nullptr);
    action.setData(QString());
    EXPECT_TRUE(Library::hoverAnnouncementTextForAction(&action).isEmpty());
}

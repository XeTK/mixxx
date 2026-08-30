// Unit tests for the issue #127 fix: an assistive-technology "press" on a row
// in the Preferences category tree selects the row but never switches the
// displayed page.
//
// WHY THIS TEST EXISTS / WHAT IT CAN AND CANNOT PROVE
// ----------------------------------------------------
// DlgPreferences itself needs live SoundManager/ControllerManager/
// VinylControlManager/EffectsManager/Library instances to construct (see its
// constructor in dlgpreferences.cpp) -- the same pattern that makes
// CoreServicesTest.DISABLED_TestInitialization unreliable enough on CI that
// it is permanently disabled (src/test/coreservicestest.cpp). Building a full
// DlgPreferences here would inherit that same flakiness risk for comparatively
// little benefit, so the fix (DlgPreferences::syncCurrentItemToSelection(),
// dlgpreferences.h/.cpp) is exposed as a static helper that operates on any
// QTreeWidget, following the same "extract a testable static helper" pattern
// used by dlgkeywheel_speech_test.cpp and bootdialog_speech_test.cpp.
//
// This test exercises that helper directly against a real QTreeWidget wired
// up the same way DlgPreferences wires contentsTreeWidget (currentItemChanged
// -> a page-switch counter), so it proves:
//   1. A selection-only change (QItemSelectionModel::select() without
//      setCurrentIndex(), which is what macOS VoiceOver's "press" produces
//      via QAccessibleTableCell's toggle action -- see the root-cause comment
//      on issue #31 and issue #127) now also moves the tree's *current item*
//      and therefore fires currentItemChanged, i.e. the pretend "page switch"
//      handler runs.
//   2. The ordinary mouse-click/keyboard-arrow path, where selection and
//      current item already change together, is not affected: the helper is
//      a no-op when they already agree, so the page-switch handler still
//      fires exactly once per change, not twice.
//
// What this test CANNOT verify: that VoiceOver on a real macOS system, or any
// other screen reader, actually drives Qt's accessibility bridge along the
// selection-only path described above, or that the real DlgPreferences page
// widget is shown. That would require a live GUI/VoiceOver run (or an
// accessibility-tree-driven end-to-end test, e.g. under tools/e2e/), which is
// outside the scope of what can be verified in this unit test binary.

#include "preferences/dialog/dlgpreferences.h"

#include <gtest/gtest.h>

#include <QSignalSpy>
#include <QTest>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "test/mixxxtest.h"

namespace {

class DlgPreferencesTreeSyncTest : public MixxxTest {
  protected:
    void SetUp() override {
        m_pTree = std::make_unique<QTreeWidget>();
        m_pItemA = new QTreeWidgetItem(m_pTree.get(), QTreeWidgetItem::Type);
        m_pItemA->setText(0, QStringLiteral("Sound Hardware"));
        m_pItemB = new QTreeWidgetItem(m_pTree.get(), QTreeWidgetItem::Type);
        m_pItemB->setText(0, QStringLiteral("Library"));
        // Mirrors DlgPreferences::addPageWidget(): only selectable, like the
        // real category tree items.
        m_pItemA->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        m_pItemB->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        m_pTree->setCurrentItem(m_pItemA);
    }

    std::unique_ptr<QTreeWidget> m_pTree;
    QTreeWidgetItem* m_pItemA;
    QTreeWidgetItem* m_pItemB;
};

// Reproduces the bug: an AX/VoiceOver "press" changes selection via the
// selection model directly, without going through setCurrentIndex(). Before
// the fix, DlgPreferences never called setCurrentItem() in response, so
// currentItemChanged (and therefore changePage()) never fired.
TEST_F(DlgPreferencesTreeSyncTest, SelectionOnlyChange_IsGuardedWithoutFix) {
    QSignalSpy currentItemChangedSpy(m_pTree.get(), &QTreeWidget::currentItemChanged);

    // Simulate the AX-driven selection-only change: select item B in the
    // selection model without moving the current index.
    m_pTree->selectionModel()->select(
            m_pTree->indexFromItem(m_pItemB),
            QItemSelectionModel::ClearAndSelect);

    ASSERT_EQ(m_pTree->selectedItems().size(), 1);
    EXPECT_EQ(m_pTree->selectedItems().first(), m_pItemB);
    // The bug: current item did NOT move, so a currentItemChanged-based
    // page-switch handler never runs.
    EXPECT_EQ(m_pTree->currentItem(), m_pItemA);
    EXPECT_EQ(currentItemChangedSpy.count(), 0);
}

// The fix: after the same selection-only change, calling
// syncCurrentItemToSelection() (which is what slotTreeItemSelectionChanged(),
// wired to itemSelectionChanged, now does) pulls the current item into sync
// and fires currentItemChanged exactly once.
TEST_F(DlgPreferencesTreeSyncTest, SelectionOnlyChange_FixSyncsCurrentItem) {
    QSignalSpy currentItemChangedSpy(m_pTree.get(), &QTreeWidget::currentItemChanged);

    m_pTree->selectionModel()->select(
            m_pTree->indexFromItem(m_pItemB),
            QItemSelectionModel::ClearAndSelect);
    ASSERT_EQ(currentItemChangedSpy.count(), 0);

    DlgPreferences::syncCurrentItemToSelection(m_pTree.get());

    EXPECT_EQ(m_pTree->currentItem(), m_pItemB);
    EXPECT_EQ(currentItemChangedSpy.count(), 1);
}

// Regression guard for the ordinary mouse-click/keyboard-arrow path: when
// setCurrentItem() is used directly (as it is by real clicks and arrow-key
// navigation), selection and current item change together, so the fix must
// be a no-op and must not cause a second currentItemChanged / page switch.
TEST_F(DlgPreferencesTreeSyncTest, NormalCurrentItemChange_NoDoubleFire) {
    QSignalSpy currentItemChangedSpy(m_pTree.get(), &QTreeWidget::currentItemChanged);

    m_pTree->setCurrentItem(m_pItemB);
    ASSERT_EQ(currentItemChangedSpy.count(), 1);

    // itemSelectionChanged would also have fired here in the real dialog;
    // simulate its handler running afterwards.
    DlgPreferences::syncCurrentItemToSelection(m_pTree.get());

    EXPECT_EQ(m_pTree->currentItem(), m_pItemB);
    // Still exactly one currentItemChanged: no double page switch.
    EXPECT_EQ(currentItemChangedSpy.count(), 1);
}

// Defensive: an empty selection (e.g. transiently while the tree is being
// rebuilt) must not crash or clear the current item.
TEST_F(DlgPreferencesTreeSyncTest, EmptySelection_IsNoOp) {
    m_pTree->clearSelection();
    ASSERT_TRUE(m_pTree->selectedItems().isEmpty());

    DlgPreferences::syncCurrentItemToSelection(m_pTree.get());

    EXPECT_EQ(m_pTree->currentItem(), m_pItemA);
}

// Characterization test, not a fix: documents the reported gap that
// contentsTreeWidget's category items get no keyboard path out of the tree
// -- Right arrow on a leaf category item (no children to expand into) is a
// no-op in stock QTreeWidget, and DlgPreferences installs no keyPressEvent
// override or focus-chaining to redirect it into the page widget on the
// right. A keyboard/screen-reader user can therefore only reach a page's
// controls via Tab, not via the arrow keys they'd naturally try after
// landing on a category. If DlgPreferences ever grows an override for this,
// this test's expectations should flip along with it.
TEST_F(DlgPreferencesTreeSyncTest, RightArrowOnLeafItem_StaysInTreeNoOp) {
    QSignalSpy currentItemChangedSpy(m_pTree.get(), &QTreeWidget::currentItemChanged);
    m_pTree->show();
    m_pTree->setFocus();

    QTest::keyClick(m_pTree.get(), Qt::Key_Right);

    EXPECT_EQ(m_pTree->currentItem(), m_pItemA);
    EXPECT_EQ(currentItemChangedSpy.count(), 0);
}

} // namespace

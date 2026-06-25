#include "widget/wlibrarysidebar.h"

#include <gtest/gtest.h>

#include <QModelIndex>
#include <QSignalSpy>
#include <QStandardItem>
#include <QStandardItemModel>
#include <memory>

#include "test/mixxxtest.h"

class WLibrarySidebarTest : public MixxxTest {
  protected:
    void SetUp() override {
        m_pModel = std::make_unique<QStandardItemModel>();
        m_pModel->appendRow(new QStandardItem(QStringLiteral("Tracks")));
        m_pModel->appendRow(new QStandardItem(QStringLiteral("Auto DJ")));
        m_pModel->appendRow(new QStandardItem(QStringLiteral("Playlists")));

        m_pSidebar = std::make_unique<WLibrarySidebar>();
        m_pSidebar->setModel(m_pModel.get());
    }

    std::unique_ptr<QStandardItemModel> m_pModel;
    std::unique_ptr<WLibrarySidebar> m_pSidebar;
};

// currentIndexChanged fires when selectIndex is called and carries the
// correct QModelIndex / display text.
TEST_F(WLibrarySidebarTest, SelectIndex_EmitsCurrentIndexChangedWithCorrectItem) {
    QSignalSpy spy(m_pSidebar.get(), &WLibrarySidebar::currentIndexChanged);

    const QModelIndex target = m_pModel->index(2, 0); // "Playlists"
    m_pSidebar->selectIndex(target);

    ASSERT_EQ(spy.count(), 1);
    const auto emitted = spy.at(0).at(0).value<QModelIndex>();
    EXPECT_EQ(emitted, target);
    EXPECT_EQ(emitted.data(Qt::DisplayRole).toString(), QStringLiteral("Playlists"));
}

// Passing an invalid index must not emit — WLibrarySidebar::selectIndex
// returns early, so currentChanged is never called.
TEST_F(WLibrarySidebarTest, SelectIndex_InvalidIndex_DoesNotEmit) {
    QSignalSpy spy(m_pSidebar.get(), &WLibrarySidebar::currentIndexChanged);
    m_pSidebar->selectIndex(QModelIndex());
    EXPECT_EQ(spy.count(), 0);
}

// ---------------------------------------------------------------------------
// Regression: currentIndexChanged must survive selectIndex() replacing the
// QItemSelectionModel.
//
// The original code in Library::bindSidebarWidget connected directly to
// pSidebarWidget->selectionModel()->currentChanged.  selectIndex() creates a
// brand-new QItemSelectionModel and calls deleteLater() on the old one, which
// silently orphaned that connection.  After startup (activateDefaultSelection
// calls selectIndex), arrow-key navigation never announced.
//
// The fix: override QAbstractItemView::currentChanged (a view-level virtual
// that QTreeView calls after rewiring to any new model) and emit
// currentIndexChanged from there.  The signal is owned by the view, not the
// model, so it is never invalidated by selectIndex().
// ---------------------------------------------------------------------------
TEST_F(WLibrarySidebarTest, CurrentIndexChanged_PersistsThroughModelReplacement) {
    QSignalSpy spy(m_pSidebar.get(), &WLibrarySidebar::currentIndexChanged);

    // First selectIndex() — replaces the initial QItemSelectionModel (mirrors
    // what activateDefaultSelection() does at startup).
    m_pSidebar->selectIndex(m_pModel->index(0, 0)); // "Tracks"
    ASSERT_EQ(spy.count(), 1) << "First selectIndex() did not emit";

    // Second selectIndex() — works on the replacement model.  With the old
    // selectionModel()->currentChanged connection this would silently do
    // nothing because the connected model was already deleted.
    m_pSidebar->selectIndex(m_pModel->index(1, 0)); // "Auto DJ"
    EXPECT_EQ(spy.count(), 2)
            << "currentIndexChanged did not fire after selectIndex() replaced "
               "the QItemSelectionModel — sidebar items will not be narrated";

    const auto emitted = spy.at(1).at(0).value<QModelIndex>();
    EXPECT_EQ(emitted.data(Qt::DisplayRole).toString(), QStringLiteral("Auto DJ"));
}

// Tests for the accessibility keyboard paths added to LibraryControl for
// track-list sorting (issue #59): cycling through sortable columns and
// toggling the sort order without a mouse.
#include "library/librarycontrol.h"

#include <gtest/gtest.h>

#include <QMap>
#include <QSet>
#include <QSqlDatabase>
#include <memory>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "library/trackmodel.h"
#include "test/mixxxtest.h"

namespace {

// Minimal TrackModel stub exposing just enough of the interface for
// LibraryControl::findNextSortableColumnId(), which only consults column
// count plus the internal/sortable flags and the SortColumnId <-> column
// index mapping. Everything else in the (large) TrackModel interface is
// unused by that function and stubbed out trivially.
class FakeTrackModel : public TrackModel {
  public:
    FakeTrackModel()
            : TrackModel(QSqlDatabase(), "faketrackmodel") {
    }

    // Registers column `index` as the (only) column that maps to sort
    // column `id`.
    void setColumn(int index, TrackModel::SortColumnId id) {
        m_idByColumn[index] = id;
    }
    void setInternal(int index) {
        m_internalColumns.insert(index);
    }
    void setUnsortable(int index) {
        m_unsortableColumns.insert(index);
    }

    // TrackModel interface (unused members of the interface are stubbed).
    TrackPointer getTrack(const QModelIndex&) const override {
        return nullptr;
    }
    TrackPointer getTrackByRef(const TrackRef&) const override {
        return nullptr;
    }
    QUrl getTrackUrl(const QModelIndex&) const override {
        return QUrl();
    }
    QString getTrackLocation(const QModelIndex&) const override {
        return QString();
    }
    TrackId getTrackId(const QModelIndex&) const override {
        return TrackId();
    }
    CoverInfo getCoverInfo(const QModelIndex&) const override {
        return CoverInfo();
    }
    const QVector<int> getTrackRows(TrackId) const override {
        return QVector<int>();
    }
    void search(const QString&) override {
    }
    const QString currentSearch() const override {
        return QString();
    }
    bool isColumnInternal(int column) override {
        return m_internalColumns.contains(column);
    }
    bool isColumnHiddenByDefault(int) override {
        return false;
    }
    bool isColumnSortable(int column) const override {
        return !m_unsortableColumns.contains(column);
    }
    TrackModel::SortColumnId sortColumnIdFromColumnIndex(int index) const override {
        return m_idByColumn.value(index, TrackModel::SortColumnId::Invalid);
    }
    int columnIndexFromSortColumnId(TrackModel::SortColumnId sortColumn) const override {
        return m_idByColumn.key(sortColumn, -1);
    }
    QString modelKey(bool) const override {
        return QString();
    }
    bool updateTrackGenre(Track*, const QString&) const override {
        return true;
    }
#if defined(__EXTRA_METADATA__)
    bool updateTrackMood(Track*, const QString&) const override {
        return true;
    }
#endif

  private:
    QMap<int, TrackModel::SortColumnId> m_idByColumn;
    QSet<int> m_internalColumns;
    QSet<int> m_unsortableColumns;
};

} // namespace

// ---------------------------------------------------------------------------
// LibraryControl::findNextSortableColumnId
// ---------------------------------------------------------------------------

TEST(LibraryControlSortCycleTest, NextSkipsToAdjacentSortableColumn) {
    FakeTrackModel model;
    model.setColumn(0, TrackModel::SortColumnId::Artist);
    model.setColumn(1, TrackModel::SortColumnId::Title);
    model.setColumn(2, TrackModel::SortColumnId::Album);

    EXPECT_EQ(TrackModel::SortColumnId::Title,
            LibraryControl::findNextSortableColumnId(
                    &model, 3, TrackModel::SortColumnId::Artist, 1));
}

TEST(LibraryControlSortCycleTest, NextWrapsAroundAtTheEnd) {
    FakeTrackModel model;
    model.setColumn(0, TrackModel::SortColumnId::Artist);
    model.setColumn(1, TrackModel::SortColumnId::Title);
    model.setColumn(2, TrackModel::SortColumnId::Album);

    EXPECT_EQ(TrackModel::SortColumnId::Artist,
            LibraryControl::findNextSortableColumnId(
                    &model, 3, TrackModel::SortColumnId::Album, 1));
}

TEST(LibraryControlSortCycleTest, PrevWalksBackwardAndWraps) {
    FakeTrackModel model;
    model.setColumn(0, TrackModel::SortColumnId::Artist);
    model.setColumn(1, TrackModel::SortColumnId::Title);
    model.setColumn(2, TrackModel::SortColumnId::Album);

    EXPECT_EQ(TrackModel::SortColumnId::Title,
            LibraryControl::findNextSortableColumnId(
                    &model, 3, TrackModel::SortColumnId::Album, -1));
    EXPECT_EQ(TrackModel::SortColumnId::Album,
            LibraryControl::findNextSortableColumnId(
                    &model, 3, TrackModel::SortColumnId::Artist, -1));
}

TEST(LibraryControlSortCycleTest, SkipsInternalAndUnsortableColumns) {
    FakeTrackModel model;
    model.setColumn(0, TrackModel::SortColumnId::Artist);
    model.setColumn(1, TrackModel::SortColumnId::Title);
    model.setInternal(1); // e.g. an internal id column between Artist/Album
    model.setColumn(2, TrackModel::SortColumnId::Album);
    model.setUnsortable(2); // e.g. a cover-art column that can't be sorted
    model.setColumn(3, TrackModel::SortColumnId::Genre);

    EXPECT_EQ(TrackModel::SortColumnId::Genre,
            LibraryControl::findNextSortableColumnId(
                    &model, 4, TrackModel::SortColumnId::Artist, 1));
}

TEST(LibraryControlSortCycleTest, NoSortableColumnsReturnsInvalid) {
    FakeTrackModel model;
    model.setColumn(0, TrackModel::SortColumnId::Artist);
    model.setInternal(0);

    EXPECT_EQ(TrackModel::SortColumnId::Invalid,
            LibraryControl::findNextSortableColumnId(
                    &model, 1, TrackModel::SortColumnId::Artist, 1));
}

TEST(LibraryControlSortCycleTest, NullModelReturnsInvalid) {
    EXPECT_EQ(TrackModel::SortColumnId::Invalid,
            LibraryControl::findNextSortableColumnId(
                    nullptr, 3, TrackModel::SortColumnId::Artist, 1));
}

TEST(LibraryControlSortCycleTest, NoCurrentSortStartsFromTheFirstColumn) {
    FakeTrackModel model;
    model.setColumn(0, TrackModel::SortColumnId::Artist);
    model.setColumn(1, TrackModel::SortColumnId::Title);

    // TrackModel::SortColumnId::Invalid (no active sort yet) has no matching
    // column index, so cycling should simply start from column 0.
    EXPECT_EQ(TrackModel::SortColumnId::Title,
            LibraryControl::findNextSortableColumnId(
                    &model, 2, TrackModel::SortColumnId::Invalid, 1));
}

// ---------------------------------------------------------------------------
// LibraryControl::slotSortColumnToggle, driven via the [Library],
// sort_column_toggle ControlObject. This is the mechanism the keyboard
// bindings for sort_column_next/sort_column_prev/sort_focused_column all
// ultimately go through (see LibraryControl::slotSortColumnCycle()), so
// exercising it here covers the shared "new column -> ascending; same
// column -> flip order" behaviour without needing a live WTrackTableView.
// ---------------------------------------------------------------------------

class LibraryControlSortToggleTest : public MixxxTest {
  protected:
    void SetUp() override {
        // No Library needed - the sort-toggle slots don't touch it.
        m_pLibraryControl = std::make_unique<LibraryControl>(nullptr, m_pConfig);
        m_pSortColumn = std::make_unique<ControlProxy>(
                QStringLiteral("[Library]"), QStringLiteral("sort_column"));
        m_pSortOrder = std::make_unique<ControlProxy>(
                QStringLiteral("[Library]"), QStringLiteral("sort_order"));
        m_pSortColumnToggle = std::make_unique<ControlProxy>(
                QStringLiteral("[Library]"), QStringLiteral("sort_column_toggle"));
    }

    std::unique_ptr<LibraryControl> m_pLibraryControl;
    std::unique_ptr<ControlProxy> m_pSortColumn;
    std::unique_ptr<ControlProxy> m_pSortOrder;
    std::unique_ptr<ControlProxy> m_pSortColumnToggle;
};

TEST_F(LibraryControlSortToggleTest, NewColumnSortsAscending) {
    m_pSortColumnToggle->set(static_cast<double>(TrackModel::SortColumnId::Title));

    EXPECT_EQ(static_cast<int>(TrackModel::SortColumnId::Title),
            static_cast<int>(m_pSortColumn->get()));
    EXPECT_EQ(0.0, m_pSortOrder->get());
}

TEST_F(LibraryControlSortToggleTest, RepeatingSameColumnTogglesOrder) {
    m_pSortColumnToggle->set(static_cast<double>(TrackModel::SortColumnId::Title));
    EXPECT_EQ(0.0, m_pSortOrder->get());

    m_pSortColumnToggle->set(static_cast<double>(TrackModel::SortColumnId::Title));
    EXPECT_EQ(1.0, m_pSortOrder->get());

    m_pSortColumnToggle->set(static_cast<double>(TrackModel::SortColumnId::Title));
    EXPECT_EQ(0.0, m_pSortOrder->get());
}

TEST_F(LibraryControlSortToggleTest, SwitchingColumnResetsToAscending) {
    m_pSortColumnToggle->set(static_cast<double>(TrackModel::SortColumnId::Title));
    m_pSortColumnToggle->set(static_cast<double>(TrackModel::SortColumnId::Title));
    ASSERT_EQ(1.0, m_pSortOrder->get()); // now descending

    m_pSortColumnToggle->set(static_cast<double>(TrackModel::SortColumnId::Bpm));
    EXPECT_EQ(static_cast<int>(TrackModel::SortColumnId::Bpm),
            static_cast<int>(m_pSortColumn->get()));
    EXPECT_EQ(0.0, m_pSortOrder->get());
}

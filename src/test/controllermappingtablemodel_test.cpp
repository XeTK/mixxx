// Tests for the accessible-text behavior of ControllerMappingTableModel and
// its concrete input/output subclasses.
//
// The production change under test makes headerData() return header text for
// Qt::AccessibleTextRole in addition to Qt::DisplayRole, so that screen
// readers can announce the column headers of the MIDI mapping tables.
#include <gtest/gtest.h>

#include "controllers/controllerinputmappingtablemodel.h"
#include "controllers/controlleroutputmappingtablemodel.h"
#include "controllers/controllermappingtablemodel.h"
#include "controllers/midi/legacymidicontrollermapping.h"
#include "test/mixxxtest.h"

namespace {

// Minimal concrete subclass used to exercise the base class headerData()
// logic directly, including the EditRole and section-number fallbacks.
class TestMappingTableModel : public ControllerMappingTableModel {
  public:
    TestMappingTableModel()
            : ControllerMappingTableModel(nullptr, nullptr, nullptr) {
    }

    QString getDisplayString(const QModelIndex& index) const override {
        Q_UNUSED(index);
        return QString();
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        Q_UNUSED(index);
        Q_UNUSED(role);
        return QVariant();
    }

  protected:
    void onMappingLoaded() override {
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return 0;
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return 3;
    }
};

} // namespace

class ControllerMappingTableModelTest : public MixxxTest {
};

// ---------------------------------------------------------------------------
// Base class headerData() accessible-text behavior
// ---------------------------------------------------------------------------

TEST_F(ControllerMappingTableModelTest, HeaderDataReturnsTextForDisplayAndAccessibleRoles) {
    TestMappingTableModel model;
    // Production sets headers via setHeaderData() with the default EditRole.
    ASSERT_TRUE(model.setHeaderData(0, Qt::Horizontal, QStringLiteral("Channel")));
    ASSERT_TRUE(model.setHeaderData(1, Qt::Horizontal, QStringLiteral("Opcode")));

    EXPECT_QSTRING_EQ("Channel", model.headerData(0, Qt::Horizontal, Qt::DisplayRole).toString());
    EXPECT_QSTRING_EQ("Channel",
            model.headerData(0, Qt::Horizontal, Qt::AccessibleTextRole).toString());
    EXPECT_QSTRING_EQ("Opcode", model.headerData(1, Qt::Horizontal, Qt::DisplayRole).toString());
    EXPECT_QSTRING_EQ("Opcode",
            model.headerData(1, Qt::Horizontal, Qt::AccessibleTextRole).toString());
}

TEST_F(ControllerMappingTableModelTest, HeaderDataAccessibleRoleFallsBackToEditRole) {
    TestMappingTableModel model;
    // Only the EditRole value is set; both DisplayRole and AccessibleTextRole
    // should fall back to it.
    ASSERT_TRUE(model.setHeaderData(0, Qt::Horizontal, QStringLiteral("Action")));

    EXPECT_QSTRING_EQ("Action", model.headerData(0, Qt::Horizontal, Qt::DisplayRole).toString());
    EXPECT_QSTRING_EQ("Action",
            model.headerData(0, Qt::Horizontal, Qt::AccessibleTextRole).toString());
}

TEST_F(ControllerMappingTableModelTest, HeaderDataUnsetColumnFallsBackToSectionNumber) {
    TestMappingTableModel model;
    // Column 2 has no header set; both roles should report the section number.
    EXPECT_QSTRING_EQ("2", model.headerData(2, Qt::Horizontal, Qt::DisplayRole).toString());
    EXPECT_QSTRING_EQ("2",
            model.headerData(2, Qt::Horizontal, Qt::AccessibleTextRole).toString());
}

TEST_F(ControllerMappingTableModelTest, HeaderDataIgnoresVerticalOrientation) {
    TestMappingTableModel model;
    ASSERT_TRUE(model.setHeaderData(0, Qt::Horizontal, QStringLiteral("Channel")));

    // The accessible-text override only applies to horizontal headers. Vertical
    // headers are delegated to the base class and must not report the
    // horizontal header text.
    EXPECT_NE(QStringLiteral("Channel"),
            model.headerData(0, Qt::Vertical, Qt::AccessibleTextRole).toString());
    EXPECT_NE(QStringLiteral("Channel"),
            model.headerData(0, Qt::Vertical, Qt::DisplayRole).toString());
}

// ---------------------------------------------------------------------------
// Concrete input/output models: real headers for both roles
// ---------------------------------------------------------------------------

TEST_F(ControllerMappingTableModelTest, InputModelReturnsHeaderTextForBothRoles) {
    ControllerInputMappingTableModel model(nullptr, nullptr, nullptr);
    model.setMapping(std::make_shared<LegacyMidiControllerMapping>());

    const QStringList expectedHeaders = {
            QStringLiteral("Channel"),
            QStringLiteral("Opcode"),
            QStringLiteral("Control"),
            QStringLiteral("Options"),
            QStringLiteral("Action"),
            QStringLiteral("Comment"),
    };

    ASSERT_EQ(expectedHeaders.size(), model.columnCount());
    for (int column = 0; column < expectedHeaders.size(); ++column) {
        EXPECT_QSTRING_EQ(expectedHeaders.at(column),
                model.headerData(column, Qt::Horizontal, Qt::DisplayRole).toString());
        EXPECT_QSTRING_EQ(expectedHeaders.at(column),
                model.headerData(column, Qt::Horizontal, Qt::AccessibleTextRole).toString());
    }
}

TEST_F(ControllerMappingTableModelTest, OutputModelReturnsHeaderTextForBothRoles) {
    ControllerOutputMappingTableModel model(nullptr, nullptr, nullptr);
    model.setMapping(std::make_shared<LegacyMidiControllerMapping>());

    const QStringList expectedHeaders = {
            QStringLiteral("Channel"),
            QStringLiteral("Opcode"),
            QStringLiteral("Control"),
            QStringLiteral("On Value"),
            QStringLiteral("Off Value"),
            QStringLiteral("Action"),
            QStringLiteral("On Range Min"),
            QStringLiteral("On Range Max"),
            QStringLiteral("Comment"),
    };

    ASSERT_EQ(expectedHeaders.size(), model.columnCount());
    for (int column = 0; column < expectedHeaders.size(); ++column) {
        EXPECT_QSTRING_EQ(expectedHeaders.at(column),
                model.headerData(column, Qt::Horizontal, Qt::DisplayRole).toString());
        EXPECT_QSTRING_EQ(expectedHeaders.at(column),
                model.headerData(column, Qt::Horizontal, Qt::AccessibleTextRole).toString());
    }
}

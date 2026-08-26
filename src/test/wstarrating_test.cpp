// Unit tests for WStarRating's keyboard support (issue #63): the widget was
// mouse-only when used in DlgTrackInfo (and, incidentally, the track
// context-menu WStarRatingAction). QTest::keyClick() dispatches the event
// straight to the widget, so no real window focus is needed to exercise
// keyPressEvent().

#include "widget/wstarrating.h"

#include <gtest/gtest.h>

#include <QSignalSpy>
#include <QTest>

class WStarRatingTest : public ::testing::Test {
  public:
    WStarRating widget{nullptr};
};

TEST_F(WStarRatingTest, RightArrowIncreasesRating) {
    widget.slotSetRating(2);
    QSignalSpy spy(&widget, &WStarRating::ratingChangeRequest);

    QTest::keyClick(&widget, Qt::Key_Right);

    ASSERT_EQ(1, spy.count());
    EXPECT_EQ(3, spy.takeFirst().at(0).toInt());
}

TEST_F(WStarRatingTest, LeftArrowDecreasesRating) {
    widget.slotSetRating(2);
    QSignalSpy spy(&widget, &WStarRating::ratingChangeRequest);

    QTest::keyClick(&widget, Qt::Key_Left);

    ASSERT_EQ(1, spy.count());
    EXPECT_EQ(1, spy.takeFirst().at(0).toInt());
}

TEST_F(WStarRatingTest, LeftArrowAtZeroStaysAtZero) {
    widget.slotSetRating(0);
    QSignalSpy spy(&widget, &WStarRating::ratingChangeRequest);

    QTest::keyClick(&widget, Qt::Key_Left);

    EXPECT_EQ(0, spy.count());
}

TEST_F(WStarRatingTest, RightArrowAtMaxStaysAtMax) {
    widget.slotSetRating(5);
    QSignalSpy spy(&widget, &WStarRating::ratingChangeRequest);

    QTest::keyClick(&widget, Qt::Key_Right);

    EXPECT_EQ(0, spy.count());
}

TEST_F(WStarRatingTest, DigitKeySetsRatingDirectly) {
    widget.slotSetRating(1);
    QSignalSpy spy(&widget, &WStarRating::ratingChangeRequest);

    QTest::keyClick(&widget, Qt::Key_5);

    ASSERT_EQ(1, spy.count());
    EXPECT_EQ(5, spy.takeFirst().at(0).toInt());
}

TEST_F(WStarRatingTest, HomeKeyClearsRating) {
    widget.slotSetRating(3);
    QSignalSpy spy(&widget, &WStarRating::ratingChangeRequest);

    QTest::keyClick(&widget, Qt::Key_Home);

    ASSERT_EQ(1, spy.count());
    EXPECT_EQ(0, spy.takeFirst().at(0).toInt());
}

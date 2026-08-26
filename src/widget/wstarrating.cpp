#include "widget/wstarrating.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QStyleOption>
#include <QStylePainter>

#include "moc_wstarrating.cpp"

class QEvent;
class QWidgets;

WStarRating::WStarRating(QWidget* pParent)
        : WWidget(pParent),
          m_starCount(0),
          m_visualStarRating(m_starCount) {
}

void WStarRating::setup(const QDomNode& node, const SkinContext& context) {
    Q_UNUSED(node);
    Q_UNUSED(context);
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
}

QSize WStarRating::sizeHint() const {
    // Center rating horizontally and vertically
    m_contentRect.setRect(
            (size().width() - m_visualStarRating.sizeHint().width()) / 2,
            (size().height() - m_visualStarRating.sizeHint().height()) / 2,
            m_visualStarRating.sizeHint().width(),
            m_visualStarRating.sizeHint().height());

    return size();
}

void WStarRating::slotSetRating(int starCount) {
    if (starCount == m_starCount || !m_visualStarRating.verifyStarCount(starCount)) {
        return;
    }
    m_starCount = starCount;
    updateVisualRating(starCount);
}

void WStarRating::paintEvent(QPaintEvent * /*unused*/) {
    QStyleOption option;
    option.initFrom(this);
    QStylePainter painter(this);

    painter.setBrush(option.palette.text());
    painter.drawPrimitive(QStyle::PE_Widget, option);

    m_visualStarRating.paint(&painter, m_contentRect);
}

void WStarRating::mouseMoveEvent(QMouseEvent *event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const int pos = event->position().toPoint().x();
#else
    const int pos = event->x();
#endif
    int star = m_visualStarRating.starAtPosition(pos, rect());

    if (star == StarRating::kInvalidStarCount) {
        resetVisualRating();
    } else {
        updateVisualRating(star);
    }
}

void WStarRating::leaveEvent(QEvent* /*unused*/) {
    resetVisualRating();
}

void WStarRating::updateVisualRating(int starCount) {
    if (starCount == m_visualStarRating.starCount()) {
        return;
    }
    m_visualStarRating.setStarCount(starCount);
    update();
}

void WStarRating::mouseReleaseEvent(QMouseEvent* /*unused*/) {
    int starCount = m_visualStarRating.starCount();
    emit ratingChangeRequest(starCount);
}

void WStarRating::keyPressEvent(QKeyEvent* event) {
    const int maxStars = m_visualStarRating.maxStarCount();
    int newCount = m_starCount;
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Down:
        newCount = m_starCount > StarRating::kMinStarCount ? m_starCount - 1 : m_starCount;
        break;
    case Qt::Key_Right:
    case Qt::Key_Up:
        newCount = m_starCount < maxStars ? m_starCount + 1 : m_starCount;
        break;
    case Qt::Key_Home:
        newCount = StarRating::kMinStarCount;
        break;
    case Qt::Key_End:
        newCount = maxStars;
        break;
    default:
        // Digit keys 0..9 set the rating directly (clamped to the max, e.g.
        // 5 stars), so a keyboard user can jump straight to a rating instead
        // of stepping through it one star at a time.
        if (event->key() >= Qt::Key_0 && event->key() <= Qt::Key_9) {
            newCount = qMin(event->key() - Qt::Key_0, maxStars);
        } else {
            WWidget::keyPressEvent(event);
            return;
        }
        break;
    }

    if (newCount != m_starCount) {
        m_starCount = newCount;
        updateVisualRating(newCount);
        emit ratingChangeRequest(newCount);
    }
    event->accept();
}

void WStarRating::fillDebugTooltip(QStringList* debug) {
    WWidget::fillDebugTooltip(debug);

    QString currentRating;
    currentRating.setNum(m_starCount);
    QString maximumRating = QString::number(m_visualStarRating.maxStarCount());

    *debug << QString("Rating: %1/%2").arg(currentRating, maximumRating);
}

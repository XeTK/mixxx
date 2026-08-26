#pragma once

#include "library/starrating.h"
#include "widget/wwidget.h"

class QDomNode;
class SkinContext;

class WStarRating : public WWidget {
    Q_OBJECT
  public:
    WStarRating(QWidget* pParent);

    virtual void setup(const QDomNode& node, const SkinContext& context);
    QSize sizeHint() const override;

  public slots:
    void slotSetRating(int starCount);

  signals:
    void ratingChangeRequest(int starCount);

  protected:
    void paintEvent(QPaintEvent* e) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent * /*unused*/) override;
    void fillDebugTooltip(QStringList* debug) override;
    // Issue #63: lets the rating be set from the keyboard once a caller
    // (e.g. DlgTrackInfo) opts the widget into StrongFocus. Harmless
    // elsewhere: skin usage explicitly sets Qt::NoFocus in setup() below, so
    // this can never be reached there, and the default WWidget ClickFocus
    // (e.g. the track-context-menu WStarRatingAction) only reaches it after
    // an explicit click, which is a reasonable bonus.
    void keyPressEvent(QKeyEvent* event) override;

  private:
    int m_starCount;

    StarRating m_visualStarRating;
    mutable QRect m_contentRect;

    void updateVisualRating(int starCount);
    void resetVisualRating() {
        updateVisualRating(m_starCount);
    }
};

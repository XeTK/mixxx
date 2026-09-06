#include "widget/wglwidget.h"

WGLWidget::WGLWidget(QWidget* pParent)
        : QWidget(pParent),
          m_pTrackDropTarget(nullptr) {
    // Intentionally never shown/rendered - see wglwidgetnull.h for why this
    // stand-in exists at all on iOS.
}

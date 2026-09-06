#include "waveform/sharedglcontext.h"

#include <QtGlobal>
#if !defined(MIXXX_USE_QOPENGL) && !defined(Q_OS_IOS)
// This debug-logging branch uses QGLContext/QGLFormat, which were removed
// entirely in Qt6 - it's already dead code under Qt6 on every platform
// (MIXXX_USE_QOPENGL is unconditionally forced ON for QT6 everywhere
// except iOS, where Qt6::OpenGL doesn't exist at all instead - see the
// QOPENGL definition in the top-level CMakeLists.txt). Excluded outright
// on iOS rather than left to fail to compile.
#include <QDebug>
#include <QGLContext>
#include <QGLFormat>

#include "widget/wglwidget.h"
#endif

WGLWidget* SharedGLContext::s_pSharedGLWidget = nullptr;

// static
void SharedGLContext::setWidget(WGLWidget* pWidget) {
    s_pSharedGLWidget = pWidget;
#if !defined(MIXXX_USE_QOPENGL) && !defined(Q_OS_IOS)
    qDebug() << "Set root GL Context widget valid:"
             << pWidget << (pWidget && pWidget->isValid());
    if (pWidget) {
        const QGLContext* pContext = pWidget->context();
        qDebug() << "Created root GL Context valid:" << pContext
                 << (pContext && pContext->isValid());
        QGLFormat format = pWidget->format();
        qDebug() << "Root GL Context format:";
        qDebug() << "Double Buffering:" << format.doubleBuffer();
        qDebug() << "Swap interval:" << format.swapInterval();
        qDebug() << "Depth buffer:" << format.depth();
        qDebug() << "Direct rendering:" << format.directRendering();
        qDebug() << "Has overlay:" << format.hasOverlay();
        qDebug() << "RGBA:" << format.rgba();
        qDebug() << "Sample buffers:" << format.sampleBuffers();
        qDebug() << "Samples:" << format.samples();
        qDebug() << "Stencil buffers:" << format.stencil();
        qDebug() << "Stereo:" << format.stereo();
    }
#endif
}

// static
WGLWidget* SharedGLContext::getWidget() {
    return s_pSharedGLWidget;
}

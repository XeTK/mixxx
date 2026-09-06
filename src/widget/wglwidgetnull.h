#pragma once

#ifndef WGLWIDGET_H
#error "Do not include this file, include wglwidget.h instead"
#endif

#include <QWidget>

////////////////////////////////////////////////////////////////////////////
// No-op WGLWidget for platforms with no Qt OpenGL module at all (iOS)
////////////////////////////////////////////////////////////////////////////
//
// iOS has neither of the other two WGLWidget backends available:
// - wglwidgetqopengl.h needs Qt6::OpenGL (QOpenGLWindow), which upstream
//   vcpkg's own qtbase port excludes for iOS entirely (confirmed by
//   actually attempting the build: Qt6OpenGLConfig.cmake is never produced
//   for arm64-ios-simulator).
// - wglwidgetqglwidget.h needs QGLWidget, which was removed from Qt
//   entirely as of Qt6 (confirmed: qglwidget.h doesn't exist anywhere in
//   this Qt6 vcpkg install, on any triplet) - it's already dead code on
//   every platform, not just iOS.
//
// WGLWidget itself, however, can't simply be left undefined: a handful of
// always-compiled classic-desktop-skin classes (WVuMeterBase, WSpinnyBase,
// GLWaveformWidgetAbstract) inherit from it directly, and iOS still needs
// mixxx-lib to link. Since iOS only ever runs the QML mobile skin - nothing
// under src/skin/legacy/ is ever instantiated at runtime there - a
// functionally-inert stand-in is correct, not just convenient: there is no
// real GL context to manage because this class is never asked to render
// anything.
class TrackDropTarget;

class WGLWidget : public QWidget {
  public:
    WGLWidget(QWidget* parent);
    ~WGLWidget() override = default;

    bool isContextValid() const {
        return false;
    }

    bool shouldRender() const {
        return false;
    }

    void makeCurrentIfNeeded() {
    }

    void doneCurrent() {
    }

    void swapBuffers() {
    }

    // called (indirectly) by WaveformWidgetFactory
    virtual void paintGL() {
    }
    // called by OpenGLWindow (real implementation only - never on iOS)
    virtual void resizeGL(int w, int h) {
        Q_UNUSED(w);
        Q_UNUSED(h);
    }
    virtual void initializeGL() {
    }

    void setTrackDropTarget(TrackDropTarget* pTarget) {
        m_pTrackDropTarget = pTarget;
    }
    TrackDropTarget* trackDropTarget() const {
        return m_pTrackDropTarget;
    }

    // The real implementation returns a QOpenGLWindow* (a Qt6::OpenGL type
    // unavailable here), but every declared caller either only needs
    // ->update() (a plain QWidget method) or is itself guarded behind
    // MIXXX_USE_QOPENGL (excluded on iOS) - returning this widget itself
    // satisfies the former without requiring the QOpenGLWindow type to
    // exist at all.
    QWidget* getOpenGLWindow() const {
        return const_cast<WGLWidget*>(this);
    }

  protected:
    // The real implementation returns its offscreen QOpenGLWindow as the
    // paint device; this widget is never shown or painted for real, so
    // returning itself (a QWidget is a valid QPaintDevice) is enough for
    // callers (WSpinny::paintEvent, WVuMeter::draw, ...) to construct a
    // QPainter that simply never gets used meaningfully.
    QPaintDevice* paintDevice() {
        return this;
    }

  private:
    TrackDropTarget* m_pTrackDropTarget;
};

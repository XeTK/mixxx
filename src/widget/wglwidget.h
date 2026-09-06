#pragma once

#include <QtGlobal>

// This define is checked in wglwidgetqglwidget.h and wglwidgetqglwidget.h
// to make sure they are only included from this header, to enforce that
// all code includes this header wglwidget.h.
#define WGLWIDGET_H

#ifdef MIXXX_USE_QOPENGL
#include "widget/wglwidgetqopengl.h"
#elif defined(Q_OS_IOS)
// Qt6::OpenGL (and thus wglwidgetqopengl.h's QOpenGLWindow-based
// implementation) doesn't exist on iOS, and wglwidgetqglwidget.h's
// QGLWidget-based implementation is dead code under Qt6 on every platform
// (QGLWidget was removed entirely in Qt6) - see wglwidgetnull.h.
#include "widget/wglwidgetnull.h"
#else
#include "widget/wglwidgetqglwidget.h"
#endif

#undef WGLWIDGET_H

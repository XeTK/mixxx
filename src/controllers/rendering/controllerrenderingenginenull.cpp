// No <QOpenGLContext> here on purpose: it (like <QOpenGLFramebufferObject>)
// compiles to essentially nothing on iOS - see controllerrenderingengine.h -
// and m_context doesn't exist as a member on iOS, so its complete type is
// never needed for this file's (defaulted) destructor.
#include <QOffscreenSurface>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QThread>

#include "controllers/rendering/controllerrenderingengine.h"
#include "moc_controllerrenderingengine.cpp"

////////////////////////////////////////////////////////////////////////////
// Always-invalid ControllerRenderingEngine for iOS
////////////////////////////////////////////////////////////////////////////
//
// The real implementation (controllerrenderingengine.cpp) renders a
// controller mapping's QML screen definition into an offscreen
// QOpenGLFramebufferObject to send frames to a physical controller's
// built-in display. QOpenGLFramebufferObject is part of the Qt6::OpenGL
// module, which doesn't exist for iOS at all (confirmed by actually
// attempting the build - see the QOPENGL definition in the top-level
// CMakeLists.txt).
//
// This isn't just a build-time workaround: iOS has no HID/USB controller
// support to begin with (see the HID CMake option - hidapi/libusb are
// excluded from the iOS vcpkg manifest, there's no IOKit USB device API on
// iOS), so there is no physical controller screen to ever render into on
// this platform regardless. Every caller (see
// ControllerScriptEngineLegacy::initialize()) already checks isValid()
// immediately after construction and skips all further use of the object
// when it's false, exactly as it does today on any desktop platform where
// GL initialization itself fails - this class just reports that
// unconditionally on iOS instead of only when a real GL context creation
// attempt fails.
ControllerRenderingEngine::ControllerRenderingEngine(
        const LegacyControllerMapping::ScreenInfo& info,
        gsl::not_null<ControllerEngineThreadControl*> engineThreadControl)
        : QObject(),
          m_screenInfo(info),
          m_GLDataFormat(0),
          m_GLDataType(0),
          m_isValid(false),
          m_pEngineThreadControl(engineThreadControl) {
    // Deliberately doesn't call prepare() (which would spin up a render
    // thread for a context that can never be created) - m_isValid is false
    // from construction, and every caller checks that before doing
    // anything else with this object.
}

ControllerRenderingEngine::~ControllerRenderingEngine() = default;

bool ControllerRenderingEngine::event(QEvent* pEvent) {
    return QObject::event(pEvent);
}

bool ControllerRenderingEngine::isRunning() const {
    return false;
}

void ControllerRenderingEngine::requestSendingFrameData(
        Controller* /*controller*/, const QByteArray& /*frame*/) {
}

void ControllerRenderingEngine::requestEngineSetup(std::shared_ptr<QQmlEngine> /*qmlEngine*/) {
}

void ControllerRenderingEngine::start() {
}

bool ControllerRenderingEngine::stop() {
    return true;
}

void ControllerRenderingEngine::finish() {
}

void ControllerRenderingEngine::renderFrame() {
}

void ControllerRenderingEngine::setup(std::shared_ptr<QQmlEngine> /*qmlEngine*/) {
}

void ControllerRenderingEngine::send(Controller* /*controller*/, const QByteArray& /*frame*/) {
}

void ControllerRenderingEngine::prepare() {
}

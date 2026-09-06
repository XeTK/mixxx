#include "qmlapplication.h"

#include <QFile>
#include <QQmlEngineExtensionPlugin>
#include <QQuickStyle>

#include "controllers/controllermanager.h"
#include "mixer/playermanager.h"
#include "moc_qmlapplication.cpp"
#include "qml/asyncimageprovider.h"
#include "qml/qmldlgpreferencesproxy.h"
#include "soundio/soundmanager.h"
#include "waveform/visualsmanager.h"
#include "waveform/waveformwidgetfactory.h"

#ifdef Q_OS_ANDROID
#include <android/log.h>
#define MIXXX_ANDROID_TRACE(msg) \
    __android_log_print(ANDROID_LOG_ERROR, "mixxx_trace", "%s", msg)
#else
#define MIXXX_ANDROID_TRACE(msg)
#endif

Q_IMPORT_QML_PLUGIN(MixxxPlugin)
Q_IMPORT_QML_PLUGIN(Mixxx_ControlsPlugin)

namespace {
// Both QML skin trees ship in every build; which one loads is decided at
// runtime. Android and iOS default to the phone-optimized skin, everything
// else to the desktop one, and the [QML],skin config key
// ("qml"/"qml-mobile") overrides the default on any platform - so the
// mobile skin can be previewed on a desktop without repackaging, and vice
// versa.
QString resolveMainQmlFilePath(const UserSettingsPointer& pSettings) {
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    const QString defaultSkin = QStringLiteral("qml-mobile");
#else
    const QString defaultSkin = QStringLiteral("qml");
#endif
    const QString skin = pSettings->getValue(
            ConfigKey(QStringLiteral("[QML]"), QStringLiteral("skin")),
            defaultSkin);
    const QString resourcePath = pSettings->getResourcePath();
    QString mainFilePath = resourcePath + skin + QStringLiteral("/main.qml");
    if (skin != defaultSkin && !QFile::exists(mainFilePath)) {
        qWarning() << "QML skin" << skin << "has no main.qml under"
                   << resourcePath << "- falling back to" << defaultSkin;
        mainFilePath = resourcePath + defaultSkin + QStringLiteral("/main.qml");
    }
    return mainFilePath;
}

// Converts a (capturing) lambda into a function pointer that can be passed to
// qmlRegisterSingletonType.
template<class F>
auto lambda_to_singleton_type_factory_ptr(F&& f) {
    static F fn = std::forward<F>(f);
    return [](QQmlEngine* pEngine, QJSEngine* pScriptEngine) -> QObject* {
        return fn(pEngine, pScriptEngine);
    };
}
} // namespace

namespace mixxx {
namespace qml {

QmlApplication::QmlApplication(
        QApplication* app,
        const CmdlineArgs& args)
        : m_pCoreServices(std::make_unique<mixxx::CoreServices>(args, app)),
          m_visualsManager(std::make_unique<VisualsManager>()),
          m_mainFilePath(resolveMainQmlFilePath(m_pCoreServices->getSettings())),
          m_pAppEngine(nullptr),
          m_autoReload() {
    MIXXX_ANDROID_TRACE("QmlApplication ctor: after CoreServices constructed");
    QQuickStyle::setStyle("Basic");

    MIXXX_ANDROID_TRACE("QmlApplication ctor: before CoreServices::initialize");
    m_pCoreServices->initialize(app);

    // MixxxMainWindow::initialize() does the equivalent of this for the
    // widgets UI. Without it, WaveformWidgetFactory::instance() stays null
    // and allshader::WaveformRenderMark (constructed fresh on every QML
    // repaint) fails its connect() calls every single frame, which
    // saturates the main thread. startVSync() is intentionally not called
    // here: that wires up the vsync-thread-driven render()/swap() path via
    // a GuiTick this class doesn't have, but the QML waveform display
    // drives its own redraws directly (see QmlWaveformDisplay), so it's
    // not needed just to make the singleton usable.
    WaveformWidgetFactory::createInstance();
    WaveformWidgetFactory::instance()->setConfig(m_pCoreServices->getSettings());

    MIXXX_ANDROID_TRACE("QmlApplication ctor: before setupDevices");
    SoundDeviceStatus result = m_pCoreServices->getSoundManager()->setupDevices();
    MIXXX_ANDROID_TRACE("QmlApplication ctor: after setupDevices");
    if (result != SoundDeviceStatus::Ok) {
        const int reInt = static_cast<int>(result);
        qCritical() << "Error setting up sound devices:" << reInt;
        MIXXX_ANDROID_TRACE("QmlApplication ctor: setupDevices failed, exiting");
        exit(reInt);
    }

    // FIXME: DlgPreferences has some initialization logic that must be executed
    // before the GUI is shown, at least for the effects system.
    MIXXX_ANDROID_TRACE("QmlApplication ctor: before makeDlgPreferences");
    std::shared_ptr<QDialog> pDlgPreferences = m_pCoreServices->makeDlgPreferences();
    // Without this, QApplication will quit when the last QWidget QWindow is
    // closed because it does not take into account the window created by
    // the QQmlApplicationEngine.
    pDlgPreferences->setAttribute(Qt::WA_QuitOnClose, false);

    // Since DlgPreferences is only meant to be used in the main QML engine, it
    // follows a strict singleton pattern design
    QmlDlgPreferencesProxy::s_pInstance =
            std::make_unique<QmlDlgPreferencesProxy>(pDlgPreferences, this);
    MIXXX_ANDROID_TRACE("QmlApplication ctor: before loadQml");
    loadQml(m_mainFilePath);
    MIXXX_ANDROID_TRACE("QmlApplication ctor: after loadQml");

    m_pCoreServices->getControllerManager()->setUpDevices();
    MIXXX_ANDROID_TRACE("QmlApplication ctor: after setUpDevices, ctor complete");

    connect(&m_autoReload,
            &QmlAutoReload::triggered,
            this,
            [this]() {
                loadQml(m_mainFilePath);
            });

    const QStringList visualGroups =
            m_pCoreServices->getPlayerManager()->getVisualPlayerGroups();
    for (const QString& group : visualGroups) {
        m_visualsManager->addDeck(group);
    }

    m_pCoreServices->getPlayerManager()->connect(
            m_pCoreServices->getPlayerManager().get(),
            &PlayerManager::numberOfDecksChanged,
            this,
            [this](int decks) {
                for (int i = 0; i < decks; ++i) {
                    QString group = PlayerManager::groupForDeck(i);
                    m_visualsManager->addDeckIfNotExist(group);
                }
            });
}

QmlApplication::~QmlApplication() {
    // Delete all the QML singletons in order to prevent leak detection in CoreService
    QmlDlgPreferencesProxy::s_pInstance.reset();
    m_visualsManager.reset();
    m_pAppEngine.reset();
    WaveformWidgetFactory::destroy();
    m_pCoreServices.reset();
}

void QmlApplication::loadQml(const QString& path) {
    // QQmlApplicationEngine::load creates a new window but also leaves the old one,
    // so it is necessary to destroy the old QQmlApplicationEngine and create a new one.
    m_pAppEngine = std::make_unique<QQmlApplicationEngine>();

    m_autoReload.clear();
    m_pAppEngine->addUrlInterceptor(&m_autoReload);
    m_pAppEngine->addImportPath(QStringLiteral(":/mixxx.org/imports"));

    // No memory leak here, the QQmlEngine takes ownership of the provider
    QQuickAsyncImageProvider* pImageProvider = new AsyncImageProvider(
            m_pCoreServices->getTrackCollectionManager());
    m_pAppEngine->addImageProvider(AsyncImageProvider::kProviderName, pImageProvider);

    m_pAppEngine->load(path);
    if (m_pAppEngine->rootObjects().isEmpty()) {
        qCritical() << "Failed to load QML file" << path;
    }
}

} // namespace qml
} // namespace mixxx

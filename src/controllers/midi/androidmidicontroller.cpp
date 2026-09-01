#include "controllers/midi/androidmidicontroller.h"

#include <android/log.h>

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QtCore/qnativeinterface.h>

#include "util/time.h"

namespace {
const QString kLogTag = QStringLiteral("mixxx");
}

AndroidMidiController::AndroidMidiController(
        const QString& deviceName, const QJniObject& deviceInfo)
        : MidiController(deviceName),
          m_deviceName(deviceName),
          m_deviceInfo(deviceInfo),
          m_callbackKey(0),
          m_parser(
                  [this](mixxx::MidiByteStreamParser::ShortMessage m) {
                      receivedShortMessage(m.status, m.data1, m.data2, m_currentMessageTimestamp);
                  },
                  [this](std::vector<uint8_t> data) {
                      receive(QByteArray(reinterpret_cast<const char*>(data.data()),
                                      static_cast<int>(data.size())),
                              m_currentMessageTimestamp);
                  }),
          m_openCompleted(false),
          m_openSucceeded(false) {
    const int inputPortCount = deviceInfo.callMethod<jint>("getInputPortCount");
    const int outputPortCount = deviceInfo.callMethod<jint>("getOutputPortCount");
    // See the port-direction naming note in the header: an Android
    // *output* port is where Mixxx *receives* from, and vice versa.
    setInputDevice(outputPortCount > 0);
    setOutputDevice(inputPortCount > 0);

    connect(this,
            &AndroidMidiController::androidDeviceOpened,
            this,
            &AndroidMidiController::handleDeviceOpened);
    connect(this,
            &AndroidMidiController::androidMidiDataReceived,
            this,
            &AndroidMidiController::handleMidiDataReceived);
}

AndroidMidiController::~AndroidMidiController() {
    if (isOpen()) {
        close();
    }
}

int AndroidMidiController::open(const QString& resourcePath) {
    if (isOpen()) {
        qWarning() << "Android MIDI device" << getName() << "already open";
        return -1;
    }

    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        qWarning() << "Android MIDI device" << getName() << ": no Android context";
        return -2;
    }

    m_callbackKey = mixxx::android::registerMidiDeviceCallback(this);
    m_bridge = QJniObject("org/mixxx/MidiDeviceBridge",
            "(Landroid/content/Context;J)V",
            context.object<jobject>(),
            static_cast<jlong>(m_callbackKey));
    if (!m_bridge.isValid()) {
        qWarning() << "Android MIDI device" << getName() << ": failed to create bridge";
        mixxx::android::unregisterMidiDeviceCallback(m_callbackKey);
        return -2;
    }

    {
        std::unique_lock lock(m_openMutex);
        m_openCompleted = false;
        m_openSucceeded = false;
    }

    m_bridge.callMethod<void>("open",
            "(Landroid/media/midi/MidiDeviceInfo;)V",
            m_deviceInfo.object<jobject>());

    // MidiManager.openDevice() is asynchronous; block until the JNI
    // callback (routed through onDeviceOpened() below) reports a result,
    // mirroring the existing mixxx::android::waitForPermission() pattern
    // used for HID USB permission grants elsewhere in this codebase. This
    // is safe: the callback arrives on Android's main looper thread, not
    // whatever thread open() itself runs on (ControllerManager's own
    // "Controller" thread), so there is no risk of blocking the thread
    // the callback needs to make progress on.
    {
        std::unique_lock lock(m_openMutex);
        int retries = 0;
        while (!m_openCond.wait_for(lock, std::chrono::seconds(1), [this] {
            return m_openCompleted;
        })) {
            QCoreApplication::processEvents();
            ++retries;
            if (retries >= 10) {
                qWarning() << "Timeout waiting for Android MIDI device to open:"
                           << getName();
                mixxx::android::unregisterMidiDeviceCallback(m_callbackKey);
                m_bridge = QJniObject();
                return -2;
            }
        }
        if (!m_openSucceeded) {
            qWarning() << "Failed to open Android MIDI device:" << getName();
            mixxx::android::unregisterMidiDeviceCallback(m_callbackKey);
            m_bridge = QJniObject();
            return -2;
        }
    }

    startEngine();
    applyMapping(resourcePath);
    setOpen(true);
    return 0;
}

int AndroidMidiController::close() {
    if (!isOpen()) {
        qWarning() << "Android MIDI device" << getName() << "already closed";
        return -1;
    }

    stopEngine();
    MidiController::close();

    if (m_bridge.isValid()) {
        m_bridge.callMethod<void>("close");
        m_bridge = QJniObject();
    }
    mixxx::android::unregisterMidiDeviceCallback(m_callbackKey);

    setOpen(false);
    return 0;
}

void AndroidMidiController::sendShortMsg(
        unsigned char status, unsigned char byte1, unsigned char byte2) {
    if (!m_bridge.isValid()) {
        return;
    }
    const jbyte bytes[3] = {
            static_cast<jbyte>(status), static_cast<jbyte>(byte1), static_cast<jbyte>(byte2)};
    QJniEnvironment env;
    jbyteArray jArray = env->NewByteArray(3);
    if (!jArray) {
        return;
    }
    env->SetByteArrayRegion(jArray, 0, 3, bytes);
    m_bridge.callMethod<jboolean>("send", "([BII)Z", jArray, 0, 3);
    env->DeleteLocalRef(jArray);
}

bool AndroidMidiController::sendBytes(const QByteArray& data) {
    if (!m_bridge.isValid() || data.isEmpty()) {
        return false;
    }
    QJniEnvironment env;
    jbyteArray jArray = env->NewByteArray(data.size());
    if (!jArray) {
        return false;
    }
    env->SetByteArrayRegion(jArray,
            0,
            data.size(),
            reinterpret_cast<const jbyte*>(data.constData()));
    const bool result = m_bridge.callMethod<jboolean>("send", "([BII)Z", jArray, 0, data.size());
    env->DeleteLocalRef(jArray);
    return result;
}

void AndroidMidiController::onDeviceOpened(bool success) {
    // Called from an arbitrary Java-side thread - see the class comment
    // on mixxx::android::MidiDeviceCallback. This also directly
    // satisfies the blocking wait in open() (via m_openCond), which is
    // safe to notify from here for the same reason: the wait is not on
    // this callback's own thread.
    {
        std::unique_lock lock(m_openMutex);
        m_openCompleted = true;
        m_openSucceeded = success;
    }
    m_openCond.notify_all();
    emit androidDeviceOpened(success);
}

void AndroidMidiController::onMidiDataReceived(const unsigned char* data, int length) {
    // Called from an arbitrary Java-side thread - see the class comment
    // on mixxx::android::MidiDeviceCallback. The timestamp is captured
    // here, at the moment of actual receipt, rather than later in
    // handleMidiDataReceived() once it's been queued across threads -
    // the same reasoning as Hss1394Controller's DeviceChannelListener.
    emit androidMidiDataReceived(
            QByteArray(reinterpret_cast<const char*>(data), length), mixxx::Time::elapsed());
}

void AndroidMidiController::handleDeviceOpened(bool success) {
    Q_UNUSED(success);
    // Nothing to do here beyond what onDeviceOpened() already handled
    // directly (unblocking open()'s wait) - this slot exists so a future
    // UI-facing notification (e.g. "controller disconnected") has
    // somewhere to hook in on the controller thread.
}

void AndroidMidiController::handleMidiDataReceived(const QByteArray& data, mixxx::Duration timestamp) {
    m_currentMessageTimestamp = timestamp;
    m_parser.feed(reinterpret_cast<const uint8_t*>(data.constData()), data.size());
}

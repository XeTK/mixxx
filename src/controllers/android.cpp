#include "android.h"

#include <android/api-level.h>
#include <android/log.h>
#include <qjnitypes.h>

#include <QCoreApplication>
#include <QDebug>
#include <QtCore/qnativeinterface.h>
#include <QtJniTypes>

#include <algorithm>
#include <cstddef>
#include <unordered_map>

namespace mixxx {
namespace android {
std::mutex s_androidLock = {};
std::condition_variable s_grantingWaitCond = {};
std::vector<std::pair<QJniObject, bool>> s_grantingResult = {};
QJniObject s_intent = {};
QJniObject s_usbManager = {};

namespace {
std::mutex s_midiCallbacksLock;
std::unordered_map<qint64, MidiDeviceCallback*> s_midiCallbacks;
qint64 s_nextMidiCallbackKey = 1;
} // namespace

qint64 registerMidiDeviceCallback(MidiDeviceCallback* pCallback) {
    std::unique_lock lock(s_midiCallbacksLock);
    const qint64 key = s_nextMidiCallbackKey++;
    s_midiCallbacks.emplace(key, pCallback);
    return key;
}

void unregisterMidiDeviceCallback(qint64 key) {
    std::unique_lock lock(s_midiCallbacksLock);
    s_midiCallbacks.erase(key);
}

namespace {

void dispatchMidiDeviceOpened(qint64 key, bool success) {
    std::unique_lock lock(s_midiCallbacksLock);
    const auto it = s_midiCallbacks.find(key);
    if (it != s_midiCallbacks.end()) {
        it->second->onDeviceOpened(success);
    }
}

void dispatchMidiDataReceived(qint64 key, const unsigned char* data, int length) {
    std::unique_lock lock(s_midiCallbacksLock);
    const auto it = s_midiCallbacks.find(key);
    if (it != s_midiCallbacks.end()) {
        it->second->onMidiDataReceived(data, length);
    }
}

} // namespace

const QJniObject& getIntent() {
    __android_log_print(ANDROID_LOG_VERBOSE, "mixxx", "about to get intent");
    std::unique_lock lock(s_androidLock);
    if (s_intent.isValid()) {
        return s_intent;
    }
    // QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
    if (!QNativeInterface::QAndroidApplication::isActivityContext()) {
        __android_log_print(ANDROID_LOG_WARN,
                "mixxx",
                "current context doesn't refer to an activity!");
    }

    QJniObject context = QNativeInterface::QAndroidApplication::context();

    s_usbManager = QJniObject("org/mixxx/UsbPermission");
    jint FLAG_IMMUTABLE =
            QJniObject::getStaticField<jint>(
                    "android/app/PendingIntent",
                    "FLAG_IMMUTABLE");
    QtJniTypes::String ACTION_USB_PERMISSION =
            QJniObject::fromString("org.mixxx.permissions.USB_PERMISSION");
    QtJniTypes::Intent intent = QJniObject("android/content/Intent",
            "(Ljava/lang/String;)V",
            ACTION_USB_PERMISSION.object<jstring>());
    if (!intent.isValid()) {
        __android_log_print(ANDROID_LOG_WARN, "mixxx", "pending intent is invalid!");
    }
    s_intent =
            QJniObject::callStaticMethod<jobject>("android/app/PendingIntent",
                    "getBroadcast",
                    "(Landroid/content/Context;ILandroid/content/"
                    "Intent;I)Landroid/app/PendingIntent;",
                    context,
                    0,
                    intent,
                    FLAG_IMMUTABLE);

    if (!s_intent.isValid()) {
        __android_log_print(ANDROID_LOG_WARN, "mixxx", "pending intent is invalid!");
    }

    __android_log_print(ANDROID_LOG_VERBOSE,
            "mixxx",
            "about to register the the receiver %d",
            s_usbManager.isValid());
    auto success = s_usbManager.callMethod<jboolean>("registerServiceBroadcastReceiver",
            "(Landroid/content/Context;)Z",
            context.object<jobject>());
    if (!success) {
        __android_log_print(ANDROID_LOG_WARN, "mixxx", "failed to registered the receiver!");
    }
    // });
    return s_intent;
}

bool waitForPermission(const QJniObject& device) {
    __android_log_print(ANDROID_LOG_VERBOSE, "mixxx", "about to wait for perm");
    std::unique_lock lock(s_androidLock);
    std::vector<std::pair<QJniObject, bool>>::const_iterator result = s_grantingResult.cend();

    int retries = 0;

    while (!s_grantingWaitCond.wait_for(
            lock, std::chrono::seconds(1), [&result, device] {
                result = std::find_if(s_grantingResult.cbegin(),
                        s_grantingResult.cend(),
                        [device](auto resultPair) {
                            return resultPair.first == device;
                        });
                return result != s_grantingResult.cend();
            })) {
        __android_log_print(ANDROID_LOG_VERBOSE,
                "mixxx",
                "Not found - current result count: %lu",
                s_grantingResult.size());
        QCoreApplication::processEvents();
        retries++;
        if (retries >= 10) {
            __android_log_print(ANDROID_LOG_WARN, "mixxx", "wait for perm timeout");
            qWarning() << "Timeout reached when waiting for Android permission to USB device";
            return false;
        }
    }
    __android_log_print(ANDROID_LOG_VERBOSE, "mixxx", "got perm result");
    return result->second;
}

void usbDeviceAccessResult(QJniObject device, bool granted) {
    std::unique_lock lock(s_androidLock);
    __android_log_print(ANDROID_LOG_WARN, "mixxx", "received permission result: %d", granted);
    s_grantingResult.push_back(std::make_pair<>(device, granted));
    s_grantingWaitCond.notify_one();
    // FIXME Handle large list?
}
} // namespace android
} // namespace mixxx

Q_DECLARE_JNI_CLASS(UsbPermissionClass, "org/mixxx/UsbPermission")

void usbDeviceAccessResult(JNIEnv*, jobject, jobject device, jboolean granted) {
    mixxx::android::usbDeviceAccessResult(device, granted);
}
Q_DECLARE_JNI_NATIVE_METHOD(usbDeviceAccessResult)

Q_DECLARE_JNI_CLASS(MidiDeviceBridgeClass, "org/mixxx/MidiDeviceBridge")

void midiDeviceOpened(JNIEnv*, jobject, jlong nativeKey, jboolean success) {
    mixxx::android::dispatchMidiDeviceOpened(nativeKey, success);
}
Q_DECLARE_JNI_NATIVE_METHOD(midiDeviceOpened)

void midiDataReceived(JNIEnv* env, jobject, jlong nativeKey, jbyteArray data, jint offset, jint count) {
    jbyte* bytes = env->GetByteArrayElements(data, nullptr);
    if (!bytes) {
        return;
    }
    mixxx::android::dispatchMidiDataReceived(
            nativeKey, reinterpret_cast<const unsigned char*>(bytes) + offset, count);
    env->ReleaseByteArrayElements(data, bytes, JNI_ABORT);
}
Q_DECLARE_JNI_NATIVE_METHOD(midiDataReceived)

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) {
    QJniEnvironment env;
    env.registerNativeMethods<QtJniTypes::UsbPermissionClass>({
            Q_JNI_NATIVE_METHOD(usbDeviceAccessResult),
    });
    env.registerNativeMethods<QtJniTypes::MidiDeviceBridgeClass>({
            Q_JNI_NATIVE_METHOD(midiDeviceOpened),
            Q_JNI_NATIVE_METHOD(midiDataReceived),
    });
    return JNI_VERSION_1_6;
}

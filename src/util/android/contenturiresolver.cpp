#include "util/android/contenturiresolver.h"

#include <android/log.h>

#include <QHash>
#include <QJniEnvironment>
#include <QJniObject>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QtCore/qnativeinterface.h>

namespace mixxx {
namespace android {

namespace {

// Bounds how many ParcelFileDescriptors (and their /proc/self/fd/N
// symlinks) are kept open at once. A full library scan resolves every
// track in sequence, so without a cap this would leak one fd per track
// and exhaust the process' descriptor limit well before a modest-sized
// library finished scanning.
constexpr int kMaxCachedDescriptors = 32;

QMutex s_cacheMutex;
QHash<QString, QJniObject> s_cachedDescriptorsByUri;
QHash<QString, QString> s_cachedPathsByUri;
QList<QString> s_cacheInsertionOrder;

// Must be called with s_cacheMutex held.
void evictOldestIfNeeded() {
    while (s_cacheInsertionOrder.size() > kMaxCachedDescriptors) {
        const QString oldestKey = s_cacheInsertionOrder.takeFirst();
        auto it = s_cachedDescriptorsByUri.find(oldestKey);
        if (it != s_cachedDescriptorsByUri.end()) {
            it->callMethod<void>("close");
            s_cachedDescriptorsByUri.erase(it);
        }
        s_cachedPathsByUri.remove(oldestKey);
    }
}

} // namespace

QString resolveContentUriToFilePath(const QUrl& contentUri) {
    const QString uriString = contentUri.toString();

    QMutexLocker locker(&s_cacheMutex);
    const auto cachedPathIt = s_cachedPathsByUri.constFind(uriString);
    if (cachedPathIt != s_cachedPathsByUri.constEnd()) {
        return cachedPathIt.value();
    }

    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        __android_log_print(ANDROID_LOG_WARN,
                "mixxx",
                "resolveContentUriToFilePath: no Android context");
        return {};
    }

    QJniObject contentResolver = context.callObjectMethod(
            "getContentResolver", "()Landroid/content/ContentResolver;");
    if (!contentResolver.isValid()) {
        return {};
    }

    QJniObject jUriString = QJniObject::fromString(uriString);
    QJniObject uri = QJniObject::callStaticMethod<jobject>("android/net/Uri",
            "parse",
            "(Ljava/lang/String;)Landroid/net/Uri;",
            jUriString.object<jstring>());
    if (!uri.isValid()) {
        return {};
    }

    QJniObject jMode = QJniObject::fromString("r");
    QJniObject parcelFileDescriptor = contentResolver.callObjectMethod(
            "openFileDescriptor",
            "(Landroid/net/Uri;Ljava/lang/String;)Landroid/os/ParcelFileDescriptor;",
            uri.object<jobject>(),
            jMode.object<jstring>());

    QJniEnvironment jniEnv;
    if (jniEnv.checkAndClearExceptions()) {
        // Most commonly a FileNotFoundException - the document was
        // deleted, renamed, or its grant was revoked since it was
        // scanned into the library.
        __android_log_print(ANDROID_LOG_WARN,
                "mixxx",
                "resolveContentUriToFilePath: openFileDescriptor threw for %s",
                uriString.toUtf8().constData());
        return {};
    }
    if (!parcelFileDescriptor.isValid()) {
        __android_log_print(ANDROID_LOG_WARN,
                "mixxx",
                "resolveContentUriToFilePath: openFileDescriptor returned null for %s",
                uriString.toUtf8().constData());
        return {};
    }

    const jint fd = parcelFileDescriptor.callMethod<jint>("getFd");
    if (fd < 0) {
        parcelFileDescriptor.callMethod<void>("close");
        return {};
    }

    const QString path = QStringLiteral("/proc/self/fd/%1").arg(fd);

    s_cachedDescriptorsByUri.insert(uriString, parcelFileDescriptor);
    s_cachedPathsByUri.insert(uriString, path);
    s_cacheInsertionOrder.append(uriString);
    evictOldestIfNeeded();

    return path;
}

} // namespace android
} // namespace mixxx

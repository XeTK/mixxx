#include "util/android/contenturiresolver.h"

#include <android/log.h>
#include <unistd.h>

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

// Bounds how many ParcelFileDescriptors are kept open at once. A full
// library scan resolves every track in sequence, so without a cap this
// would leak one fd per track and exhaust the process' descriptor limit
// well before a modest-sized library finished scanning.
constexpr int kMaxCachedDescriptors = 32;

QMutex s_cacheMutex;
QHash<QString, QJniObject> s_cachedDescriptorsByUri;
QHash<QString, int> s_cachedFdsByUri;
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
        s_cachedFdsByUri.remove(oldestKey);
    }
}

} // namespace

int resolveContentUriToSharedReadFd(const QString& uriString) {
    QMutexLocker locker(&s_cacheMutex);
    const auto cachedFdIt = s_cachedFdsByUri.constFind(uriString);
    if (cachedFdIt != s_cachedFdsByUri.constEnd()) {
        return cachedFdIt.value();
    }

    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        __android_log_print(ANDROID_LOG_WARN,
                "mixxx",
                "resolveContentUriToSharedReadFd: no Android context");
        return -1;
    }

    QJniObject contentResolver = context.callObjectMethod(
            "getContentResolver", "()Landroid/content/ContentResolver;");
    if (!contentResolver.isValid()) {
        return -1;
    }

    QJniObject jUriString = QJniObject::fromString(uriString);
    QJniObject uri = QJniObject::callStaticMethod<jobject>("android/net/Uri",
            "parse",
            "(Ljava/lang/String;)Landroid/net/Uri;",
            jUriString.object<jstring>());
    if (!uri.isValid()) {
        return -1;
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
                "resolveContentUriToSharedReadFd: openFileDescriptor threw for %s",
                uriString.toUtf8().constData());
        return -1;
    }
    if (!parcelFileDescriptor.isValid()) {
        __android_log_print(ANDROID_LOG_WARN,
                "mixxx",
                "resolveContentUriToSharedReadFd: openFileDescriptor returned null for %s",
                uriString.toUtf8().constData());
        return -1;
    }

    const jint fd = parcelFileDescriptor.callMethod<jint>("getFd");
    if (fd < 0) {
        parcelFileDescriptor.callMethod<void>("close");
        return -1;
    }

    s_cachedDescriptorsByUri.insert(uriString, parcelFileDescriptor);
    s_cachedFdsByUri.insert(uriString, fd);
    s_cacheInsertionOrder.append(uriString);
    evictOldestIfNeeded();

    return fd;
}

bool openContentUriAsQFile(const QString& contentUri, QFile* pFile) {
    const int sharedFd = resolveContentUriToSharedReadFd(contentUri);
    if (sharedFd < 0) {
        return false;
    }
    const int duplicatedFd = ::dup(sharedFd);
    if (duplicatedFd < 0) {
        __android_log_print(ANDROID_LOG_WARN,
                "mixxx",
                "openContentUriAsQFile: dup() failed for %s",
                contentUri.toUtf8().constData());
        return false;
    }
    if (!pFile->open(duplicatedFd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(duplicatedFd);
        return false;
    }
    return true;
}

} // namespace android
} // namespace mixxx

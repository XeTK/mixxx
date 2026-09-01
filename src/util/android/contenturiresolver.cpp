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

// Bounds how many ParcelFileDescriptors are kept in the shared-fd cache
// at once. A full library scan resolves every track in sequence, so
// without a cap this would leak one fd per track and exhaust the
// process' descriptor limit well before a modest-sized library finished
// scanning.
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

// Shared JNI plumbing for both resolveContentUriToSharedReadFd() and
// openContentUriAsQFile(): asks Android's ContentResolver to open the
// given content:// URI, returning a Java ParcelFileDescriptor (invalid
// on failure, with a warning already logged).
QJniObject openParcelFileDescriptor(const QString& uriString) {
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        __android_log_print(ANDROID_LOG_WARN,
                "mixxx",
                "openParcelFileDescriptor: no Android context");
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
                "openParcelFileDescriptor: openFileDescriptor threw for %s",
                uriString.toUtf8().constData());
        return {};
    }
    if (!parcelFileDescriptor.isValid()) {
        __android_log_print(ANDROID_LOG_WARN,
                "mixxx",
                "openParcelFileDescriptor: openFileDescriptor returned null for %s",
                uriString.toUtf8().constData());
        return {};
    }
    return parcelFileDescriptor;
}

} // namespace

int resolveContentUriToSharedReadFd(const QString& uriString) {
    QMutexLocker locker(&s_cacheMutex);
    const auto cachedFdIt = s_cachedFdsByUri.constFind(uriString);
    if (cachedFdIt != s_cachedFdsByUri.constEnd()) {
        return cachedFdIt.value();
    }

    QJniObject parcelFileDescriptor = openParcelFileDescriptor(uriString);
    if (!parcelFileDescriptor.isValid()) {
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
    // Deliberately does NOT go through the shared-fd cache/dup() a
    // shared descriptor: dup() creates a new file descriptor *number*,
    // but it does not give that number its own file position - it still
    // shares the same underlying open file description (and therefore
    // the same lseek/read cursor) as the descriptor it was dup'd from.
    // QFile reads via a plain, stateful lseek()+read(), so two QFiles
    // sharing a dup() lineage of the same URI - e.g. a track loaded to
    // a deck while the analyzer thread scans the same track in the
    // background, which genuinely happens - corrupt each other's reads
    // (confirmed concretely: the analyzer failed with FLAC "LOST_SYNC"
    // decode errors while a deck was simultaneously playing the same
    // track through a dup()'d descriptor of the same fd). Each
    // QFile-based consumer therefore gets a fully independent
    // ContentResolver-issued descriptor instead.
    QJniObject parcelFileDescriptor = openParcelFileDescriptor(contentUri);
    if (!parcelFileDescriptor.isValid()) {
        return false;
    }

    // detachFd() transfers ownership of the underlying fd out of the
    // Java ParcelFileDescriptor (which would otherwise close it - via
    // its CloseGuard finalizer - whenever it gets garbage collected,
    // possibly while our QFile is still actively using the same fd
    // number) to us; QFile(fd, AutoCloseHandle) then becomes the sole
    // owner and closes it exactly once when done.
    const jint fd = parcelFileDescriptor.callMethod<jint>("detachFd");
    if (fd < 0) {
        parcelFileDescriptor.callMethod<void>("close");
        return false;
    }

    if (!pFile->open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(fd);
        return false;
    }
    return true;
}

} // namespace android
} // namespace mixxx

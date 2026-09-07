#include "qml/qmllibraryproxy.h"

#include <QAbstractItemModel>
#include <QFileDialog>
#include <QStandardPaths>

#include "library/library.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "moc_qmllibraryproxy.cpp"

namespace mixxx {
namespace qml {

QmlLibraryProxy::QmlLibraryProxy(std::shared_ptr<Library> pLibrary, QObject* parent)
        : QObject(parent),
          m_pLibrary(pLibrary),
          m_pModelProperty(new QmlLibraryTrackListModel(m_pLibrary->trackTableModel(), this)) {
}

// static
QmlLibraryProxy* QmlLibraryProxy::create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine) {
    // The implementation of this method is mostly taken from the code example
    // that shows the replacement for `qmlRegisterSingletonInstance()` when
    // using `QML_SINGLETON`.
    // https://doc.qt.io/qt-6/qqmlengine.html#QML_SINGLETON

    // The instance has to exist before it is used. We cannot replace it.
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary) {
        qWarning() << "Library hasn't been registered yet";
        return nullptr;
    }
    return new QmlLibraryProxy(s_pLibrary, pQmlEngine);
}

QStringList QmlLibraryProxy::getDirs() const {
    return m_pLibrary->trackCollectionManager()->internalCollection()->getRootDirStrings();
}

bool QmlLibraryProxy::addDir() {
    const QString dir = QFileDialog::getExistingDirectory(nullptr,
            tr("Choose a music directory"),
            QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
            QFileDialog::ShowDirsOnly);
    if (dir.isEmpty()) {
        // User cancelled the picker.
        return false;
    }
    return m_pLibrary->requestAddDir(dir);
}

bool QmlLibraryProxy::removeDir(const QString& dir) {
    return m_pLibrary->requestRemoveDir(dir, LibraryRemovalType::HideTracks);
}

} // namespace qml
} // namespace mixxx

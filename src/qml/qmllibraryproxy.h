#pragma once
#include <QObject>
#include <QQmlEngine>
#include <memory>

#include "qml/qmllibrarytracklistmodel.h"
#include "util/parented_ptr.h"

class Library;

namespace mixxx {
namespace qml {

class QmlLibraryTrackListModel;

class QmlLibraryProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(mixxx::qml::QmlLibraryTrackListModel* model MEMBER m_pModelProperty CONSTANT)
    QML_NAMED_ELEMENT(Library)
    QML_SINGLETON

  public:
    explicit QmlLibraryProxy(std::shared_ptr<Library> pLibrary, QObject* parent = nullptr);

    static QmlLibraryProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);
    static void registerLibrary(std::shared_ptr<Library> pLibrary) {
        s_pLibrary = std::move(pLibrary);
    }

    /// Currently configured library (music) directories, for a mobile
    /// preferences page to list - mirrors DlgPrefLibrary's directory list,
    /// minus the desktop-only relocate flow.
    Q_INVOKABLE QStringList getDirs() const;

    /// Prompts for a new music directory via QFileDialog::getExistingDirectory()
    /// and, if one was picked, adds it to the library. On iOS, Qt's platform
    /// plugin bridges this call to the native UIDocumentPickerViewController
    /// in directory-picking mode, so no separate Objective-C++ picker is
    /// needed - see Sandbox::createSecurityToken(), which Library::requestAddDir()
    /// already calls, for how access to the picked folder survives past this
    /// run via a security-scoped bookmark.
    /// Returns true if a directory was picked and successfully added.
    Q_INVOKABLE bool addDir();

    /// Stops watching `dir`, keeping already-imported tracks (and their
    /// metadata/cues/etc) in the library - the same outcome as DlgPrefLibrary's
    /// "Hide Tracks" removal choice, picked here as the single sane default
    /// for a mobile UI with no room for a three-way confirmation dialog.
    /// Returns true if the directory was removed.
    Q_INVOKABLE bool removeDir(const QString& dir);

    /// Triggers an on-demand scan of all library directories via
    /// TrackCollectionManager::startLibraryScan(). addDir()/removeDir() only
    /// register the directory change with the database - on desktop, the
    /// actual scan happens either at the next startup (CoreServices::initialize()
    /// gates its startLibraryAutoScan() call on, among other things, whether a
    /// music dir was just added) or via a separate explicit "Scan library"
    /// action. Mobile has no equivalent action anywhere else, so without this
    /// a folder added through PrefsLibrary.qml would show empty until the app
    /// was force-quit and relaunched. Progress/result surface via
    /// scanStarted()/scanFinished()/scanSummaryReady().
    Q_INVOKABLE void scanLibrary();

  signals:
    /// Forwarded from TrackCollectionManager::libraryScanStarted().
    void scanStarted();
    /// Forwarded from TrackCollectionManager::libraryScanFinished().
    void scanFinished();
    /// Forwarded from TrackCollectionManager::libraryScanSummary(), flattened
    /// to plain ints for QML - see MixxxMainWindow::slotLibraryScanSummaryDlg()
    /// for the fuller desktop popup this deliberately doesn't try to match.
    void scanSummaryReady(int numNewTracks, int numMissingTracks, int tracksTotal);

  private:
    static inline std::shared_ptr<Library> s_pLibrary;

    std::shared_ptr<Library> m_pLibrary;

    /// This needs to be a plain pointer because it's used as a `Q_PROPERTY` member variable.
    QmlLibraryTrackListModel* m_pModelProperty;
};

} // namespace qml
} // namespace mixxx

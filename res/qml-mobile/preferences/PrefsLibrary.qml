import "../" as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "../Theme"

// Lets a user add/remove library (music) folders from the mobile skin -
// desktop has DlgPrefLibrary's directory list/Add/Remove buttons for this,
// but the mobile Preferences screen had no "Library" category at all until
// now. This matters most on iOS: CoreServices auto-picks the sandboxed
// ~/Documents/Music folder on first run (see coreservices.cpp's
// Q_OS_IOS/Q_OS_WASM branch, untouched here) since there's no sensible
// default outside the sandbox, but that left no way to add anything OUTSIDE
// the sandbox (a Files app location, iCloud Drive, a USB-C drive) without a
// folder picker.
//
// This page is that picker, wired straight to Mixxx.Library's
// getDirs()/addDir()/removeDir() (qml/qmllibraryproxy.h/cpp), which reuse
// the exact same Library::requestAddDir()/requestRemoveDir() C++ path
// desktop's QFileDialog-based DlgPrefLibrary uses - no separate QML proxy
// class was needed since QmlLibraryProxy already existed (exposed as
// Mixxx.Library) for the track list model.
//
// On iOS, Qt's platform plugin bridges QFileDialog::getExistingDirectory()
// itself to the native UIDocumentPickerViewController in directory-picking
// mode (QIOSFileDialog::showNativeDocumentPickerDialog), so addDir() below
// needed no custom Objective-C++ picker. Persistence of access to a
// picked-outside-the-sandbox folder across app restarts is handled by
// Library::requestAddDir()'s existing call to
// Sandbox::createSecurityTokenForDir(), which already has an iOS-specific
// branch (see util/sandbox.cpp) that creates and reloads a security-scoped
// NSURL bookmark - nothing new was needed there either.
//
// addDir()/removeDir() only register the directory change with the
// database - they don't scan it. On desktop that's fine because
// DlgPrefLibrary isn't the only way to trigger a scan (there's a separate
// "Rescan library" action, plus CoreServices runs one at the next startup
// regardless), but mobile has no equivalent, so a freshly-added folder
// would show no tracks until the app was force-quit and relaunched. The
// "Scan Now" button below calls Mixxx.Library.scanLibrary() to trigger one
// on demand instead.
Item {
    id: root

    property var dirs: []
    property string statusMessage: ""
    property color statusColor: Theme.deckTextColor
    property bool scanning: false

    function refreshDirs() {
        root.dirs = Mixxx.Library.getDirs();
    }

    Component.onCompleted: root.refreshDirs()

    Connections {
        target: Mixxx.Library

        function onScanStarted() {
            root.scanning = true;
            root.statusColor = Theme.deckTextColor;
            root.statusMessage = "Scanning library...";
        }

        function onScanFinished() {
            root.scanning = false;
        }

        function onScanSummaryReady(numNewTracks, numMissingTracks, tracksTotal) {
            root.statusColor = Theme.deckTextColor;
            if (numNewTracks === 0 && numMissingTracks === 0) {
                root.statusMessage = "Scan finished - no changes, " +
                        tracksTotal + " tracks total.";
            } else {
                root.statusMessage = "Scan finished: " + numNewTracks +
                        " new, " + numMissingTracks + " missing, " +
                        tracksTotal + " tracks total.";
            }
        }
    }

    Column {
        id: headerColumn

        width: parent.width
        spacing: 10

        Skin.SectionText {
            width: parent.width
            text: "Music Folders"
        }

        Skin.Button {
            width: parent.width
            height: 40
            activeColor: Theme.blue
            enabled: !root.scanning
            text: "Add Folder..."
            onClicked: {
                root.statusMessage = "";
                if (Mixxx.Library.addDir()) {
                    root.refreshDirs();
                } else {
                    root.statusColor = Theme.red;
                    root.statusMessage = "Folder wasn't added - it may already " +
                            "be in your library, or the picker was cancelled.";
                }
            }
        }

        Skin.Button {
            width: parent.width
            height: 40
            activeColor: Theme.blue
            enabled: !root.scanning && root.dirs.length > 0
            text: root.scanning ? "Scanning..." : "Scan Now"
            onClicked: {
                root.statusMessage = "";
                Mixxx.Library.scanLibrary();
            }
        }

        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            visible: root.statusMessage.length > 0
            text: root.statusMessage
            color: root.statusColor
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize
        }
    }

    Text {
        anchors.top: headerColumn.bottom
        anchors.topMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
        visible: root.dirs.length === 0
        text: "No library folders yet"
        color: Theme.deckTextColor
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textFontPixelSize
    }

    ListView {
        id: listView

        anchors.top: headerColumn.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        spacing: 6
        model: root.dirs

        delegate: Item {
            id: itemDlgt

            required property string modelData

            implicitWidth: listView.width
            implicitHeight: 56

            Skin.EmbeddedBackground {
                anchors.fill: parent
            }

            Text {
                anchors.left: parent.left
                anchors.right: removeButton.left
                anchors.leftMargin: 12
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideMiddle
                text: itemDlgt.modelData
                color: Theme.deckTextColor
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textFontPixelSize
            }

            Skin.Button {
                id: removeButton

                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 90
                height: 40
                activeColor: Theme.red
                text: "Remove"
                onClicked: {
                    Mixxx.Library.removeDir(itemDlgt.modelData);
                    root.refreshDirs();
                }
            }
        }
    }
}

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.12
import "Theme"

ApplicationWindow {
    id: root

    // "decks" | "library" | "effects" | "sampler" - each is a genuinely
    // separate full-screen view on mobile rather than an inline section
    // competing with the others for scroll space. The hamburger button
    // doubles as a back button whenever we're not on "decks".
    property string currentScreen: "decks"
    // Hides per-deck knobs/hotcues/rate slider/mixer, leaving just the big
    // waveform + basic transport - meant for when a physical controller is
    // connected and the phone screen is better used as a waveform display
    // than a duplicate set of on-screen controls.
    property alias compactControls: compactControlsButton.checked

    // On Android and iOS the window must track the (rotating) screen; the
    // fixed size is only a sane default for the desktop preview. Without
    // this, an app launched from a portrait home screen keeps its
    // portrait-sized GL window after the forced rotation to landscape (the
    // manifest/OS handles configChanges itself, so nothing recreates the
    // activity and Qt misses the resize), leaving the UI squashed into the
    // left third of the screen.
    //
    // On iOS specifically, leaving this on the 1920x1080 desktop-preview
    // default (aspect ratio 16:9) while the real device viewport is a
    // different aspect ratio (e.g. an iPad mini's ~1133x744 points, ~1.52:1)
    // means the QML scene graph is laid out at a size that doesn't match
    // the actual window content area. Visually the content still appears
    // to fill the screen, but item geometry (including every MouseArea's
    // hit-test bounds) is computed against the wrong 1920x1080 canvas, so
    // taps land on the wrong logical coordinates - most visibly inside
    // ListView delegates (Library track rows, Preferences category rows)
    // where the accumulated vertical offset from the top of a tall list
    // makes the drift large enough that clicks silently miss their target.
    readonly property bool isMobilePlatform: Qt.platform.os === "android" || Qt.platform.os === "ios"
    width: root.isMobilePlatform ? Screen.width : 1920
    height: root.isMobilePlatform ? Screen.height : 1080
    color: Theme.backgroundColor
    visible: true

    // Reconnect to whatever BLE MIDI controller was last used, if any,
    // instead of making the DJ reopen Controllers prefs and hit Connect
    // again every launch. No-ops quietly if there's nothing persisted yet
    // or Bluetooth permission isn't already granted.
    Component.onCompleted: Mixxx.ControllerManager.reconnectLastBluetoothMidiDevice()

    // A small persistent hamburger button replaces the old full-width
    // toolbar row - it doesn't eat a fixed strip of vertical space the way
    // a toolbar (even a collapsible one) does, and phones have plenty of
    // width but little height to spare in landscape. On any screen other
    // than "decks" it becomes a back button instead of opening the drawer.
    Skin.Button {
        id: menuButton

        z: 10
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 4
        // Big enough to hit with a thumb.
        width: 48
        height: 40
        // "<" is plain ASCII; the ☰/← glyphs render as tofu boxes on
        // Android (the bundled fonts don't cover them), so the hamburger
        // is drawn with rectangles below instead of a glyph.
        text: root.currentScreen === "decks" ? "" : "<"
        activeColor: Theme.white
        onClicked: {
            if (root.currentScreen === "decks")
                menuDrawer.open();
            else
                root.currentScreen = "decks";
        }

        Column {
            visible: root.currentScreen === "decks"
            anchors.centerIn: parent
            spacing: 4

            Repeater {
                model: 3

                Rectangle {
                    width: 18
                    height: 2
                    radius: 1
                    color: Theme.buttonNormalColor
                }
            }
        }
    }

    Popup {
        id: menuDrawer

        x: (root.width - width) / 2
        y: (root.height - height) / 2
        dim: true
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        contentWidth: menuColumn.implicitWidth
        contentHeight: menuColumn.implicitHeight

        background: BorderImage {
            anchors.fill: parent
            horizontalTileMode: BorderImage.Stretch
            verticalTileMode: BorderImage.Stretch
            source: Theme.imgPopupBackground

            border {
                top: 10
                left: 20
                right: 20
                bottom: 10
            }
        }

        Column {
            id: menuColumn

            spacing: 4

            Skin.Button {
                id: compactControlsButton

                width: 160
                text: "Compact"
                activeColor: Theme.white
                checkable: true
            }

            Skin.Button {
                width: 160
                text: "Library"
                activeColor: Theme.white
                onClicked: {
                    root.currentScreen = "library";
                    menuDrawer.close();
                }
            }

            Skin.Button {
                width: 160
                text: "Effects"
                activeColor: Theme.white
                onClicked: {
                    root.currentScreen = "effects";
                    menuDrawer.close();
                }
            }

            Skin.Button {
                width: 160
                text: "Sampler"
                activeColor: Theme.white
                onClicked: {
                    root.currentScreen = "sampler";
                    menuDrawer.close();
                }
            }

            Skin.Button {
                width: 160
                text: "Prefs"
                activeColor: Theme.white
                onClicked: {
                    root.currentScreen = "preferences";
                    menuDrawer.close();
                }
            }

            Skin.Button {
                id: showDevToolsButton

                width: 160
                text: "Develop"
                activeColor: Theme.white
                checkable: true
                checked: devToolsWindow.visible
                onClicked: {
                    if (devToolsWindow.visible)
                        devToolsWindow.close();
                    else
                        devToolsWindow.show();

                    menuDrawer.close();
                }

                DeveloperToolsWindow {
                    id: devToolsWindow

                    width: 640
                    height: 480
                }
            }
        }
    }

    // Turning a controller's browse encoder (e.g. the DDJ-FLX2's Shift+Jog)
    // moves the library selection even while it's off-screen behind the
    // decks/effects/sampler view - LibraryControl.qml's own MoveUp/MoveDown/
    // MoveVertical bindings still fire - but the DJ can't see what's being
    // selected. Jump to the library screen as soon as that happens so the
    // selection is actually visible to load from. All three controls are
    // watched since different controller mappings use either MoveVertical
    // (a signed delta) or the separate MoveUp/MoveDown pair.
    function showLibraryOnBrowse(value) {
        if (value != 0)
            root.currentScreen = "library";
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveVertical"
        onValueChanged: (value) => root.showLibraryOnBrowse(value)
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveUp"
        onValueChanged: (value) => root.showLibraryOnBrowse(value)
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveDown"
        onValueChanged: (value) => root.showLibraryOnBrowse(value)
    }

    Item {
        id: decksScreen

        anchors.fill: parent
        visible: root.currentScreen === "decks"

        // Everything scales to the window instead of scrolling: each
        // waveform takes a share of the height and the deck controls fill
        // whatever remains (the mixer scales itself down if that's tighter
        // than its natural size - see Mixer.qml). In compact mode the
        // controls collapse and the waveforms split the whole screen.
        readonly property real waveformHeight: root.compactControls ? height / 2 : Math.max(70, height * 0.22)

        // No reserved header row - the hamburger/back button floats
        // (z: 10, see above) directly over the top corner of the
        // first waveform instead of pushing content down.
        Item {
            id: deck1waveformWrap

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: decksScreen.waveformHeight

            Behavior on height {
                NumberAnimation {
                    duration: 150
                }
            }

            Skin.WaveformDisplay {
                id: deck1waveform

                anchors.fill: parent
                group: "[Channel1]"
            }

            Skin.TrackInfoBubble {
                // Sits at the bottom of channel A's waveform rather
                // than the top, since the top-left corner is where beat
                // markers/cue flags tend to cluster and the hamburger
                // button already lives there too.
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.margins: 4
                group: "[Channel1]"
            }
        }

        Item {
            id: deck2waveformWrap

            anchors.top: deck1waveformWrap.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: decksScreen.waveformHeight

            Behavior on height {
                NumberAnimation {
                    duration: 150
                }
            }

            Skin.WaveformDisplay {
                id: deck2waveform

                anchors.fill: parent
                group: "[Channel2]"
            }

            Skin.TrackInfoBubble {
                // Bottom instead of top now too - the hamburger/back
                // button moved to the top-right corner, which would
                // otherwise sit right on top of this.
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.margins: 4
                group: "[Channel2]"
            }
        }

        Skin.DeckRow {
            id: decks12

            anchors.top: deck2waveformWrap.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            leftDeckGroup: "[Channel1]"
            rightDeckGroup: "[Channel2]"
            minimized: root.compactControls
        }
    }

    Item {
        id: libraryScreen

        anchors.fill: parent
        visible: root.currentScreen === "library"

        Skin.Library {
            anchors.fill: parent
            onTrackLoadedToDeck: {
                root.currentScreen = "decks";
            }
        }
    }

    Item {
        id: effectsScreen

        anchors.fill: parent
        visible: root.currentScreen === "effects"

        Skin.EffectRow {
            anchors.fill: parent
        }
    }

    Item {
        id: samplerScreen

        anchors.fill: parent
        visible: root.currentScreen === "sampler"

        Skin.SamplerRow {
            anchors.fill: parent
        }
    }

    Item {
        id: preferencesScreen

        anchors.fill: parent
        visible: root.currentScreen === "preferences"

        Skin.Preferences {
            anchors.fill: parent
        }
    }
}

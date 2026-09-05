import Mixxx 1.0 as Mixxx
import Qt5Compat.GraphicalEffects
import QtQuick 2.12
import "Theme"

Item {
    id: root

    // Loading a track (by any method - double-click, Enter, or the
    // long-press "load to deck" popup) should return to the decks screen
    // automatically, since there's no reason to keep looking at the library
    // once you've picked something.
    signal trackLoadedToDeck

    Rectangle {
        color: Theme.deckBackgroundColor
        anchors.fill: parent

        LibraryControl {
            id: libraryControl

            onMoveVertical: (offset) => {
                listView.moveSelectionVertical(offset);
            }
            onLoadSelectedTrack: (group, play) => {
                listView.loadSelectedTrack(group, play);
            }
            onLoadSelectedTrackIntoNextAvailableDeck: (play) => {
                listView.loadSelectedTrackIntoNextAvailableDeck(play);
            }
            onFocusWidgetChanged: {
                switch (focusWidget) {
                    case FocusedWidgetControl.WidgetKind.LibraryView:
                        listView.forceActiveFocus();
                        break;
                }
            }
        }

        ListView {
            id: listView

            function moveSelectionVertical(value) {
                if (value == 0)
                    return ;

                const rowCount = model.rowCount();
                if (rowCount == 0)
                    return ;

                currentIndex = Mixxx.MathUtils.positiveModulo(currentIndex + value, rowCount);
            }

            function loadSelectedTrackIntoNextAvailableDeck(play) {
                const url = model.get(currentIndex).fileUrl;
                if (!url)
                    return ;

                Mixxx.PlayerManager.loadLocationUrlIntoNextAvailableDeck(url, play);
                root.trackLoadedToDeck();
            }

            function loadSelectedTrack(group, play) {
                const url = model.get(currentIndex).fileUrl;
                if (!url)
                    return ;

                const player = Mixxx.PlayerManager.getPlayer(group);
                if (!player)
                    return ;

                player.loadTrackFromLocationUrl(url, play);
                root.trackLoadedToDeck();
            }

            anchors.fill: parent
            anchors.margins: 10
            clip: true
            keyNavigationWraps: true
            highlightMoveDuration: 250
            highlightResizeDuration: 50
            model: Mixxx.Library.model
            Keys.onPressed: (event) => {
                switch (event.key) {
                    case Qt.Key_Enter:
                        case Qt.Key_Return:
                            listView.loadSelectedTrackIntoNextAvailableDeck(false);
                        break;
                }
            }

            delegate: Item {
                id: itemDlgt

                required property int index
                required property url fileUrl
                required property string artist
                required property string title
                required property string album
                required property string duration
                required property string bpm
                required property string key
                required property string genre
                required property url coverArtUrl

                implicitWidth: listView.width
                implicitHeight: 60

                // Cover art thumbnail, like the desktop library's Cover Art
                // column - the same generic "no cover art" icon the desktop
                // skins use (not just a flat color swatch) when there's no
                // embedded/found artwork, so rows stay visually
                // distinguishable while browsing a track list.
                Rectangle {
                    id: coverArtFrame

                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    width: 48
                    height: 48
                    radius: 4
                    color: Theme.knobBackgroundColor
                }

                Image {
                    id: coverArtPlaceholder

                    anchors.fill: coverArtFrame
                    anchors.margins: 6
                    source: "qrc:/images/library/cover_default.svg"
                    visible: itemDlgt.coverArtUrl.toString().length === 0
                    asynchronous: true
                    fillMode: Image.PreserveAspectFit
                }

                Image {
                    id: coverArt

                    anchors.fill: coverArtFrame
                    source: itemDlgt.coverArtUrl
                    visible: false
                    asynchronous: true
                    fillMode: Image.PreserveAspectCrop
                }

                Rectangle {
                    id: coverArtMask

                    anchors.fill: coverArtFrame
                    radius: coverArtFrame.radius
                    visible: false
                }

                OpacityMask {
                    anchors.fill: coverArtFrame
                    source: coverArt
                    maskSource: coverArtMask
                    visible: itemDlgt.coverArtUrl.toString().length > 0
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: coverArtFrame.right
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    spacing: 2

                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        font.bold: true
                        text: itemDlgt.title
                        color: (listView.currentIndex == itemDlgt.index && listView.activeFocus) ? Theme.blue : Theme.deckTextColor

                        Behavior on color {
                            ColorAnimation {
                                duration: listView.highlightMoveDuration
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        color: Theme.deckTextColor
                        text: [itemDlgt.artist, itemDlgt.album].filter((s) => s.length > 0).join("  ·  ")
                    }

                    // Auxiliary metadata that doesn't fit on the lines above -
                    // useful for picking a track by feel (matching key/bpm)
                    // rather than opening it first to find out.
                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        opacity: 0.7
                        color: Theme.deckTextColor
                        // The bpm role renders unanalysed tracks as "-",
                        // which would otherwise show up as a stray "- BPM".
                        text: [itemDlgt.genre, itemDlgt.key, itemDlgt.bpm && itemDlgt.bpm !== "-" ? itemDlgt.bpm + " BPM" : "", itemDlgt.duration].filter((s) => s.length > 0).join("  ·  ")
                    }
                }

                Image {
                    id: dragItem

                    Drag.active: dragArea.drag.active
                    Drag.dragType: Drag.Automatic
                    Drag.supportedActions: Qt.CopyAction
                    Drag.mimeData: {
                        "text/uri-list": itemDlgt.fileUrl,
                        "text/plain": itemDlgt.fileUrl
                    }
                    anchors.fill: parent
                }

                MouseArea {
                    id: dragArea

                    anchors.fill: parent
                    drag.target: dragItem
                    onPressed: {
                        listView.forceActiveFocus();
                        listView.currentIndex = itemDlgt.index;
                        parent.grabToImage((result) => {
                                dragItem.Drag.imageSource = result.url;
                        });
                    }
                    onDoubleClicked: listView.loadSelectedTrackIntoNextAvailableDeck(false)
                    onPressAndHold: {
                        listView.forceActiveFocus();
                        listView.currentIndex = itemDlgt.index;
                        loadToDeckPopup.x = dragArea.mouseX;
                        loadToDeckPopup.y = dragArea.mouseY;
                        loadToDeckPopup.open();
                    }
                }

                LoadToDeckPopup {
                    id: loadToDeckPopup

                    onLoadToDeck: (group) => {
                        listView.loadSelectedTrack(group, false);
                    }
                }
            }

            highlight: Rectangle {
                border.color: listView.activeFocus ? Theme.blue : Theme.deckTextColor
                border.width: 1
                color: "transparent"
            }
        }
    }
}

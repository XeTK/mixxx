import Mixxx 1.0 as Mixxx
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
                required property string duration
                required property string bpm
                required property string key
                required property string genre

                implicitWidth: listView.width
                implicitHeight: 44

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    spacing: 2

                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        text: itemDlgt.artist + " - " + itemDlgt.title
                        color: (listView.currentIndex == itemDlgt.index && listView.activeFocus) ? Theme.blue : Theme.deckTextColor

                        Behavior on color {
                            ColorAnimation {
                                duration: listView.highlightMoveDuration
                            }
                        }
                    }

                    // Auxiliary metadata that doesn't fit on the title line -
                    // useful for picking a track by feel (matching key/bpm)
                    // rather than opening it first to find out.
                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        opacity: 0.7
                        color: Theme.deckTextColor
                        text: [itemDlgt.genre, itemDlgt.key, itemDlgt.bpm ? itemDlgt.bpm + " BPM" : "", itemDlgt.duration].filter((s) => s.length > 0).join("  ·  ")
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

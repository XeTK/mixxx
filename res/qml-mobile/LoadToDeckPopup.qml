import "." as Skin
import QtQuick 2.12
import QtQuick.Controls 2.12
import "Theme"

Popup {
    id: root

    signal loadToDeck(string group)

    dim: true
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    contentWidth: column.implicitWidth
    contentHeight: column.implicitHeight

    Column {
        id: column

        spacing: 4

        Skin.Button {
            width: 200
            height: 48
            text: "Load to Deck 1"
            activeColor: Theme.deckActiveColor
            highlight: true
            onClicked: {
                root.loadToDeck("[Channel1]");
                root.close();
            }
        }

        Skin.Button {
            width: 200
            height: 48
            text: "Load to Deck 2"
            activeColor: Theme.deckActiveColor
            highlight: true
            onClicked: {
                root.loadToDeck("[Channel2]");
                root.close();
            }
        }
    }

    enter: Transition {
        NumberAnimation {
            properties: "opacity"
            from: 0
            to: 1
            duration: 100
        }
    }

    exit: Transition {
        NumberAnimation {
            properties: "opacity"
            from: 1
            to: 0
            duration: 100
        }
    }

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
}

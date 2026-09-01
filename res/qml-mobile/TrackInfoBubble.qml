import "." as Skin
import Mixxx 1.0 as Mixxx
import Qt5Compat.GraphicalEffects
import QtQuick 2.12
import "Theme"

// Floats over a deck's waveform instead of taking its own row (see
// main.qml) - small rounded cover art + title/key/bpm chip, dark
// translucent background so it stays legible over the waveform without
// blocking much of it.
Item {
    id: root

    required property string group
    property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)

    // Nothing useful to show (and an empty pill just looks like a stray
    // dark blob on the waveform) until a track is actually loaded.
    visible: deckPlayer.isLoaded

    implicitWidth: row.implicitWidth + 16
    implicitHeight: 36

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: "#a0000000"
    }

    Row {
        id: row

        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 6
        spacing: 6

        Image {
            id: coverArt

            anchors.verticalCenter: parent.verticalCenter
            width: 26
            height: 26
            source: root.deckPlayer.coverArtUrl
            visible: false
            asynchronous: true
        }

        Rectangle {
            id: coverArtCircle

            width: coverArt.width
            height: coverArt.height
            radius: height / 2
            visible: false
        }

        OpacityMask {
            width: coverArt.width
            height: coverArt.height
            anchors.verticalCenter: parent.verticalCenter
            source: coverArt
            maskSource: coverArtCircle
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0

            Skin.EmbeddedText {
                text: root.deckPlayer.title
                font.bold: false
                font.pixelSize: Theme.textFontPixelSize
            }

            Row {
                spacing: 6

                Skin.EmbeddedText {
                    text: root.deckPlayer.keyText
                    font.pixelSize: Theme.textFontPixelSize - 1
                }

                Skin.EmbeddedText {
                    text: bpmControl.value > 0 ? bpmControl.value.toFixed(1) : ""
                    font.pixelSize: Theme.textFontPixelSize - 1

                    Mixxx.ControlProxy {
                        id: bpmControl

                        group: root.group
                        key: "bpm"
                    }
                }
            }
        }
    }
}

import QtQuick 2.12

Item {
    id: root

    required property string leftDeckGroup
    required property string rightDeckGroup
    property alias mixer: mixer
    property bool minimized: false

    // No implicitHeight here - main.qml anchors this row between the
    // waveforms and the bottom of the window, so it fills whatever real
    // estate is left and the decks/mixer scale themselves to fit it.
    states: [
        State {
            when: root.minimized
            name: "minimized"

            PropertyChanges {
                target: mixer
                visible: false
            }

            AnchorChanges {
                target: leftDeck
                anchors.right: mixer.horizontalCenter
            }

            AnchorChanges {
                target: rightDeck
                anchors.left: mixer.horizontalCenter
            }

        },
        State {
            // This State can't be deduplicated by making the first one
            // reversible, because for decks 3/4 the mixer may already be
            // hidden (since the whole deck row is already hidden). In that
            // case, disabling the minimized state would not show the mixer
            // again.
            when: !root.minimized
            name: "maximized"

            PropertyChanges {
                target: mixer
                visible: true
            }

            AnchorChanges {
                target: leftDeck
                anchors.right: mixer.left
            }

            AnchorChanges {
                target: rightDeck
                anchors.left: mixer.right
            }
        }
    ]

    Deck {
        id: leftDeck

        minimized: root.minimized
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        group: root.leftDeckGroup
    }

    Mixer {
        id: mixer

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        leftDeckGroup: root.leftDeckGroup
        rightDeckGroup: root.rightDeckGroup

        FadeBehavior on visible {
            fadeTarget: mixer
        }
    }

    Deck {
        id: rightDeck

        minimized: root.minimized
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        group: root.rightDeckGroup
    }

    transitions: Transition {
        to: "minimized"
        reversible: true

        AnchorAnimation {
            targets: [leftDeck, rightDeck]
            duration: 150
        }
    }
}

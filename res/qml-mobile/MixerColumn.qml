import "." as Skin
import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property string group

    // The gain knob that used to sit on top of the fader now lives in
    // EqColumn's side column, so the volume fader gets this full height.
    Item {
        anchors.top: parent.top
        anchors.topMargin: 5
        anchors.bottomMargin: 5
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: pflButton.top

        Skin.VuMeter {
            x: 15
            y: (parent.height - height) / 2
            width: 4
            height: parent.height - 40
            group: root.group
            key: "vu_meter_left"
        }

        Skin.VuMeter {
            x: parent.width - width - 15
            y: (parent.height - height) / 2
            width: 4
            height: parent.height - 40
            group: root.group
            key: "vu_meter_right"
        }

        Skin.ControlSlider {
            id: volumeSlider

            anchors.fill: parent
            group: root.group
            key: "volume"
            barColor: Theme.volumeSliderBarColor
            bg: Theme.imgVolumeSliderBackground
        }
    }

    Skin.ControlButton {
        id: pflButton

        group: root.group
        key: "pfl"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        text: "PFL"
        activeColor: Theme.pflActiveButtonColor
        toggleable: true
    }
}

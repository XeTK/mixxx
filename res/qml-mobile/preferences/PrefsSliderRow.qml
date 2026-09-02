import "../" as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.12
import "../Theme"

// A single numeric preference row. Two backing modes:
// - useControl: false (default) - a plain ConfigObject key/value setting,
//   read once via Mixxx.Config.getValue and written on change.
// - useControl: true - a live ControlObject (e.g. [Tts],duckStrength,
//   read by the audio thread), bound two-way via Mixxx.ControlProxy.
Item {
    id: root

    required property string label
    property string group
    property string key
    property bool useControl: false
    property real from: 0
    property real to: 1
    property real stepSize: 0.01
    property real defaultValue: 0
    property var valueFormatter: (v) => v.toFixed(2)

    property real value: root.useControl ? controlProxy.value : Mixxx.Config.getValue(root.group, root.key, root.defaultValue)

    implicitHeight: 56

    Mixxx.ControlProxy {
        id: controlProxy

        group: root.useControl ? root.group : ""
        key: root.useControl ? root.key : ""
    }

    Text {
        id: labelText

        anchors.left: parent.left
        anchors.top: parent.top
        elide: Text.ElideRight
        width: parent.width - valueText.width - 8
        text: root.label
        color: Theme.deckTextColor
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textFontPixelSize
    }

    Text {
        id: valueText

        anchors.right: parent.right
        anchors.top: parent.top
        text: root.valueFormatter(root.value)
        color: Theme.deckTextColor
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textFontPixelSize
    }

    Slider {
        id: slider

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 32
        orientation: Qt.Horizontal
        from: root.from
        to: root.to
        stepSize: root.stepSize
        value: root.value

        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth
            height: 4
            radius: 2
            color: Theme.deckLineColor

            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                radius: 2
                color: Theme.blue
            }
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 24
            height: 24
            radius: 12
            color: Theme.white
        }

        onMoved: {
            if (root.useControl) {
                controlProxy.value = slider.value;
            } else {
                root.value = slider.value;
                Mixxx.Config.setValue(root.group, root.key, slider.value);
            }
        }
    }
}

import "../" as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "../Theme"

// A single bool preference row, backed by a plain ConfigObject key/value
// setting (via the generic Mixxx.Config.getValue/setValue bridge).
Item {
    id: root

    required property string label
    required property string group
    required property string key
    property bool defaultValue: false

    // ConfigObject has no change notification, so this is read once on
    // load rather than kept live - see qmlconfigproxy.h.
    property bool value: Mixxx.Config.getValue(root.group, root.key, root.defaultValue)

    implicitHeight: 48

    function toggle() {
        root.value = !root.value;
        Mixxx.Config.setValue(root.group, root.key, root.value);
    }

    Text {
        anchors.left: parent.left
        anchors.right: toggleButton.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        elide: Text.ElideRight
        text: root.label
        color: Theme.deckTextColor
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textFontPixelSize
    }

    Skin.Button {
        id: toggleButton

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 64
        height: 40
        activeColor: Theme.blue
        highlight: root.value
        text: root.value ? "On" : "Off"
        onClicked: root.toggle()
    }
}

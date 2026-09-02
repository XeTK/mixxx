import "../" as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "../Theme"

// A single enum/choice preference row, backed by a plain ConfigObject
// key/value setting. `options` is an array of {text, value} objects -
// `value` is what's actually stored/compared against the config (which may
// not match the option's index, e.g. MultiSamplingMode's 0/2/4/8/16).
Item {
    id: root

    required property string label
    required property string group
    required property string key
    required property var options
    property var defaultValue

    property var currentValue: Mixxx.Config.getValue(root.group, root.key, root.defaultValue)

    implicitHeight: 48

    function indexForValue(v) {
        for (let i = 0; i < root.options.length; i++) {
            if (root.options[i].value === v) {
                return i;
            }
        }
        return 0;
    }

    Text {
        id: labelText

        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width * 0.45
        elide: Text.ElideRight
        text: root.label
        color: Theme.deckTextColor
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textFontPixelSize
    }

    Skin.ComboBox {
        id: combo

        anchors.left: labelText.right
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        height: 40
        textRole: "text"
        model: root.options
        currentIndex: root.indexForValue(root.currentValue)
        onActivated: (index) => {
            root.currentValue = root.options[index].value;
            Mixxx.Config.setValue(root.group, root.key, root.currentValue);
        }
    }
}

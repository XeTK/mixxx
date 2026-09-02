import "../" as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "../Theme"

// Mirrors the basic parts of DlgPrefController/DlgPrefControllers
// (src/controllers/dlgprefcontroller[s].cpp): list detected controllers,
// toggle each on/off, pick a mapping preset. Deep per-control mapping
// editing / MIDI-learn stays out of scope, same as on desktop that's a
// dedicated dialog, not something this pass replicates.
Item {
    id: root

    Text {
        anchors.centerIn: parent
        visible: listView.count === 0
        text: "No controllers detected"
        color: Theme.deckTextColor
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textFontPixelSize
    }

    ListView {
        id: listView

        anchors.fill: parent
        anchors.margins: 10
        clip: true
        spacing: 10
        model: Mixxx.ControllerManager.controllers

        delegate: Item {
            id: itemDlgt

            required property int index
            required property string display
            required property bool isOpen
            property var mappingOptions: []

            implicitWidth: listView.width
            implicitHeight: 96

            function loadMappings() {
                const mappings = Mixxx.ControllerManager.getMappingsForController(itemDlgt.index);
                itemDlgt.mappingOptions = [{ text: "No mapping", path: "" }].concat(
                        mappings.map((m) => ({ text: m.name, path: m.path })));

                const currentPath = Mixxx.ControllerManager.getCurrentMappingPath(itemDlgt.index);
                let selectedIndex = 0;
                for (let i = 0; i < itemDlgt.mappingOptions.length; i++) {
                    if (itemDlgt.mappingOptions[i].path === currentPath) {
                        selectedIndex = i;
                        break;
                    }
                }
                mappingCombo.currentIndex = selectedIndex;
            }

            Component.onCompleted: itemDlgt.loadMappings()

            Skin.EmbeddedBackground {
                anchors.fill: parent
            }

            Column {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                Item {
                    width: parent.width
                    height: 32

                    Text {
                        anchors.left: parent.left
                        anchors.right: enabledButton.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideRight
                        font.bold: true
                        text: itemDlgt.display
                        color: Theme.deckTextColor
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textFontPixelSize
                    }

                    Skin.Button {
                        id: enabledButton

                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: 64
                        height: 32
                        activeColor: Theme.blue
                        highlight: itemDlgt.isOpen
                        text: itemDlgt.isOpen ? "On" : "Off"
                        onClicked: {
                            const path = mappingCombo.currentIndex >= 0
                                    ? itemDlgt.mappingOptions[mappingCombo.currentIndex].path
                                    : "";
                            Mixxx.ControllerManager.applyMapping(
                                    itemDlgt.index, path, !itemDlgt.isOpen);
                        }
                    }
                }

                Skin.ComboBox {
                    id: mappingCombo

                    width: parent.width
                    height: 40
                    textRole: "text"
                    model: itemDlgt.mappingOptions
                    onActivated: (index) => {
                        const path = itemDlgt.mappingOptions[index].path;
                        Mixxx.ControllerManager.applyMapping(
                                itemDlgt.index, path, path.length > 0);
                    }
                }
            }
        }
    }
}

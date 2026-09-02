import "." as Skin
import QtQuick 2.12
import "Theme"

// Touch-friendly, QML-native replacement for the legacy desktop
// Mixxx.PreferencesDialog. A flat category list (mirroring main.qml's
// top-level screen switching) with a Loader swapping in the selected
// category's detail page - a second, nested level of the same
// list-then-detail navigation idiom Library.qml uses at the top level.
//
// Only a handful of categories are implemented so far (the ones that are
// plain ConfigObject settings, needing no new device/controller-enumeration
// bridge) - add more entries to `categories` as they're built.
Item {
    id: root

    readonly property var categories: [
        {
            id: "accessibility",
            label: "Accessibility",
            source: "preferences/PrefsAccessibility.qml"
        },
        {
            id: "autodj",
            label: "Auto DJ",
            source: "preferences/PrefsAutoDj.qml"
        },
        {
            id: "interface",
            label: "Interface",
            source: "preferences/PrefsInterface.qml"
        },
        {
            id: "soundhardware",
            label: "Sound Hardware",
            source: "preferences/PrefsSoundHardware.qml"
        },
        {
            id: "controllers",
            label: "Controllers",
            source: "preferences/PrefsControllers.qml"
        }
    ]
    property string currentCategory: ""

    function sourceForCategory(categoryId) {
        for (let i = 0; i < root.categories.length; i++) {
            if (root.categories[i].id === categoryId) {
                return root.categories[i].source;
            }
        }
        return "";
    }

    Rectangle {
        color: Theme.deckBackgroundColor
        anchors.fill: parent

        ListView {
            id: categoryList

            anchors.fill: parent
            anchors.margins: 10
            clip: true
            visible: root.currentCategory === ""
            model: root.categories

            delegate: Item {
                id: itemDlgt

                required property var modelData

                implicitWidth: categoryList.width
                implicitHeight: 56

                Skin.EmbeddedBackground {
                    anchors.fill: parent
                }

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    verticalAlignment: Text.AlignVCenter
                    font.bold: true
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textFontPixelSize
                    color: Theme.deckTextColor
                    text: itemDlgt.modelData.label
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.currentCategory = itemDlgt.modelData.id;
                    }
                }
            }

            spacing: 6
        }

        Item {
            anchors.fill: parent
            anchors.margins: 10
            visible: root.currentCategory !== ""

            Skin.Button {
                id: categoryBackButton

                anchors.top: parent.top
                anchors.left: parent.left
                width: 96
                height: 36
                activeColor: Theme.white
                text: "< Back"
                onClicked: {
                    root.currentCategory = "";
                }
            }

            Loader {
                anchors.top: categoryBackButton.bottom
                anchors.topMargin: 8
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                source: root.currentCategory !== "" ? root.sourceForCategory(root.currentCategory) : ""
            }
        }
    }
}

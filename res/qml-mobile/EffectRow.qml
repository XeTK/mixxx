import "." as Skin
import QtQuick 2.12
import "Theme"

// Full-screen phone layout: the two effect units stack vertically, each
// with its three effect slots as full-width touch rows (always expanded -
// no desktop-style collapse button) and the unit's super/dry-wet knobs in
// a header row.
Item {
    id: root

    Column {
        anchors.fill: parent

        Repeater {
            model: 2

            Item {
                id: unit

                required property int index
                readonly property int unitNumber: index + 1
                readonly property string unitGroup: "[EffectRack1_EffectUnit" + unitNumber + "]"

                width: root.width
                height: root.height / 2

                Skin.SectionBackground {
                    anchors.fill: parent
                }

                Item {
                    id: unitHeader

                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 10
                    anchors.topMargin: 5
                    // Stay clear of the floating back button in the
                    // top-right corner (main.qml).
                    anchors.rightMargin: 64
                    height: 40

                    Skin.EmbeddedText {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: "FX " + unit.unitNumber
                    }

                    Skin.EmbeddedText {
                        anchors.right: superKnob.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Super"
                        font.bold: false
                    }

                    Skin.ControlKnob {
                        id: superKnob

                        anchors.right: mixLabel.left
                        anchors.rightMargin: 20
                        anchors.verticalCenter: parent.verticalCenter
                        width: 36
                        height: 36
                        arcStart: Knob.ArcStart.Minimum
                        group: unit.unitGroup
                        key: "super1"
                        color: Theme.effectUnitColor
                    }

                    Skin.EmbeddedText {
                        id: mixLabel

                        anchors.right: mixKnob.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Mix"
                        font.bold: false
                    }

                    Skin.ControlKnob {
                        id: mixKnob

                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: 36
                        height: 36
                        arcStart: Knob.ArcStart.Minimum
                        group: unit.unitGroup
                        key: "mix"
                        color: Theme.effectUnitColor
                    }
                }

                Column {
                    anchors.top: unitHeader.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 5

                    Repeater {
                        model: 3

                        Skin.EffectSlot {
                            required property int index

                            width: parent.width
                            height: (unit.height - unitHeader.height - 15) / 3
                            unitNumber: unit.unitNumber
                            effectNumber: index + 1
                            expanded: true
                        }
                    }
                }
            }
        }
    }
}

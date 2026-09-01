import "." as Skin
import QtQuick 2.12
import QtQuick.Shapes 1.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "Theme"

Column {
    id: root

    required property string group
    // When vertical space is tight, the gain and quick-effect filter move
    // into a second column beside the EQ stack (compact). With enough room
    // they rejoin the EQs as one tall stack: gain, EQs, filter.
    property bool compact: true
    // Renders the columns right-to-left so the compact gain/filter column
    // can face this channel's own deck on either side of the mixer.
    property bool mirrored: false
    property var player: Mixxx.PlayerManager.getPlayer(root.group)

    Mixxx.ControlProxy {
        id: stemCountControl

        group: root.group
        key: "stem_count"
    }

    function stemGroup(group, index) {
        return `${group.substr(0, group.length-1)}_Stem${index + 1}]`
    }

    Row {
        spacing: 4
        layoutDirection: root.mirrored ? Qt.RightToLeft : Qt.LeftToRight

        Column {
            id: stem
            spacing: 4
            // Was 10, while the knobs inside are 56 wide - they painted
            // ~46px past the column's declared bounds, so the mixer
            // undercounted its own width and the overflow landed on top of
            // the neighbouring deck's buttons.
            width: 56
            visible: opacity != 0
            Repeater {
                model: root.player.stemsModel

                Skin.StemKnob {
                    required property int index

                    id: stem
                    stemGroup: root.stemGroup(root.group, index)
                    property alias color: stem.stemColor
                }
            }
        }
        Column {
            id: eq
            spacing: 4
            // Same as the stem column above: match the knobs' real width.
            width: 56
            visible: opacity != 0

            Rectangle {
                visible: !root.compact
                width: 56
                height: 56
                color: Theme.knobBackgroundColor
                radius: 5

                Skin.ControlKnob {
                    anchors.centerIn: parent
                    width: 48
                    height: 48
                    group: root.group
                    key: "pregain"
                    color: Theme.gainKnobColor
                }
            }

            Skin.EqKnob {
                statusKey: "button_parameter3"
                knob.group: "[EqualizerRack1_" + root.group + "_Effect1]"
                knob.key: "parameter3"
                knob.color: Theme.eqHighColor
            }

            Skin.EqKnob {
                statusKey: "button_parameter2"
                knob.group: "[EqualizerRack1_" + root.group + "_Effect1]"
                knob.key: "parameter2"
                knob.color: Theme.eqMidColor
            }

            Skin.EqKnob {
                knob.group: "[EqualizerRack1_" + root.group + "_Effect1]"
                knob.key: "parameter1"
                statusKey: "button_parameter1"
                knob.color: Theme.eqLowColor
            }

            Skin.QuickFxKnob {
                visible: !root.compact
                group: "[QuickEffectRack1_" + root.group + "]"
                knob.arcStyle: ShapePath.DashLine
                knob.arcStylePattern: [2, 2]
                knob.color: Theme.eqFxColor
            }
        }

        Item {
            id: aux

            visible: root.compact
            width: 56
            height: eq.height

            // Compact mode only: gain pinned to the top, filter pinned to
            // the bottom of the container, empty space between.
            Rectangle {
                anchors.top: parent.top
                width: 56
                height: 56
                color: Theme.knobBackgroundColor
                radius: 5

                Skin.ControlKnob {
                    anchors.centerIn: parent
                    width: 48
                    height: 48
                    group: root.group
                    key: "pregain"
                    color: Theme.gainKnobColor
                }
            }

            Skin.QuickFxKnob {
                anchors.bottom: parent.bottom
                group: "[QuickEffectRack1_" + root.group + "]"
                knob.arcStyle: ShapePath.DashLine
                knob.arcStylePattern: [2, 2]
                knob.color: Theme.eqFxColor
            }
        }
        states: [
            State {
                name: "eq"
                when: stemCountControl.value == 0
                PropertyChanges { target: stem; opacity: 0; width: 0}
            },
            State {
                name: "stem"
                when: stemCountControl.value != 0
                PropertyChanges { target: eq; opacity: 0; width: 0 }
            }
        ]

        transitions: [
            Transition {
                from: "eq"
                to: "stem"
                ParallelAnimation {
                    PropertyAnimation { targets: [eq, stem]; properties: "opacity,width"; duration: 1000}
                }
            },
            Transition {
                from: "stem"
                to: "eq"
                ParallelAnimation {
                    PropertyAnimation { targets: [eq, stem]; properties: "opacity,width"; duration: 1000}
                }
            }
        ]
    }
}

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12

Item {
    id: root

    required property string leftDeckGroup
    required property string rightDeckGroup

    // The deck row hands us its full height. The knob/fader cluster keeps
    // its natural size while there's room and scales down uniformly once
    // there isn't, so short screens compress the mixer instead of clipping
    // its bottom off.
    // With enough vertical room, each channel's controls become one tall
    // stack (gain, EQs, filter) and the faders stretch to match; otherwise
    // the compact two-column layout kicks in. 310 = the tall stack's
    // natural height (4 knobs + filter + spacing) plus margins.
    readonly property bool stackedControls: height >= 310

    // Clamped well above zero because the height can transiently go
    // negative while the anchor layout settles, and a negative scale
    // renders everything mirrored.
    readonly property real contentScale: content.height > 0 ? Math.max(0.05, Math.min(1, height / (content.height + 10))) : 1

    implicitWidth: content.width * contentScale + 10

    Skin.SectionBackground {
        anchors.centerIn: parent
        width: parent.width
        height: Math.min(parent.height, (content.height + 10) * root.contentScale)
    }

    Row {
        id: content

        spacing: 5
        anchors.centerIn: parent
        scale: root.contentScale
        // The EQ stack is the intrinsic-height driver; sizing the row off
        // it explicitly (rather than letting the positioner and the
        // "height: parent.height" children negotiate it circularly) keeps
        // the geometry deterministic.
        height: leftEq.implicitHeight

        Skin.EqColumn {
            id: leftEq

            group: root.leftDeckGroup
            compact: !root.stackedControls
            // Gain/filter column faces the left deck.
            mirrored: true
        }

        Skin.MixerColumn {
            width: 56
            height: parent.height
            group: root.leftDeckGroup
        }

        Skin.MixerColumn {
            width: 56
            height: parent.height
            group: root.rightDeckGroup
        }

        Skin.EqColumn {
            group: root.rightDeckGroup
            compact: !root.stackedControls
        }
    }
}

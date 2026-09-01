import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12

// Full-screen phone layout: sampler pads in a grid instead of one long
// desktop strip.
Item {
    id: root

    // Only instantiate samplers that actually exist - referencing
    // unconfigured ones (e.g. [Sampler5]+ with the default of 4) spams
    // control errors and renders broken white cells.
    Mixxx.ControlProxy {
        id: numSamplersControl

        group: "[App]"
        key: "num_samplers"
    }

    readonly property int samplerCount: Math.min(8, numSamplersControl.value)
    readonly property int gridRows: Math.max(1, Math.ceil(samplerCount / 4))

    Grid {
        id: grid

        anchors.fill: parent
        anchors.margins: 5
        // Keep the first row's gain knobs out from under the floating
        // back button in the top-right corner (main.qml).
        anchors.topMargin: 50
        columns: 4
        spacing: 5

        Repeater {
            model: root.samplerCount

            Skin.Sampler {
                required property int index

                width: (grid.width - (grid.columns - 1) * grid.spacing) / grid.columns
                height: (grid.height - (root.gridRows - 1) * grid.spacing) / root.gridRows
                group: "[Sampler" + (index + 1) + "]"
            }
        }
    }
}

import "../" as Skin
import QtQuick 2.12
import QtQuick.Controls 2.12
import "../Theme"

// A mobile-relevant subset of DlgPrefInterface
// (src/preferences/dialog/dlgprefinterface.cpp). Skin/scheme selection,
// StartInFullscreen and HideMenuBar are dropped - the mobile shell only
// has the one skin and always runs full-screen/without a menu bar, so
// those settings don't apply here. Locale selection is dropped for this
// pass too - it needs to enumerate installed translation files, the same
// class of device/resource-enumeration problem as Sound Hardware, which is
// out of scope for the plain-ConfigObject bridge this page uses.
Item {
    id: root

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        Column {
            width: parent.width
            spacing: 10

            PrefsDropdownRow {
                width: parent.width
                label: "Tooltips"
                group: "[Controls]"
                key: "Tooltips"
                defaultValue: 1
                options: [
                    {
                        text: "Off",
                        value: 0
                    },
                    {
                        text: "On",
                        value: 1
                    },
                    {
                        text: "Only in library",
                        value: 2
                    }
                ]
            }

            PrefsDropdownRow {
                width: parent.width
                label: "Multi-sampling (anti-aliasing)"
                group: "[Preferences]"
                key: "multi_sampling"
                defaultValue: 4
                options: [
                    {
                        text: "Disabled",
                        value: 0
                    },
                    {
                        text: "2x MSAA",
                        value: 2
                    },
                    {
                        text: "4x MSAA",
                        value: 4
                    },
                    {
                        text: "8x MSAA",
                        value: 8
                    },
                    {
                        text: "16x MSAA",
                        value: 16
                    }
                ]
            }

            PrefsSliderRow {
                width: parent.width
                label: "UI scale"
                group: "[Config]"
                key: "ScaleFactor"
                defaultValue: 1.0
                from: 0.5
                to: 3.0
                stepSize: 0.05
                valueFormatter: (v) => Math.round(v * 100) + "%"
            }
        }
    }
}

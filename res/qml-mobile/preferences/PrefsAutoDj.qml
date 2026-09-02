import "../" as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.12
import "../Theme"

// Mirrors DlgPrefAutoDJ (src/preferences/dialog/dlgprefautodj.cpp). Skips
// "[Auto DJ],Requeue" - that's set by the "Repeat Playlist" toggle in the
// Auto DJ panel itself, not user-editable from Preferences on desktop
// either.
Item {
    id: root

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        Column {
            width: parent.width
            spacing: 10

            PrefsSliderRow {
                width: parent.width
                label: "Minimum tracks queued ahead"
                group: "[Auto DJ]"
                key: "MinimumAvailable"
                defaultValue: 20
                from: 1
                to: 100
                stepSize: 1
                valueFormatter: (v) => v.toFixed(0)
            }

            PrefsToggleRow {
                width: parent.width
                label: "Don't re-queue recently played tracks"
                group: "[Auto DJ]"
                key: "UseIgnoreTime"
                defaultValue: false
            }

            Item {
                width: parent.width
                height: 48

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Ignore time (HH:mm)"
                    color: Theme.deckTextColor
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textFontPixelSize
                }

                Skin.TextField {
                    id: ignoreTimeField

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 100
                    height: 40
                    text: Mixxx.Config.getValue("[Auto DJ]", "IgnoreTime", "23:59")
                    onEditingFinished: {
                        Mixxx.Config.setValue("[Auto DJ]", "IgnoreTime", ignoreTimeField.text);
                    }
                }
            }

            PrefsToggleRow {
                width: parent.width
                label: "Randomly enqueue tracks from the library"
                group: "[Auto DJ]"
                key: "EnableRandomQueue"
                defaultValue: false
            }

            PrefsSliderRow {
                width: parent.width
                label: "Minimum random tracks queued ahead"
                group: "[Auto DJ]"
                key: "RandomQueueMinimumAllowed"
                defaultValue: 5
                from: 1
                to: 50
                stepSize: 1
                valueFormatter: (v) => v.toFixed(0)
            }

            PrefsToggleRow {
                width: parent.width
                label: "Re-center crossfader when Auto DJ is disabled"
                group: "[Auto DJ]"
                key: "center_xfader_when_disabling"
                defaultValue: false
            }
        }
    }
}

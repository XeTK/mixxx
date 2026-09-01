import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property string group
    property bool minimized: false
    property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)

    Drag.active: dragArea.drag.active
    Drag.dragType: Drag.Automatic
    Drag.supportedActions: Qt.CopyAction
    Drag.mimeData: {
        let data = {
            "mixxx/player": group
        };
        const trackLocationUrl = deckPlayer.trackLocationUrl;
        if (trackLocationUrl)
            data["text/uri-list"] = trackLocationUrl;

        return data;
    }

    MouseArea {
        id: dragArea

        anchors.fill: root
        drag.target: root
    }

    Skin.SectionBackground {
        anchors.fill: parent
    }

    // DeckInfoBar (cover art/title/key/bpm) doesn't get its own row here on
    // mobile - it floats over the main waveform in main.qml instead
    // (Skin.TrackInfoBubble), reclaiming this row's height for the rest of
    // the deck controls.
    Skin.SyncButton {
        id: syncButton

        visible: !root.minimized
        anchors.top: parent.top
        anchors.topMargin: 5
        anchors.right: parent.right
        anchors.rightMargin: 5
        group: root.group

        FadeBehavior on visible {
            fadeTarget: syncButton
        }
    }

    // The tempo slider runs the full deck height (below Sync) instead of
    // stopping at the button bar - pitch bending by touch needs the
    // travel distance far more than the buttons need the extra width.
    Skin.ControlSlider {
        id: rateSlider

        visible: !root.minimized
        anchors.topMargin: 5
        anchors.rightMargin: 5
        anchors.bottomMargin: 5
        anchors.top: syncButton.bottom
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: syncButton.width
        group: root.group
        key: "rate"
        barStart: 0.5
        barColor: Theme.bpmSliderBarColor
        bg: Theme.imgBpmSliderBackground

        FadeBehavior on visible {
            fadeTarget: rateSlider
        }
    }

    Rectangle {
        id: overview

        visible: !root.minimized
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        anchors.topMargin: 5
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: rateSlider.left
        radius: 5
        color: Theme.deckBackgroundColor
        // Was 56 - this mini overview duplicates the big waveform above it
        // (main.qml), so it doesn't need to be nearly as tall; shrunk to
        // just enough to still show the FX1/FX2/quantize/passthrough row
        // underneath it.
        height: 34

        Skin.WaveformOverview {
            group: root.group
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: parent.height - 22
        }

        Item {
            id: waveformBar

            // The strip's children are fixed-width, so on narrow decks they
            // pile on top of each other - shed the least important ones
            // progressively instead of garbling, down to nothing on decks
            // squeezed by extreme aspect ratios.
            readonly property bool showPassthrough: width >= 290
            readonly property bool showPosition: width >= 225
            readonly property bool showQuantize: width >= 170
            readonly property bool showFx2: width >= 115
            readonly property bool showFx1: width >= 60

            height: 22
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            Rectangle {
                id: waveformBarVSeparator

                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.rightMargin: 5
                anchors.leftMargin: 5
                height: 2
                color: Theme.deckLineColor
            }

            InfoBarButton {
                visible: waveformBar.showFx1
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.leftMargin: 5
                width: rateSlider.width
                group: "[EffectRack1_EffectUnit1]"
                key: "group_" + root.group + "_enable"
                activeColor: Theme.deckActiveColor

                foreground: Skin.EmbeddedText {
                    anchors.centerIn: parent
                    text: "FX 1"
                }
            }

            Rectangle {
                id: waveformBarHSeparator1

                visible: waveformBar.showFx2
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: waveformBarVSeparator.left
                anchors.leftMargin: rateSlider.width
                width: 2
                color: Theme.deckLineColor
            }

            InfoBarButton {
                visible: waveformBar.showFx2
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: waveformBarHSeparator1.left
                width: rateSlider.width
                group: "[EffectRack1_EffectUnit2]"
                key: "group_" + root.group + "_enable"
                activeColor: Theme.deckActiveColor

                foreground: Skin.EmbeddedText {
                    anchors.centerIn: parent
                    text: "FX 2"
                }
            }

            Rectangle {
                id: waveformBarHSeparator2

                visible: waveformBar.showFx2
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: waveformBarHSeparator1.right
                anchors.leftMargin: rateSlider.width
                width: 2
                color: Theme.deckLineColor
            }

            Skin.EmbeddedText {
                id: waveformBarPosition

                visible: waveformBar.showPosition
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: waveformBarHSeparator2.right
                anchors.leftMargin: 5
                text: {
                    const positionSeconds = samplesControl.value / 2 / sampleRateControl.value * playPositionControl.value;
                    if (isNaN(positionSeconds))
                        return "";

                    // The position can be negative while the playhead is in
                    // the preroll region before 0:00 (e.g. right after
                    // cueing to the very start) - format the magnitude and
                    // add the sign back, otherwise the zero-padding mangles
                    // it into nonsense like "0-1:58.9".
                    const sign = positionSeconds < 0 ? "-" : "";
                    const absoluteSeconds = Math.abs(positionSeconds);
                    let minutes = Math.floor(absoluteSeconds / 60);
                    let seconds = absoluteSeconds - (minutes * 60);
                    const deciseconds = Math.trunc((seconds - Math.trunc(seconds)) * 10);
                    seconds = Math.trunc(seconds);
                    if (minutes < 10)
                        minutes = "0" + minutes;

                    if (seconds < 10)
                        seconds = "0" + seconds;

                    return sign + minutes + ':' + seconds + "." + deciseconds;
                }

                Mixxx.ControlProxy {
                    id: playPositionControl

                    group: root.group
                    key: "playposition"
                }

                Mixxx.ControlProxy {
                    id: sampleRateControl

                    group: root.group
                    key: "track_samplerate"
                }

                Mixxx.ControlProxy {
                    id: samplesControl

                    group: root.group
                    key: "track_samples"
                }
            }

            Item {
                id: waveformBarRightSpace

                anchors.top: waveformBar.top
                anchors.bottom: waveformBar.bottom
                anchors.right: waveformBar.right
                width: rateSlider.width
            }

            Rectangle {
                id: waveformBarHSeparator

                anchors.top: waveformBar.top
                anchors.bottom: waveformBar.bottom
                anchors.right: waveformBarRightSpace.left
                anchors.bottomMargin: 5
                width: 2
                color: Theme.deckLineColor
            }

            InfoBarButton {
                visible: waveformBar.showQuantize
                anchors.top: waveformBarVSeparator.bottom
                anchors.bottom: waveformBar.bottom
                anchors.left: waveformBarRightSpace.left
                anchors.right: waveformBarRightSpace.right
                group: root.group
                key: "quantize"
                activeColor: Theme.deckActiveColor

                foreground: Image {
                    anchors.centerIn: parent
                    source: "images/icon_quantize.svg"
                }
            }

            Item {
                id: waveformBarLeftSpace

                anchors.top: waveformBar.top
                anchors.bottom: waveformBar.bottom
                anchors.right: waveformBarHSeparator.left
                width: rateSlider.width
            }

            Rectangle {
                id: waveformBarHSeparator3

                visible: waveformBar.showPassthrough
                anchors.top: waveformBar.top
                anchors.bottom: waveformBar.bottom
                anchors.right: waveformBarLeftSpace.left
                anchors.bottomMargin: 5
                width: 2
                color: Theme.deckLineColor
            }

            InfoBarButton {
                visible: waveformBar.showPassthrough
                anchors.top: waveformBarVSeparator.bottom
                anchors.bottom: waveformBar.bottom
                anchors.left: waveformBarLeftSpace.left
                anchors.right: waveformBarLeftSpace.right
                group: root.group
                key: "passthrough"
                activeColor: Theme.deckActiveColor

                foreground: Image {
                    anchors.centerIn: parent
                    source: "images/icon_passthrough.svg"
                }
            }
        }

        FadeBehavior on visible {
            fadeTarget: overview
        }
    }

    Item {
        id: buttonBar

        // Was a fixed 56px strip - now fills whatever's left below the mini
        // overview instead of leaving dead space, and CUE/PLAY/hotcues size
        // themselves (via playButton.height) off that actual available
        // height rather than a small fixed default.
        anchors.top: overview.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: rateSlider.left
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        anchors.topMargin: 5
        anchors.bottomMargin: 5
        visible: !root.minimized

        // Sized off the real remaining width (not a multiple of the button
        // height) so the row always fits the deck regardless of window
        // aspect ratio.
        readonly property real hotcueButtonWidth: Math.max(20, (buttonBar.width + 3) / 4)

        // Play and Cue share the bottom row, Play always on the left; the
        // hotcues get the full top row to themselves.
        Skin.ControlButton {
            id: playButton

            anchors.left: parent.left
            anchors.bottom: parent.bottom
            width: (buttonBar.width - 5) / 2
            height: (buttonBar.height - 5) / 2
            group: root.group
            key: "play"
            text: "Play"
            toggleable: true
            activeColor: Theme.deckActiveColor
        }

        Skin.ControlButton {
            id: cueButton

            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: (buttonBar.width - 5) / 2
            height: (buttonBar.height - 5) / 2
            group: root.group
            key: "cue_default"
            text: "Cue"
            activeColor: Theme.deckActiveColor
        }

        Row {
            anchors.left: parent.left
            anchors.top: parent.top
            spacing: -1

            Repeater {
                model: 4

                Skin.HotcueButton {
                    required property int index

                    hotcueNumber: this.index + 1
                    group: root.group
                    width: buttonBar.hotcueButtonWidth
                    height: playButton.height
                }
            }
        }

        FadeBehavior on visible {
            fadeTarget: buttonBar
        }
    }

    Mixxx.PlayerDropArea {
        anchors.fill: parent
        group: root.group
    }
}

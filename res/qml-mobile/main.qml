import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.12
import "Theme"

ApplicationWindow {
    id: root

    property alias show4decks: show4DecksButton.checked
    property alias showEffects: showEffectsButton.checked
    property alias showSamplers: showSamplersButton.checked
    property alias maximizeLibrary: maximizeLibraryButton.checked
    // Hides per-deck knobs/hotcues/rate slider/mixer, leaving just the big
    // waveform + basic transport - meant for when a physical controller is
    // connected and the phone screen is better used as a waveform display
    // than a duplicate set of on-screen controls.
    property alias compactControls: compactControlsButton.checked
    property bool toolbarCollapsed: false

    width: 1920
    height: 1080
    color: Theme.backgroundColor
    visible: true

    Flickable {
        id: mainFlickable

        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(mainColumn.implicitHeight, height)
        clip: true
        boundsBehavior: Flickable.StopAtBounds

    Column {
        id: mainColumn

        width: mainFlickable.width

        Rectangle {
            id: toolbar

            width: parent.width
            height: root.toolbarCollapsed ? 14 : 36
            color: Theme.toolbarBackgroundColor
            radius: 1

            Behavior on height {
                NumberAnimation {
                    duration: 120
                }
            }

            Flickable {
                anchors.fill: parent
                anchors.rightMargin: 22
                contentWidth: toolbarRow.implicitWidth + 10
                contentHeight: height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                visible: !root.toolbarCollapsed

                Row {
                    id: toolbarRow

                    padding: 5
                    spacing: 5

                Skin.Button {
                    id: show4DecksButton

                    text: "4 Decks"
                    activeColor: Theme.white
                    checkable: true
                }

                Skin.Button {
                    id: compactControlsButton

                    text: "Compact"
                    activeColor: Theme.white
                    checkable: true
                }

                Skin.Button {
                    id: maximizeLibraryButton

                    text: "Library"
                    activeColor: Theme.white
                    checkable: true
                }

                Skin.Button {
                    id: showEffectsButton

                    text: "Effects"
                    activeColor: Theme.white
                    checkable: true
                }

                Skin.Button {
                    id: showSamplersButton

                    text: "Sampler"
                    activeColor: Theme.white
                    checkable: true
                }

                Skin.Button {
                    id: showPreferencesButton

                    text: "Prefs"
                    activeColor: Theme.white
                    onClicked: {
                        Mixxx.PreferencesDialog.show();
                    }
                }

                Skin.Button {
                    id: showDevToolsButton

                    text: "Develop"
                    activeColor: Theme.white
                    checkable: true
                    checked: devToolsWindow.visible
                    onClicked: {
                        if (devToolsWindow.visible)
                            devToolsWindow.close();
                        else
                            devToolsWindow.show();
                    }

                    DeveloperToolsWindow {
                        id: devToolsWindow

                        width: 640
                        height: 480
                    }
                }
                }
            }

            Skin.Button {
                id: toolbarCollapseButton

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 20
                height: parent.height
                text: root.toolbarCollapsed ? "▸" : "▾"
                activeColor: Theme.white
                onClicked: {
                    root.toolbarCollapsed = !root.toolbarCollapsed;
                }
            }
        }

        Skin.WaveformDisplay {
            id: deck3waveform

            group: "[Channel3]"
            width: root.width
            height: 90
            visible: root.show4decks && !root.maximizeLibrary

            FadeBehavior on visible {
                fadeTarget: deck3waveform
            }
        }

        Skin.WaveformDisplay {
            id: deck1waveform

            group: "[Channel1]"
            width: root.width
            height: 90
            visible: !root.maximizeLibrary

            FadeBehavior on visible {
                fadeTarget: deck1waveform
            }
        }

        Skin.WaveformDisplay {
            id: deck2waveform

            group: "[Channel2]"
            width: root.width
            height: 90
            visible: !root.maximizeLibrary

            FadeBehavior on visible {
                fadeTarget: deck2waveform
            }
        }

        Skin.WaveformDisplay {
            id: deck4waveform

            group: "[Channel4]"
            width: root.width
            height: 90
            visible: root.show4decks && !root.maximizeLibrary

            FadeBehavior on visible {
                fadeTarget: deck4waveform
            }
        }

        Skin.DeckRow {
            id: decks12

            leftDeckGroup: "[Channel1]"
            rightDeckGroup: "[Channel2]"
            width: parent.width
            minimized: root.maximizeLibrary || root.compactControls
        }

        Skin.CrossfaderRow {
            id: crossfader

            crossfaderWidth: decks12.mixer.width
            width: parent.width
            visible: !root.maximizeLibrary

            Skin.FadeBehavior on visible {
                fadeTarget: crossfader
            }
        }

        Skin.DeckRow {
            id: decks34

            leftDeckGroup: "[Channel3]"
            rightDeckGroup: "[Channel4]"
            width: parent.width
            minimized: root.maximizeLibrary || root.compactControls
            visible: root.show4decks

            Skin.FadeBehavior on visible {
                fadeTarget: decks34
            }
        }

        Skin.SamplerRow {
            id: samplers

            width: parent.width
            visible: root.showSamplers

            Skin.FadeBehavior on visible {
                fadeTarget: samplers
            }
        }

        Skin.EffectRow {
            id: effects

            width: parent.width
            visible: root.showEffects

            Skin.FadeBehavior on visible {
                fadeTarget: effects
            }
        }

        Skin.Library {
            width: parent.width
            height: Math.max(mainFlickable.height - y, 300)
        }

        move: Transition {
            NumberAnimation {
                properties: "x,y"
                duration: 150
            }
        }
    }
    }
}

import "../" as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.12
import "../Theme"

// Mirrors the parts of DlgPrefSound (src/preferences/dialog/dlgprefsound.cpp)
// that make sense without a full per-channel routing matrix: pick an audio
// API, an output device, optionally an input device, a sample rate, and a
// buffer size - one "Main" stereo output/input path per device, the same
// shortcut SoundManagerConfig::loadDefaults() itself uses. Deep per-channel
// assignment (headphones, mic 1/2, vinyl control, etc.) is out of scope.
Item {
    id: root

    property var apiList: []
    property var sampleRateList: []
    property bool useInputDevice: false
    property string statusMessage: ""
    property color statusColor: Theme.deckTextColor

    // AudioBufferSizeIndex values (SoundManagerConfig::AudioBufferSizeIndex)
    // - fixed set, doubling per step, so no need to query them dynamically.
    readonly property var bufferSizeOptions: [
        { text: "~1 ms", value: 1 },
        { text: "~2 ms", value: 2 },
        { text: "~5 ms", value: 3 },
        { text: "~10 ms", value: 4 },
        { text: "~20 ms", value: 5 },
        { text: "~40 ms", value: 6 },
        { text: "~80 ms", value: 7 }
    ]

    function refreshForApi(api) {
        Mixxx.SoundManager.refreshDevicesForApi(api);
        root.sampleRateList = Mixxx.SoundManager.getSampleRates(api);
    }

    function loadCurrentConfig() {
        root.apiList = Mixxx.SoundManager.getHostApiList();
        const current = Mixxx.SoundManager.getCurrentConfig();

        const apiIndex = root.apiList.indexOf(current.api);
        apiCombo.currentIndex = apiIndex >= 0 ? apiIndex : 0;
        root.refreshForApi(apiCombo.currentText);

        const outputIndex = outputCombo.find(current.outputDeviceName);
        outputCombo.currentIndex = outputIndex >= 0 ? outputIndex : 0;

        root.useInputDevice = current.inputDeviceName && current.inputDeviceName.length > 0;
        if (root.useInputDevice) {
            const inputIndex = inputCombo.find(current.inputDeviceName);
            inputCombo.currentIndex = inputIndex >= 0 ? inputIndex : 0;
        }

        const rateIndex = root.sampleRateList.indexOf(current.sampleRate);
        sampleRateCombo.currentIndex = rateIndex >= 0 ? rateIndex : 0;

        for (let i = 0; i < root.bufferSizeOptions.length; i++) {
            if (root.bufferSizeOptions[i].value === current.bufferSizeIndex) {
                bufferSizeCombo.currentIndex = i;
                break;
            }
        }
    }

    Component.onCompleted: root.loadCurrentConfig()

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        Column {
            width: parent.width
            spacing: 10

            Skin.SectionText {
                width: parent.width
                text: "Audio Device"
            }

            Item {
                width: parent.width
                height: 48

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "API"
                    color: Theme.deckTextColor
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textFontPixelSize
                }

                Skin.ComboBox {
                    id: apiCombo

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width * 0.55
                    height: 40
                    model: root.apiList
                    onActivated: (index) => {
                        root.refreshForApi(apiCombo.textAt(index));
                        outputCombo.currentIndex = 0;
                        inputCombo.currentIndex = 0;
                        sampleRateCombo.currentIndex = 0;
                    }
                }
            }

            Item {
                width: parent.width
                height: 48

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Output device"
                    color: Theme.deckTextColor
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textFontPixelSize
                }

                Skin.ComboBox {
                    id: outputCombo

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width * 0.55
                    height: 40
                    textRole: "display"
                    model: Mixxx.SoundManager.outputDevices
                }
            }

            Item {
                width: parent.width
                height: 48

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Use input device"
                    color: Theme.deckTextColor
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textFontPixelSize
                }

                Skin.Button {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 64
                    height: 40
                    activeColor: Theme.blue
                    highlight: root.useInputDevice
                    text: root.useInputDevice ? "On" : "Off"
                    onClicked: root.useInputDevice = !root.useInputDevice
                }
            }

            Item {
                width: parent.width
                height: 48
                visible: root.useInputDevice

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Input device"
                    color: Theme.deckTextColor
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textFontPixelSize
                }

                Skin.ComboBox {
                    id: inputCombo

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width * 0.55
                    height: 40
                    textRole: "display"
                    model: Mixxx.SoundManager.inputDevices
                }
            }

            Item {
                width: parent.width
                height: 48

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Sample rate"
                    color: Theme.deckTextColor
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textFontPixelSize
                }

                Skin.ComboBox {
                    id: sampleRateCombo

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width * 0.55
                    height: 40
                    model: root.sampleRateList
                }
            }

            Item {
                width: parent.width
                height: 48

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Buffer size"
                    color: Theme.deckTextColor
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textFontPixelSize
                }

                Skin.ComboBox {
                    id: bufferSizeCombo

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width * 0.55
                    height: 40
                    textRole: "text"
                    model: root.bufferSizeOptions
                }
            }

            Skin.Button {
                width: 120
                height: 40
                activeColor: Theme.blue
                text: "Apply"
                onClicked: {
                    const outputRow = outputCombo.currentIndex;
                    const inputRow = root.useInputDevice ? inputCombo.currentIndex : -1;
                    const rate = root.sampleRateList[sampleRateCombo.currentIndex];
                    const bufferIndex = root.bufferSizeOptions[bufferSizeCombo.currentIndex].value;
                    const ok = Mixxx.SoundManager.applyConfig(
                            apiCombo.currentText, outputRow, inputRow, rate, bufferIndex);
                    if (ok) {
                        root.statusMessage = "Applied successfully.";
                        root.statusColor = Theme.green;
                    } else {
                        root.statusMessage = Mixxx.SoundManager.getLastErrorMessage();
                        root.statusColor = Theme.red;
                    }
                }
            }

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                visible: root.statusMessage.length > 0
                text: root.statusMessage
                color: root.statusColor
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textFontPixelSize
            }
        }
    }
}

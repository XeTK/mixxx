import "../" as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "../Theme"

// Mirrors the basic parts of DlgPrefController/DlgPrefControllers
// (src/controllers/dlgprefcontroller[s].cpp): list detected controllers,
// toggle each on/off, pick a mapping preset. Deep per-control mapping
// editing / MIDI-learn stays out of scope, same as on desktop that's a
// dedicated dialog, not something this pass replicates.
Item {
    id: root

    // Bonded Bluetooth devices as {name, address}, from the BLE MIDI
    // connect flow below.
    property var bleDevices: []
    property string bleStatus: ""
    property color bleStatusColor: Theme.deckTextColor
    // iOS has no discrete "bonded devices" list to show or connect to -
    // pairing goes entirely through Apple's own CABTMIDICentralViewController
    // system UI (see QmlControllerManagerProxy::startBluetoothMidiScan()),
    // so the device combo/Connect button below don't apply there.
    readonly property bool isIos: Qt.platform.os === "ios"

    Text {
        anchors.centerIn: parent
        visible: listView.count === 0 && root.bleDevices.length === 0
        text: "No controllers detected"
        color: Theme.deckTextColor
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textFontPixelSize
    }

    // Pre-select whichever device last connected successfully (persisted
    // across restarts) instead of always defaulting to the first bonded
    // device in the list.
    function selectRememberedDevice() {
        if (root.bleDevices.length === 0) {
            return;
        }
        const lastAddress = Mixxx.ControllerManager.getLastBluetoothMidiAddress();
        let selectedIndex = 0;
        if (lastAddress.length > 0) {
            for (let i = 0; i < root.bleDevices.length; i++) {
                if (root.bleDevices[i].address === lastAddress) {
                    selectedIndex = i;
                    break;
                }
            }
        }
        bleCombo.currentIndex = selectedIndex;
    }

    function refreshBleDevices() {
        root.bleDevices = Mixxx.ControllerManager.getBluetoothMidiDevices();
        root.selectRememberedDevice();
    }

    function mergeBleDevices(devices) {
        // Merge scanned devices into the bonded list, deduplicating by
        // address (a device can be both bonded and advertising).
        const merged = [];
        const seen = new Set();
        for (const device of root.bleDevices.concat(devices)) {
            if (!seen.has(device.address)) {
                seen.add(device.address);
                merged.push(device);
            }
        }
        root.bleDevices = merged;
        root.selectRememberedDevice();
    }

    Component.onCompleted: root.refreshBleDevices()

    Column {
        id: bleColumn

        width: parent.width
        spacing: 10

        // BLE MIDI devices only show up in the controller list once
        // something establishes their GATT connection, which Android's
        // Settings Bluetooth screen is unreliable at. This lets Mixxx
        // open the connection itself (MidiManager.openBluetoothDevice)
        // for any already-bonded device.
        Skin.SectionText {
            width: parent.width
            text: "Bluetooth MIDI"
        }

        Item {
            width: parent.width
            height: 48
            visible: !root.isIos

            Skin.ComboBox {
                id: bleCombo

                anchors.left: parent.left
                anchors.right: bleConnectButton.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                height: 40
                model: root.bleDevices.map((device) => device.name)
            }

            Skin.Button {
                id: bleConnectButton

                anchors.right: bleScanButton.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 110
                height: 40
                activeColor: Theme.blue
                text: "Connect"
                enabled: root.bleDevices.length > 0 && !root.bleConnecting
                onClicked: {
                    if (bleCombo.currentIndex < 0) {
                        return;
                    }
                    const device = root.bleDevices[bleCombo.currentIndex];
                    root.bleStatusColor = Theme.deckTextColor;
                    root.bleStatus = "Connecting to " + device.name + " ...";
                    Mixxx.ControllerManager.connectBluetoothMidiDevice(device.address);
                }
            }

            Skin.Button {
                id: bleScanButton

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 90
                height: 40
                activeColor: Theme.blue
                text: root.bleScanning ? "..." : "Scan"
                enabled: !root.bleScanning
                onClicked: {
                    root.bleScanning = true;
                    root.bleStatusColor = Theme.deckTextColor;
                    root.bleStatus = "Scanning for BLE MIDI devices ...";
                    Mixxx.ControllerManager.startBluetoothMidiScan();
                }
            }
        }

        // iOS equivalent of the row above: a single button opens Apple's
        // own Bluetooth MIDI pairing screen (scan, pair, and connect are
        // all handled there - see QmlControllerManagerProxy::
        // startBluetoothMidiScan()'s iOS branch), rather than a custom
        // device list Mixxx has no way to populate on this platform.
        Skin.Button {
            width: parent.width
            height: 40
            visible: root.isIos
            activeColor: Theme.blue
            enabled: !root.bleScanning
            text: root.bleScanning ? "..." : "Pair a Bluetooth MIDI Device..."
            onClicked: {
                root.bleScanning = true;
                root.bleStatusColor = Theme.deckTextColor;
                root.bleStatus = "";
                Mixxx.ControllerManager.startBluetoothMidiScan();
            }
        }

        Item {
            width: parent.width
            height: 48
            visible: !root.isIos && root.bleDevices.length === 0

            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: root.blePermissionRequested
                        ? "No bonded Bluetooth devices found"
                        : "Bonded Bluetooth devices need permission"
                color: Theme.deckTextColor
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textFontPixelSize
            }

            Skin.Button {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 110
                height: 40
                activeColor: Theme.blue
                visible: !root.blePermissionRequested
                text: "Allow"
                onClicked: Mixxx.ControllerManager.requestBluetoothPermission()
            }
        }

        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            visible: root.bleStatus.length > 0
            text: root.bleStatus
            color: root.bleStatusColor
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize
        }
    }

    property bool bleConnecting: false
    property bool blePermissionRequested: false
    property bool bleScanning: false

    Connections {
        target: Mixxx.ControllerManager

        function onBluetoothMidiDeviceConnected(success, deviceName) {
            root.bleConnecting = false;
            if (success) {
                root.bleStatusColor = Theme.green;
                root.bleStatus = "Connected " + deviceName;
            } else {
                root.bleStatusColor = Theme.red;
                root.bleStatus = "Bluetooth MIDI connection failed";
            }
        }

        function onBluetoothScanFinished(devices) {
            root.bleScanning = false;
            if (root.isIos) {
                // iOS's pairing UI (Apple's own CABTMIDICentralViewController)
                // always reports back as an empty list here regardless of
                // whether a device was actually paired - see
                // QmlControllerManagerProxy::startBluetoothMidiScan()'s iOS
                // branch - so an empty list means "the pairing screen
                // closed", not "nothing found". Any paired device already
                // shows up in the controller list below by the time this
                // fires.
                root.bleStatus = "";
                return;
            }
            if (devices.length === 0) {
                root.bleStatusColor = Theme.red;
                root.bleStatus = "No BLE MIDI devices found - is the controller in pairing mode?";
            } else {
                root.bleStatus = "";
            }
            root.mergeBleDevices(devices);
        }

        function onBluetoothPermissionResult(granted) {
            root.blePermissionRequested = true;
            if (!granted) {
                root.bleStatusColor = Theme.red;
                root.bleStatus = "Bluetooth permission denied";
            }
            root.refreshBleDevices();
        }
    }

    ListView {
        id: listView

        // Anchored to the BLE section's actual (variable-height) bottom
        // rather than a fixed margin - that section's height changes
        // depending on whether the permission-request row and/or status
        // text are visible, and a fixed margin tuned for one state left a
        // large dead gap (or, in principle, an overlap) in every other
        // state.
        anchors.top: bleColumn.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        spacing: 10
        model: Mixxx.ControllerManager.controllers

        delegate: Item {
            id: itemDlgt

            required property int index
            required property string display
            required property bool isOpen
            property var mappingOptions: []
            // Settings (checkboxes/dropdowns) exposed by the currently
            // loaded mapping's <settings> block, e.g. the DDJ-FLX2's
            // accessibility-pads toggle - see
            // QmlControllerManagerProxy::getMappingSettings().
            property var settingsList: []

            implicitWidth: listView.width
            // Base row (name/enable + mapping combo) plus one row per
            // setting - computed from the data rather than the settings
            // Column's own rendered height, since that Column is inside
            // an anchors.fill:parent Column and so can't be read back
            // without a binding loop.
            implicitHeight: 96 + (itemDlgt.settingsList.length > 0
                    ? 6 + itemDlgt.settingsList.length * 46
                    : 0)

            function loadMappings() {
                const mappings = Mixxx.ControllerManager.getMappingsForController(itemDlgt.index);
                itemDlgt.mappingOptions = [{ text: "No mapping", path: "" }].concat(
                        mappings.map((m) => ({ text: m.name, path: m.path })));

                const currentPath = Mixxx.ControllerManager.getCurrentMappingPath(itemDlgt.index);
                let selectedIndex = 0;
                for (let i = 0; i < itemDlgt.mappingOptions.length; i++) {
                    if (itemDlgt.mappingOptions[i].path === currentPath) {
                        selectedIndex = i;
                        break;
                    }
                }
                mappingCombo.currentIndex = selectedIndex;
                itemDlgt.loadSettings();
            }

            function loadSettings() {
                itemDlgt.settingsList = Mixxx.ControllerManager.getMappingSettings(itemDlgt.index);
            }

            Component.onCompleted: itemDlgt.loadMappings()

            Skin.EmbeddedBackground {
                anchors.fill: parent
            }

            Column {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                Item {
                    width: parent.width
                    height: 32

                    Text {
                        anchors.left: parent.left
                        anchors.right: enabledButton.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideRight
                        font.bold: true
                        text: itemDlgt.display
                        color: Theme.deckTextColor
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textFontPixelSize
                    }

                    Skin.Button {
                        id: enabledButton

                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: 64
                        height: 32
                        activeColor: Theme.blue
                        highlight: itemDlgt.isOpen
                        text: itemDlgt.isOpen ? "On" : "Off"
                        onClicked: {
                            const path = mappingCombo.currentIndex >= 0
                                    ? itemDlgt.mappingOptions[mappingCombo.currentIndex].path
                                    : "";
                            Mixxx.ControllerManager.applyMapping(
                                    itemDlgt.index, path, !itemDlgt.isOpen);
                            itemDlgt.loadSettings();
                        }
                    }
                }

                Skin.ComboBox {
                    id: mappingCombo

                    width: parent.width
                    height: 40
                    textRole: "text"
                    model: itemDlgt.mappingOptions
                    onActivated: (index) => {
                        const path = itemDlgt.mappingOptions[index].path;
                        Mixxx.ControllerManager.applyMapping(
                                itemDlgt.index, path, path.length > 0);
                        itemDlgt.loadSettings();
                    }
                }

                Column {
                    id: settingsColumn

                    width: parent.width
                    spacing: 6
                    visible: itemDlgt.settingsList.length > 0

                    Repeater {
                        model: itemDlgt.settingsList

                        delegate: Item {
                            id: settingDlgt

                            required property var modelData

                            width: settingsColumn.width
                            height: 40

                            Text {
                                anchors.left: parent.left
                                anchors.right: parent.horizontalCenter
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                elide: Text.ElideRight
                                text: settingDlgt.modelData.label
                                color: Theme.deckTextColor
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.textFontPixelSize
                            }

                            Skin.Button {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                width: 64
                                height: 32
                                visible: settingDlgt.modelData.type === "boolean"
                                activeColor: Theme.blue
                                highlight: settingDlgt.modelData.value === true
                                text: settingDlgt.modelData.value ? "On" : "Off"
                                onClicked: {
                                    Mixxx.ControllerManager.setMappingSetting(
                                            itemDlgt.index,
                                            settingDlgt.modelData.variable,
                                            !settingDlgt.modelData.value);
                                    itemDlgt.loadSettings();
                                }
                            }

                            Skin.ComboBox {
                                id: enumSettingCombo

                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width * 0.5
                                height: 40
                                visible: settingDlgt.modelData.type === "enum"
                                textRole: "label"
                                model: settingDlgt.modelData.type === "enum"
                                        ? settingDlgt.modelData.options
                                        : []
                                Component.onCompleted: {
                                    for (let i = 0; i < enumSettingCombo.model.length; i++) {
                                        if (enumSettingCombo.model[i].value ===
                                                settingDlgt.modelData.value) {
                                            enumSettingCombo.currentIndex = i;
                                            break;
                                        }
                                    }
                                }
                                onActivated: (index) => {
                                    Mixxx.ControllerManager.setMappingSetting(
                                            itemDlgt.index,
                                            settingDlgt.modelData.variable,
                                            settingDlgt.modelData.options[index].value);
                                    itemDlgt.loadSettings();
                                }
                            }

                            Text {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                visible: settingDlgt.modelData.type === "other"
                                text: settingDlgt.modelData.value
                                color: Theme.deckTextColor
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.textFontPixelSize
                            }
                        }
                    }
                }
            }
        }
    }

    // "No controllers" placeholder only counts the non-BLE list; with the
    // BLE section above it, centering on the whole page would overlap it.
}

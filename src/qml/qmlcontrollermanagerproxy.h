#pragma once
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <memory>

#include "controllers/controllermanager.h"
#include "preferences/usersettings.h"
#include "qml/qmlcontrollerlistmodel.h"

class LegacyControllerMapping;

namespace mixxx {
namespace qml {

class QmlControllerManagerProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(mixxx::qml::QmlControllerListModel* controllers
                    MEMBER m_pControllerListModel CONSTANT);
    QML_NAMED_ELEMENT(ControllerManager)
    QML_SINGLETON

  public:
    explicit QmlControllerManagerProxy(
            std::shared_ptr<ControllerManager> pControllerManager,
            UserSettingsPointer pConfig,
            QObject* parent = nullptr);
    ~QmlControllerManagerProxy() override;

    // List of {name, path} for mapping presets matching this controller's
    // type (e.g. .midi.xml), combining the user and system mapping dirs.
    Q_INVOKABLE QVariantList getMappingsForController(int controllerRow) const;
    // The mapping path already persisted for this controller, for
    // pre-selecting the QML page's dropdown. Empty if none configured.
    Q_INVOKABLE QString getCurrentMappingPath(int controllerRow) const;
    // Loads and applies mappingPath (or clears the mapping if empty) and
    // opens/closes the controller to match `enabled` - the same effect as
    // picking a preset and clicking Apply/OK on the desktop dialog. Returns
    // false (with a qWarning()) if the controller row or mapping file is
    // invalid.
    Q_INVOKABLE bool applyMapping(int controllerRow, const QString& mappingPath, bool enabled);

    // Settings (checkboxes/dropdowns) exposed by the controller's currently
    // loaded mapping via a <settings> block (e.g. the DDJ-FLX2's "Use the
    // Hot Cue pads as accessibility pads" toggle). Each entry is
    // {variable, label, description, type, value}, plus an "options" list
    // of {value, label} when type == "enum". type is one of "boolean",
    // "enum", or "other" - integer/real/color/file settings report their
    // current value as a string but aren't editable from this page yet.
    Q_INVOKABLE QVariantList getMappingSettings(int controllerRow) const;
    // Sets one setting by variable name - value should be a bool for
    // "boolean" settings, or one of that setting's "options" values (a
    // string) for "enum" settings - then persists it and re-applies the
    // mapping so the running script's engine.getSetting() picks up the
    // change immediately (settings are only snapshotted into the JS
    // engine when a mapping is (re)opened). Returns false (with a
    // qWarning()) if the controller row or setting variable isn't found.
    Q_INVOKABLE bool setMappingSetting(int controllerRow, const QString& variable, const QVariant& value);

    // BLE MIDI connect flow (Android only - no-ops/empty elsewhere):
    // Bonded Bluetooth devices as {name, address} entries for the connect
    // UI. Empty if Bluetooth is off/unavailable; also empty when the
    // BLUETOOTH_CONNECT runtime permission hasn't been granted yet (call
    // requestBluetoothPermission() first in that case).
    Q_INVOKABLE QVariantList getBluetoothMidiDevices() const;
    // The address last passed to a successful connectBluetoothMidiDevice()
    // call, persisted across restarts, so the QML page can pre-select it
    // in the device list instead of always defaulting to the first bonded
    // device. Empty if none has ever connected successfully.
    Q_INVOKABLE QString getLastBluetoothMidiAddress() const;
    // Asks Android for BLUETOOTH_CONNECT (no-op if already granted);
    // emits bluetoothPermissionResult(granted) once resolved.
    Q_INVOKABLE void requestBluetoothPermission();
    // Opens the BLE MIDI GATT connection to the bonded device with the
    // given address, requesting the permission first if needed. Emits
    // bluetoothMidiDeviceConnected(success, deviceName) when done; on
    // success the device shows up in the controllers list automatically.
    Q_INVOKABLE void connectBluetoothMidiDevice(const QString& address);
    // Scans for ~8s for advertising BLE MIDI devices (which may not be
    // bonded yet - Android bonds transparently when we connect to them).
    // Emits bluetoothScanFinished(devices) with {name, address} entries.
    Q_INVOKABLE void startBluetoothMidiScan();
    // Reconnects to whichever device the last successful
    // connectBluetoothMidiDevice() call used, if any was persisted. Meant
    // to be called once at startup (see main.qml) so a previously-used BLE
    // MIDI controller doesn't need to be manually reconnected every
    // launch. Silently does nothing if there's no persisted device or if
    // Bluetooth permission hasn't already been granted in a past
    // session - startup shouldn't surface an unsolicited system permission
    // dialog before the user has even opened the Controllers page.
    Q_INVOKABLE void reconnectLastBluetoothMidiDevice();

    static QmlControllerManagerProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);
    static void registerControllerManager(
            std::shared_ptr<ControllerManager> pControllerManager,
            UserSettingsPointer pConfig) {
        s_pControllerManager = std::move(pControllerManager);
        s_pConfig = std::move(pConfig);
    }

  signals:
    // Internal only: connected in the constructor to
    // ControllerManager::slotApplyMapping with a BlockingQueuedConnection,
    // matching how DlgPrefController invokes it - ControllerManager runs on
    // its own thread, so this can't be a plain method call.
    void requestApplyMapping(Controller* pController,
            std::shared_ptr<LegacyControllerMapping> pMapping,
            bool bEnabled);
    // Result of requestBluetoothPermission().
    void bluetoothPermissionResult(bool granted);
    // Result of connectBluetoothMidiDevice(). deviceName is the
    // android.media.midi PROPERTY_NAME of the opened device, if known.
    void bluetoothMidiDeviceConnected(bool success, const QString& deviceName);
    // Result of startBluetoothMidiScan(): {name, address} entries for
    // every advertising BLE MIDI device seen.
    void bluetoothScanFinished(const QVariantList& devices);

  private:
    static inline std::shared_ptr<ControllerManager> s_pControllerManager;
    static inline UserSettingsPointer s_pConfig;

    void openBleMidiDeviceIfPermitted(const QString& address);

    const std::shared_ptr<ControllerManager> m_pControllerManager;
    const UserSettingsPointer m_pConfig;
    QmlControllerListModel* m_pControllerListModel;
    // The address passed to the most recent connectBluetoothMidiDevice()
    // call, persisted to m_pConfig once that connection succeeds so
    // reconnectLastBluetoothMidiDevice() can find it again next launch.
    QString m_lastRequestedBleAddress;
};

} // namespace qml
} // namespace mixxx

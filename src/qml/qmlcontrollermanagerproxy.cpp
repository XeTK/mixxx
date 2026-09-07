#include "qml/qmlcontrollermanagerproxy.h"

#include <QGuiApplication>
#include <QPermissions>

#ifdef Q_OS_ANDROID
#include "controllers/android.h"
#elif defined(Q_OS_IOS)
#include "controllers/midi/blemidipairingios.h"
#endif
#include "controllers/controller.h"
#include "controllers/controllermappinginfo.h"
#include "controllers/controllermappinginfoenumerator.h"
#include "controllers/defs_controllers.h"
#include "controllers/legacycontrollermapping.h"
#include "controllers/legacycontrollermappingfilehandler.h"
#include "controllers/legacycontrollersettings.h"
#include "moc_qmlcontrollermanagerproxy.cpp"
#include "util/assert.h"

namespace {
const QString kBluetoothMidiConfigGroup = QLatin1String("[BluetoothMidi]");
const QString kLastDeviceAddressConfigKey = QLatin1String("last_device_address");
} // namespace

namespace mixxx {
namespace qml {

QmlControllerManagerProxy::QmlControllerManagerProxy(
        std::shared_ptr<ControllerManager> pControllerManager,
        UserSettingsPointer pConfig,
        QObject* parent)
        : QObject(parent),
          m_pControllerManager(pControllerManager),
          m_pConfig(pConfig),
          m_pControllerListModel(new QmlControllerListModel(pControllerManager, this)) {
    // ControllerManager runs on its own thread - a plain method call would
    // touch its internal state from the QML/GUI thread. Route through a
    // signal with a blocking queued connection instead, exactly matching
    // how DlgPrefController invokes the same slot.
    connect(this,
            &QmlControllerManagerProxy::requestApplyMapping,
            m_pControllerManager.get(),
            &ControllerManager::slotApplyMapping,
            Qt::BlockingQueuedConnection);
#ifdef Q_OS_ANDROID
    mixxx::android::setBleMidiResultReceiver([this](bool success, const QString& deviceName) {
        if (success) {
            // The opened BLE device now registers with android.media.midi -
            // re-enumerate so it shows up in the controllers list.
            // ControllerManager runs on its own thread.
            QMetaObject::invokeMethod(m_pControllerManager.get(),
                    &ControllerManager::updateControllerList,
                    Qt::QueuedConnection);
            // Remember this address so reconnectLastBluetoothMidiDevice()
            // can find it again on the next launch.
            m_pConfig->set(ConfigKey(kBluetoothMidiConfigGroup, kLastDeviceAddressConfigKey),
                    ConfigValue(m_lastRequestedBleAddress));
        }
        // Arrives on the Android main thread - hop to this object's thread
        // before emitting, so QML connections don't have to be direct.
        QMetaObject::invokeMethod(this,
                [this, success, deviceName]() {
                    emit bluetoothMidiDeviceConnected(success, deviceName);
                },
                Qt::QueuedConnection);
    });
    mixxx::android::setBleScanResultReceiver([this](const QStringList& devices) {
        QVariantList result;
        for (const QString& entry : devices) {
            const int separatorIndex = entry.lastIndexOf('|');
            if (separatorIndex < 0) {
                continue;
            }
            QVariantMap device;
            device["name"] = entry.left(separatorIndex);
            device["address"] = entry.mid(separatorIndex + 1);
            result.append(device);
        }
        QMetaObject::invokeMethod(this,
                [this, result]() {
                    emit bluetoothScanFinished(result);
                },
                Qt::QueuedConnection);
    });
#elif defined(Q_OS_IOS)
    mixxx::ios::setBluetoothPairingDismissedReceiver([this]() {
        // The paired device now registers with CoreMIDI - re-enumerate so
        // it shows up in the controllers list, the same way the Android
        // branch above does after opening its own BLE MIDI connection.
        // ControllerManager runs on its own thread.
        QMetaObject::invokeMethod(m_pControllerManager.get(),
                &ControllerManager::updateControllerList,
                Qt::QueuedConnection);
        // CABTMIDICentralViewController has no per-device result to report
        // (see blemidipairingios.h) - an empty scan-finished signals QML
        // that the pairing flow ended, without claiming success or
        // failure either way.
        QMetaObject::invokeMethod(this,
                [this]() {
                    emit bluetoothScanFinished({});
                },
                Qt::QueuedConnection);
    });
#endif
}

QmlControllerManagerProxy::~QmlControllerManagerProxy() {
#ifdef Q_OS_ANDROID
    // The receivers capture this - don't leave them dangling.
    mixxx::android::setBleMidiResultReceiver({});
    mixxx::android::setBleScanResultReceiver({});
#elif defined(Q_OS_IOS)
    mixxx::ios::setBluetoothPairingDismissedReceiver({});
#endif
}

QVariantList QmlControllerManagerProxy::getMappingsForController(int controllerRow) const {
    QVariantList result;
    Controller* pController = m_pControllerListModel->controllerAt(controllerRow);
    if (!pController) {
        return result;
    }

    const QString extension = pController->mappingExtension();
    QList<MappingInfo> mappings;
    auto pUserEnumerator = m_pControllerManager->getMainThreadUserMappingEnumerator();
    if (pUserEnumerator) {
        mappings += pUserEnumerator->getMappingsByExtension(extension);
    }
    auto pSystemEnumerator = m_pControllerManager->getMainThreadSystemMappingEnumerator();
    if (pSystemEnumerator) {
        mappings += pSystemEnumerator->getMappingsByExtension(extension);
    }

    for (const MappingInfo& mapping : std::as_const(mappings)) {
        QVariantMap entry;
        entry["name"] = mapping.getName();
        entry["path"] = mapping.getPath();
        result.append(entry);
    }
    return result;
}

QString QmlControllerManagerProxy::getCurrentMappingPath(int controllerRow) const {
    Controller* pController = m_pControllerListModel->controllerAt(controllerRow);
    if (!pController) {
        return QString();
    }
    return m_pControllerManager->getConfiguredMappingFileForDevice(pController->getName());
}

bool QmlControllerManagerProxy::applyMapping(
        int controllerRow, const QString& mappingPath, bool enabled) {
    Controller* pController = m_pControllerListModel->controllerAt(controllerRow);
    if (!pController) {
        qWarning() << "QmlControllerManagerProxy: controller row" << controllerRow
                   << "not found";
        return false;
    }

    std::shared_ptr<LegacyControllerMapping> pMapping;
    if (!mappingPath.isEmpty()) {
        pMapping = LegacyControllerMappingFileHandler::loadMapping(
                QFileInfo(mappingPath), QDir(resourceMappingsPath(m_pConfig)));
        if (!pMapping) {
            qWarning() << "QmlControllerManagerProxy: failed to load mapping"
                       << mappingPath;
            return false;
        }
    }

    emit requestApplyMapping(pController, pMapping, enabled);
    // slotApplyMapping already opened/closed the controller synchronously
    // (BlockingQueuedConnection) by the time emit() returns - force the
    // list model to pick up the new isOpen state, since
    // ControllerManager::devicesChanged() only fires on hotplug/enumeration.
    m_pControllerListModel->slotUpdated();
    return true;
}

QVariantList QmlControllerManagerProxy::getMappingSettings(int controllerRow) const {
    QVariantList result;
    Controller* pController = m_pControllerListModel->controllerAt(controllerRow);
    if (!pController) {
        return result;
    }
    auto pMapping = pController->getMapping();
    if (!pMapping) {
        return result;
    }

    for (const auto& pSetting : pMapping->getSettings()) {
        QVariantMap entry;
        entry["variable"] = pSetting->variableName();
        entry["label"] = pSetting->label();
        entry["description"] = pSetting->description();

        if (auto* pBooleanSetting =
                        dynamic_cast<LegacyControllerBooleanSetting*>(pSetting.get())) {
            entry["type"] = "boolean";
            entry["value"] = pBooleanSetting->value().toBool();
        } else if (auto* pEnumSetting =
                           dynamic_cast<LegacyControllerEnumSetting*>(pSetting.get())) {
            entry["type"] = "enum";
            entry["value"] = pSetting->stringify();
            QVariantList options;
            for (const auto& item : pEnumSetting->options()) {
                QVariantMap option;
                option["value"] = item.value;
                option["label"] = item.label;
                options.append(option);
            }
            entry["options"] = options;
        } else {
            // Integer/real/color/file settings - reported read-only for
            // now, no editor built for them on this page yet.
            entry["type"] = "other";
            entry["value"] = pSetting->stringify();
        }
        result.append(entry);
    }
    return result;
}

bool QmlControllerManagerProxy::setMappingSetting(
        int controllerRow, const QString& variable, const QVariant& value) {
    Controller* pController = m_pControllerListModel->controllerAt(controllerRow);
    if (!pController) {
        qWarning() << "QmlControllerManagerProxy: controller row" << controllerRow
                   << "not found";
        return false;
    }
    auto pMapping = pController->getMapping();
    if (!pMapping) {
        qWarning() << "QmlControllerManagerProxy: controller row" << controllerRow
                   << "has no mapping loaded";
        return false;
    }

    for (const auto& pSetting : pMapping->getSettings()) {
        if (pSetting->variableName() != variable) {
            continue;
        }
        const QString valueString = value.typeId() == QMetaType::Bool
                ? (value.toBool() ? "true" : "false")
                : value.toString();
        bool ok = false;
        pSetting->parse(valueString, &ok);
        if (!ok) {
            qWarning() << "QmlControllerManagerProxy: failed to parse value for setting"
                       << variable;
            return false;
        }
        pMapping->saveSettings(m_pConfig, pController->getName());
        // Controller::applyMapping() only snapshots settings into the
        // running script's JS engine when a mapping is (re)opened - redo
        // that here so engine.getSetting() picks up the new value right
        // away instead of only on the next manual re-enable.
        emit requestApplyMapping(pController, pMapping, pController->isOpen());
        return true;
    }
    qWarning() << "QmlControllerManagerProxy: setting" << variable << "not found";
    return false;
}

QVariantList QmlControllerManagerProxy::getBluetoothMidiDevices() const {
    QVariantList result;
#ifdef Q_OS_ANDROID
    const auto entries = mixxx::android::listBondedBluetoothDevices();
    if (!entries.has_value()) {
        // BLUETOOTH_CONNECT not granted yet - QML should call
        // requestBluetoothPermission() and retry.
        return result;
    }
    for (const QString& entry : *entries) {
        const int separatorIndex = entry.lastIndexOf('|');
        if (separatorIndex < 0) {
            continue;
        }
        QVariantMap device;
        device["name"] = entry.left(separatorIndex);
        device["address"] = entry.mid(separatorIndex + 1);
        result.append(device);
    }
#endif
    return result;
}

QString QmlControllerManagerProxy::getLastBluetoothMidiAddress() const {
    return m_pConfig->getValueString(
            ConfigKey(kBluetoothMidiConfigGroup, kLastDeviceAddressConfigKey));
}

void QmlControllerManagerProxy::requestBluetoothPermission() {
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    // QBluetoothPermission is Qt's own cross-platform permissions API
    // (QtCore, not QtBluetooth) - its iOS backend is a real CoreBluetooth-
    // backed plugin (Qt6QDarwinBluetoothPermissionPlugin), so this check
    // works identically on both platforms with no iOS-specific code here.
    QBluetoothPermission permission;
    switch (qApp->checkPermission(permission)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(permission, this, [this]() {
            const bool granted = qApp->checkPermission(QBluetoothPermission()) ==
                    Qt::PermissionStatus::Granted;
            emit bluetoothPermissionResult(granted);
        });
        return;
    case Qt::PermissionStatus::Granted:
        emit bluetoothPermissionResult(true);
        return;
    case Qt::PermissionStatus::Denied:
        emit bluetoothPermissionResult(false);
        return;
    }
#else
    emit bluetoothPermissionResult(false);
#endif
}

void QmlControllerManagerProxy::openBleMidiDeviceIfPermitted(const QString& address) {
#ifdef Q_OS_ANDROID
    if (mixxx::android::openBluetoothMidiDevice(address)) {
        // Result arrives asynchronously via bluetoothMidiDeviceConnected().
        return;
    }
#elif defined(Q_OS_IOS)
    // There's no discrete per-device "address" to open on iOS - see
    // getBluetoothMidiDevices() and startBluetoothMidiScan() below - so
    // this is only reachable via a stale/unexpected QML call. Fall back to
    // (re-)presenting the system pairing UI rather than silently failing.
    Q_UNUSED(address);
    if (mixxx::ios::presentBluetoothMidiPairingUI()) {
        // Dismissal is reported via bluetoothScanFinished(), same as
        // startBluetoothMidiScan() - there's no separate "connected"
        // signal to raise here.
        return;
    }
#endif
    emit bluetoothMidiDeviceConnected(false, QString());
}

void QmlControllerManagerProxy::connectBluetoothMidiDevice(const QString& address) {
    m_lastRequestedBleAddress = address;
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    QBluetoothPermission permission;
    switch (qApp->checkPermission(permission)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(permission, this, [this, address]() {
            openBleMidiDeviceIfPermitted(address);
        });
        return;
    case Qt::PermissionStatus::Granted:
        openBleMidiDeviceIfPermitted(address);
        return;
    case Qt::PermissionStatus::Denied:
        emit bluetoothMidiDeviceConnected(false, QString());
        return;
    }
#else
    Q_UNUSED(address);
    emit bluetoothMidiDeviceConnected(false, QString());
#endif
}

void QmlControllerManagerProxy::startBluetoothMidiScan() {
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    QBluetoothPermission permission;
    switch (qApp->checkPermission(permission)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(permission, this, [this]() {
            startBluetoothMidiScan();
        });
        return;
    case Qt::PermissionStatus::Granted:
#ifdef Q_OS_ANDROID
        if (mixxx::android::startMidiBleScan()) {
            // Results arrive via bluetoothScanFinished().
            return;
        }
        qWarning() << "QmlControllerManagerProxy: startMidiBleScan() failed to start";
#elif defined(Q_OS_IOS)
        if (mixxx::ios::presentBluetoothMidiPairingUI()) {
            // Dismissal (and the resulting bluetoothScanFinished()) is
            // reported via the receiver set in the constructor.
            return;
        }
        qWarning() << "QmlControllerManagerProxy: no view controller to "
                      "present the Bluetooth MIDI pairing UI over";
#endif
        break;
    case Qt::PermissionStatus::Denied:
        qWarning() << "QmlControllerManagerProxy: Bluetooth permission denied, can't scan";
        break;
    }
#endif
    emit bluetoothScanFinished({});
}

void QmlControllerManagerProxy::reconnectLastBluetoothMidiDevice() {
#ifdef Q_OS_ANDROID
    const QString address = m_pConfig->getValueString(
            ConfigKey(kBluetoothMidiConfigGroup, kLastDeviceAddressConfigKey));
    if (address.isEmpty()) {
        return;
    }
    // Only reconnect if permission was already granted in a past session -
    // startup shouldn't surface an unsolicited system permission dialog
    // before the user has even opened the Controllers preferences page.
    QBluetoothPermission permission;
    if (qApp->checkPermission(permission) == Qt::PermissionStatus::Granted) {
        connectBluetoothMidiDevice(address);
    }
#elif defined(Q_OS_IOS)
    // Unlike Android, iOS/CoreBluetooth reconnects a previously-paired BLE
    // MIDI accessory automatically at the OS level whenever it's in range -
    // no app action (or even a running app) is needed. The one thing worth
    // doing here is re-enumerating once at startup, in case the accessory
    // reconnected before Mixxx's controller list was first built.
    QMetaObject::invokeMethod(m_pControllerManager.get(),
            &ControllerManager::updateControllerList,
            Qt::QueuedConnection);
#endif
}

// static
QmlControllerManagerProxy* QmlControllerManagerProxy::create(
        QQmlEngine* pQmlEngine, QJSEngine* pJsEngine) {
    // The implementation of this method is mostly taken from the code example
    // that shows the replacement for `qmlRegisterSingletonInstance()` when
    // using `QML_SINGLETON`.
    // https://doc.qt.io/qt-6/qqmlengine.html#QML_SINGLETON

    // The instance has to exist before it is used. We cannot replace it.
    VERIFY_OR_DEBUG_ASSERT(s_pControllerManager) {
        qWarning() << "ControllerManager hasn't been registered yet";
        return nullptr;
    }
    return new QmlControllerManagerProxy(s_pControllerManager, s_pConfig, pQmlEngine);
}

} // namespace qml
} // namespace mixxx

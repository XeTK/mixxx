#include "qml/qmlcontrollermanagerproxy.h"

#include <QGuiApplication>
#include <QPermissions>

#include "controllers/android.h"
#include "controllers/controller.h"
#include "controllers/controllermappinginfo.h"
#include "controllers/controllermappinginfoenumerator.h"
#include "controllers/defs_controllers.h"
#include "controllers/legacycontrollermapping.h"
#include "controllers/legacycontrollermappingfilehandler.h"
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
#endif
}

QmlControllerManagerProxy::~QmlControllerManagerProxy() {
#ifdef Q_OS_ANDROID
    // The receivers capture this - don't leave them dangling.
    mixxx::android::setBleMidiResultReceiver({});
    mixxx::android::setBleScanResultReceiver({});
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
#ifdef Q_OS_ANDROID
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
#endif
    emit bluetoothMidiDeviceConnected(false, QString());
}

void QmlControllerManagerProxy::connectBluetoothMidiDevice(const QString& address) {
    m_lastRequestedBleAddress = address;
#ifdef Q_OS_ANDROID
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
#ifdef Q_OS_ANDROID
    QBluetoothPermission permission;
    switch (qApp->checkPermission(permission)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(permission, this, [this]() {
            startBluetoothMidiScan();
        });
        return;
    case Qt::PermissionStatus::Granted:
        if (mixxx::android::startMidiBleScan()) {
            // Results arrive via bluetoothScanFinished().
            return;
        }
        qWarning() << "QmlControllerManagerProxy: startMidiBleScan() failed to start";
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

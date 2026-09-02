#include "qml/qmlcontrollermanagerproxy.h"

#include "controllers/controller.h"
#include "controllers/controllermappinginfo.h"
#include "controllers/controllermappinginfoenumerator.h"
#include "controllers/defs_controllers.h"
#include "controllers/legacycontrollermapping.h"
#include "controllers/legacycontrollermappingfilehandler.h"
#include "moc_qmlcontrollermanagerproxy.cpp"
#include "util/assert.h"

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

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

  private:
    static inline std::shared_ptr<ControllerManager> s_pControllerManager;
    static inline UserSettingsPointer s_pConfig;

    const std::shared_ptr<ControllerManager> m_pControllerManager;
    const UserSettingsPointer m_pConfig;
    QmlControllerListModel* m_pControllerListModel;
};

} // namespace qml
} // namespace mixxx

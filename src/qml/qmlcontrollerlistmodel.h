#pragma once
#include <QAbstractListModel>
#include <QQmlEngine>
#include <memory>

#include "controllers/controllermanager.h"

namespace mixxx {
namespace qml {

/// A flat list of currently-known controllers, backing a QML ListView. Only
/// ever accessed as a child property of Mixxx.ControllerManager - see
/// QmlControllerManagerProxy::controllers.
class QmlControllerListModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(ControllerListModel)
    QML_UNCREATABLE("Only accessible via Mixxx.ControllerManager.controllers")
  public:
    enum Roles {
        IsOpenRole = Qt::UserRole + 1,
    };
    Q_ENUM(Roles)

    explicit QmlControllerListModel(
            std::shared_ptr<ControllerManager> pControllerManager,
            QObject* parent = nullptr);

    QVariant data(const QModelIndex& index, int role) const override;
    int rowCount(const QModelIndex& parent) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariant get(int row) const;

    // Not exposed to QML - used internally by QmlControllerManagerProxy to
    // resolve a QML-selected row back to a real Controller.
    Controller* controllerAt(int row) const;

  public slots:
    // Public (not just connected internally) so QmlControllerManagerProxy
    // can force a refresh right after applyMapping() changes a controller's
    // open state - ControllerManager::devicesChanged() only fires on
    // hotplug/enumeration, not per-controller open/close.
    void slotUpdated();

  private:
    const std::shared_ptr<ControllerManager> m_pControllerManager;
    QList<Controller*> m_controllers;
};

} // namespace qml
} // namespace mixxx

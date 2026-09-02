#include "qml/qmlcontrollerlistmodel.h"

#include "controllers/controller.h"
#include "moc_qmlcontrollerlistmodel.cpp"

namespace mixxx {
namespace qml {

namespace {
const QHash<int, QByteArray> kRoleNames = {
        {Qt::DisplayRole, "display"},
        {QmlControllerListModel::IsOpenRole, "isOpen"},
};
} // namespace

QmlControllerListModel::QmlControllerListModel(
        std::shared_ptr<ControllerManager> pControllerManager, QObject* parent)
        : QAbstractListModel(parent),
          m_pControllerManager(pControllerManager) {
    slotUpdated();
    connect(m_pControllerManager.get(),
            &ControllerManager::devicesChanged,
            this,
            &QmlControllerListModel::slotUpdated);
}

void QmlControllerListModel::slotUpdated() {
    beginResetModel();
    m_controllers = m_pControllerManager->getControllers();
    endResetModel();
}

Controller* QmlControllerListModel::controllerAt(int row) const {
    if (row < 0 || row >= m_controllers.size()) {
        return nullptr;
    }
    return m_controllers.at(row);
}

QVariant QmlControllerListModel::data(const QModelIndex& index, int role) const {
    if (index.row() < 0 || index.row() >= m_controllers.size()) {
        return QVariant();
    }

    Controller* pController = m_controllers.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return pController->getName();
    case IsOpenRole:
        return pController->isOpen();
    default:
        return QVariant();
    }
}

int QmlControllerListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_controllers.size();
}

QHash<int, QByteArray> QmlControllerListModel::roleNames() const {
    return kRoleNames;
}

QVariant QmlControllerListModel::get(int row) const {
    QVariantMap dataMap;
    const QModelIndex idx = index(row, 0);
    if (!idx.isValid()) {
        return dataMap;
    }

    for (auto it = kRoleNames.constBegin(); it != kRoleNames.constEnd(); it++) {
        dataMap.insert(it.value(), data(idx, it.key()));
    }
    return dataMap;
}

} // namespace qml
} // namespace mixxx

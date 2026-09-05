#include "qml/qmlsounddevicemodel.h"

#include "moc_qmlsounddevicemodel.cpp"

namespace mixxx {
namespace qml {

namespace {
const int kChannelCountRole = Qt::UserRole;
const QHash<int, QByteArray> kRoleNames = {
        {Qt::DisplayRole, "display"},
        {kChannelCountRole, "channelCount"},
};
} // namespace

QmlSoundDeviceModel::QmlSoundDeviceModel(QObject* parent)
        : QAbstractListModel(parent) {
}

void QmlSoundDeviceModel::setDevices(const QList<SoundDevicePointer>& devices) {
    beginResetModel();
    m_devices = devices;
    endResetModel();
}

SoundDevicePointer QmlSoundDeviceModel::deviceAt(int row) const {
    if (row < 0 || row >= m_devices.size()) {
        return SoundDevicePointer();
    }
    return m_devices.at(row);
}

QVariant QmlSoundDeviceModel::data(const QModelIndex& index, int role) const {
    if (index.row() < 0 || index.row() >= m_devices.size()) {
        return QVariant();
    }

    switch (role) {
    case Qt::DisplayRole:
        return m_devices.at(index.row())->getDisplayName();
    case kChannelCountRole:
        return static_cast<int>(m_devices.at(index.row())->getNumOutputChannels());
    default:
        return QVariant();
    }
}

int QmlSoundDeviceModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_devices.size();
}

QHash<int, QByteArray> QmlSoundDeviceModel::roleNames() const {
    return kRoleNames;
}

QVariant QmlSoundDeviceModel::get(int row) const {
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

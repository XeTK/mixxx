#pragma once
#include <QAbstractListModel>
#include <QQmlEngine>

#include "soundio/sounddevice.h"

namespace mixxx {
namespace qml {

/// A flat list of SoundDevices for one audio API/direction, backing a QML
/// ComboBox/ListView. Only ever accessed as a child property of
/// Mixxx.SoundManager - see QmlSoundManagerProxy::outputDevices/inputDevices.
class QmlSoundDeviceModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(SoundDeviceModel)
    QML_UNCREATABLE("Only accessible via Mixxx.SoundManager.outputDevices/inputDevices")
  public:
    explicit QmlSoundDeviceModel(QObject* parent = nullptr);

    QVariant data(const QModelIndex& index, int role) const override;
    int rowCount(const QModelIndex& parent) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariant get(int row) const;

    // Not exposed to QML - used internally by QmlSoundManagerProxy to
    // resolve a QML-selected row back to a real device for applyConfig().
    void setDevices(const QList<SoundDevicePointer>& devices);
    SoundDevicePointer deviceAt(int row) const;

  private:
    QList<SoundDevicePointer> m_devices;
};

} // namespace qml
} // namespace mixxx

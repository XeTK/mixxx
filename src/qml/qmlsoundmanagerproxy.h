#pragma once
#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

#include "qml/qmlsounddevicemodel.h"
#include "soundio/soundmanager.h"

namespace mixxx {
namespace qml {

class QmlSoundManagerProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(mixxx::qml::QmlSoundDeviceModel* outputDevices
                    MEMBER m_pOutputDeviceModel CONSTANT);
    Q_PROPERTY(mixxx::qml::QmlSoundDeviceModel* inputDevices
                    MEMBER m_pInputDeviceModel CONSTANT);
    QML_NAMED_ELEMENT(SoundManager)
    QML_SINGLETON

  public:
    explicit QmlSoundManagerProxy(
            std::shared_ptr<SoundManager> pSoundManager,
            QObject* parent = nullptr);

    Q_INVOKABLE QStringList getHostApiList() const;
    // Repopulates outputDevices/inputDevices for the given API. Call this
    // whenever QML's API dropdown selection changes.
    Q_INVOKABLE void refreshDevicesForApi(const QString& api);
    Q_INVOKABLE QVariantList getSampleRates(const QString& api) const;
    // { api, sampleRate, bufferSizeIndex, outputDeviceName, inputDeviceName,
    //   headphoneDeviceName } for pre-selecting the QML page's controls on
    // load.
    Q_INVOKABLE QVariantMap getCurrentConfig() const;
    // outputDeviceRow/inputDeviceRow/headphoneDeviceRow index into
    // whichever device list refreshDevicesForApi(api) last populated
    // (headphoneDeviceRow indexes the same outputDevices list as
    // outputDeviceRow); pass -1 for "no input device" / "no separate
    // headphone/cue output". Returns whether SoundManager::setConfig()
    // actually succeeded - on failure, call getLastErrorMessage() for why.
    Q_INVOKABLE bool applyConfig(
            const QString& api,
            int outputDeviceRow,
            int inputDeviceRow,
            int headphoneDeviceRow,
            int sampleRate,
            int bufferSizeIndex);
    Q_INVOKABLE QString getLastErrorMessage() const;

    static QmlSoundManagerProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);
    static void registerSoundManager(std::shared_ptr<SoundManager> pSoundManager) {
        s_pSoundManager = std::move(pSoundManager);
    }

  private:
    static inline std::shared_ptr<SoundManager> s_pSoundManager;

    const std::shared_ptr<SoundManager> m_pSoundManager;
    QmlSoundDeviceModel* m_pOutputDeviceModel;
    QmlSoundDeviceModel* m_pInputDeviceModel;
    SoundDeviceStatus m_lastStatus;
};

} // namespace qml
} // namespace mixxx

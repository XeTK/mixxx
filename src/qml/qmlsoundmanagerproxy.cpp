#include "qml/qmlsoundmanagerproxy.h"

#include "moc_qmlsoundmanagerproxy.cpp"
#include "util/assert.h"

namespace mixxx {
namespace qml {

QmlSoundManagerProxy::QmlSoundManagerProxy(
        std::shared_ptr<SoundManager> pSoundManager, QObject* parent)
        : QObject(parent),
          m_pSoundManager(pSoundManager),
          m_pOutputDeviceModel(new QmlSoundDeviceModel(this)),
          m_pInputDeviceModel(new QmlSoundDeviceModel(this)),
          m_lastStatus(SoundDeviceStatus::Ok) {
    // Populate for whatever API is currently configured, so the device
    // models aren't empty before QML explicitly picks an API.
    refreshDevicesForApi(m_pSoundManager->getConfig().getAPI());
}

QStringList QmlSoundManagerProxy::getHostApiList() const {
    QStringList apis;
    for (const QString& api : m_pSoundManager->getHostAPIList()) {
        apis.append(api);
    }
    return apis;
}

void QmlSoundManagerProxy::refreshDevicesForApi(const QString& api) {
    m_pOutputDeviceModel->setDevices(m_pSoundManager->getDeviceList(api, true, false));
    m_pInputDeviceModel->setDevices(m_pSoundManager->getDeviceList(api, false, true));
}

QVariantList QmlSoundManagerProxy::getSampleRates(const QString& api) const {
    QVariantList rates;
    for (const auto& rate : m_pSoundManager->getSampleRates(api)) {
        rates.append(static_cast<int>(rate.value()));
    }
    return rates;
}

QVariantMap QmlSoundManagerProxy::getCurrentConfig() const {
    const SoundManagerConfig config = m_pSoundManager->getConfig();

    QString outputDeviceName;
    const auto outputs = config.getOutputs();
    for (auto it = outputs.constBegin(); it != outputs.constEnd(); ++it) {
        if (it.value().getType() == AudioPathType::Main) {
            outputDeviceName = it.key().name;
            break;
        }
    }

    QString inputDeviceName;
    const auto inputs = config.getInputs();
    if (!inputs.isEmpty()) {
        inputDeviceName = inputs.constBegin().key().name;
    }

    QVariantMap result;
    result["api"] = config.getAPI();
    result["sampleRate"] = static_cast<int>(config.getSampleRate().value());
    result["bufferSizeIndex"] = static_cast<int>(config.getAudioBufferSizeIndex());
    result["outputDeviceName"] = outputDeviceName;
    result["inputDeviceName"] = inputDeviceName;
    return result;
}

bool QmlSoundManagerProxy::applyConfig(
        const QString& api,
        int outputDeviceRow,
        int inputDeviceRow,
        int sampleRate,
        int bufferSizeIndex) {
    SoundManagerConfig config(m_pSoundManager.get());
    config.setAPI(api);
    config.setSampleRate(mixxx::audio::SampleRate(
            static_cast<mixxx::audio::SampleRate::value_t>(sampleRate)));
    config.setAudioBufferSizeIndex(static_cast<unsigned int>(bufferSizeIndex));

    const SoundDevicePointer pOutputDevice = m_pOutputDeviceModel->deviceAt(outputDeviceRow);
    if (pOutputDevice) {
        config.addOutput(pOutputDevice->getDeviceId(),
                AudioOutput(AudioPathType::Main,
                        0,
                        mixxx::audio::ChannelCount::stereo(),
                        0));
    }

    if (inputDeviceRow >= 0) {
        const SoundDevicePointer pInputDevice = m_pInputDeviceModel->deviceAt(inputDeviceRow);
        if (pInputDevice) {
            config.addInput(pInputDevice->getDeviceId(),
                    AudioInput(AudioPathType::Microphone,
                            0,
                            mixxx::audio::ChannelCount::stereo(),
                            0));
        }
    }

    m_lastStatus = m_pSoundManager->setConfig(config);
    return m_lastStatus == SoundDeviceStatus::Ok;
}

QString QmlSoundManagerProxy::getLastErrorMessage() const {
    return m_pSoundManager->getLastErrorMessage(m_lastStatus);
}

// static
QmlSoundManagerProxy* QmlSoundManagerProxy::create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine) {
    // The implementation of this method is mostly taken from the code example
    // that shows the replacement for `qmlRegisterSingletonInstance()` when
    // using `QML_SINGLETON`.
    // https://doc.qt.io/qt-6/qqmlengine.html#QML_SINGLETON

    // The instance has to exist before it is used. We cannot replace it.
    VERIFY_OR_DEBUG_ASSERT(s_pSoundManager) {
        qWarning() << "SoundManager hasn't been registered yet";
        return nullptr;
    }
    return new QmlSoundManagerProxy(s_pSoundManager, pQmlEngine);
}

} // namespace qml
} // namespace mixxx

#include "controllers/midi/androidmidienumerator.h"

#include <QJniArray>
#include <QJniObject>
#include <QtCore/qnativeinterface.h>

#include "controllers/midi/androidmidicontroller.h"
#include "moc_androidmidienumerator.cpp"

AndroidMidiEnumerator::AndroidMidiEnumerator() = default;

AndroidMidiEnumerator::~AndroidMidiEnumerator() {
    qDeleteAll(m_devices);
}

QList<Controller*> AndroidMidiEnumerator::queryDevices() {
    qDeleteAll(m_devices);
    m_devices.clear();

    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        return m_devices;
    }

    QJniObject MIDI_SERVICE = QJniObject::getStaticObjectField(
            "android/content/Context", "MIDI_SERVICE", "Ljava/lang/String;");
    QJniObject midiManager = context.callObjectMethod("getSystemService",
            "(Ljava/lang/String;)Ljava/lang/Object;",
            MIDI_SERVICE.object());
    if (!midiManager.isValid()) {
        return m_devices;
    }

    auto deviceInfos = midiManager.callMethod<QJniArray<QJniObject>>(
            "getDevices", "()[Landroid/media/midi/MidiDeviceInfo;");

    QJniObject PROPERTY_NAME = QJniObject::getStaticObjectField(
            "android/media/midi/MidiDeviceInfo", "PROPERTY_NAME", "Ljava/lang/String;");

    for (const auto& deviceInfo : deviceInfos) {
        const int inputPortCount = deviceInfo->callMethod<jint>("getInputPortCount");
        const int outputPortCount = deviceInfo->callMethod<jint>("getOutputPortCount");
        if (inputPortCount == 0 && outputPortCount == 0) {
            // Not usable as a MIDI controller either direction.
            continue;
        }

        QJniObject properties = deviceInfo->callObjectMethod(
                "getProperties", "()Landroid/os/Bundle;");
        QString name;
        if (properties.isValid()) {
            QJniObject nameObj = properties.callObjectMethod("getString",
                    "(Ljava/lang/String;)Ljava/lang/String;",
                    PROPERTY_NAME.object());
            if (nameObj.isValid()) {
                name = nameObj.toString();
            }
        }
        if (name.isEmpty()) {
            name = QStringLiteral("Android MIDI Device");
        }

        m_devices.push_back(new AndroidMidiController(name, *deviceInfo));
    }

    return m_devices;
}

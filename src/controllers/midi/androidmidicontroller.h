#pragma once

#include <QJniObject>

#include <condition_variable>
#include <mutex>

#include "controllers/android.h"
#include "controllers/midi/midibytestreamparser.h"
#include "controllers/midi/midicontroller.h"
#include "util/duration.h"

/// MIDI backend for Android, using the platform's android.media.midi API
/// (via a small Java bridge, org/mixxx/MidiDeviceBridge - see
/// packaging/android/src/org/mixxx/MidiDeviceBridge.java) instead of
/// PortMidi, which is unavailable on Android (it has no ALSA sequencer
/// or equivalent exposed to apps). This is what makes a class-compliant
/// USB MIDI DJ controller (e.g. a Pioneer DDJ-400) usable when plugged
/// into an Android phone via USB-OTG.
///
/// Unlike PortMidiController, which polls PortMidi's queue on a timer,
/// this backend is event-driven: android.media.midi delivers incoming
/// bytes via a callback on an arbitrary Java-side thread (see
/// MidiDeviceCallback in controllers/android.h for how that's safely
/// routed back to a possibly-already-destroyed controller), which are
/// fed through MidiByteStreamParser to reconstruct discrete messages
/// before handing them to MidiController's usual receivedShortMessage()/
/// receive().
///
/// Note on port direction naming: Android's MidiDeviceInfo names ports
/// from the *device's* point of view (an "input port" is where the
/// device receives data, i.e. where Mixxx sends to), whereas Mixxx's
/// Controller::isInputDevice()/isOutputDevice() are named from Mixxx's
/// own point of view (an "input device" is one Mixxx receives from).
/// The two are therefore inverses of each other - see the constructor.
class AndroidMidiController : public MidiController, public mixxx::android::MidiDeviceCallback {
    Q_OBJECT
  public:
    /// deviceInfo is an android.media.midi.MidiDeviceInfo instance, as
    /// returned by MidiManager.getDevices() (see AndroidMidiEnumerator).
    AndroidMidiController(const QString& deviceName, const QJniObject& deviceInfo);
    ~AndroidMidiController() override;

    PhysicalTransportProtocol getPhysicalTransportProtocol() const override {
        // MidiDeviceInfo.getType(): 1 = USB, 2 = Bluetooth (BLE MIDI),
        // 3 = virtual. Default to USB for anything unrecognized - that's
        // what this backend was built for.
        const int type = m_deviceInfo.callMethod<jint>("getType");
        if (type == 2) {
            return PhysicalTransportProtocol::BlueTooth;
        }
        return PhysicalTransportProtocol::USB;
    }
    QString getVendorString() const override {
        return QString();
    }
    QString getProductString() const override {
        return m_deviceName;
    }
    std::optional<uint16_t> getVendorId() const override {
        return std::nullopt;
    }
    std::optional<uint16_t> getProductId() const override {
        return std::nullopt;
    }
    QString getSerialNumber() const override {
        return QString();
    }
    std::optional<uint8_t> getUsbInterfaceNumber() const override {
        return std::nullopt;
    }

    // mixxx::android::MidiDeviceCallback - called from an arbitrary
    // Java-side thread; must only emit signals (see the class comment on
    // MidiDeviceCallback for why nothing else is safe here).
    void onDeviceOpened(bool success) override;
    void onMidiDataReceived(const unsigned char* data, int length) override;

  protected:
    void sendShortMsg(unsigned char status, unsigned char byte1, unsigned char byte2) override;

  private:
    int open(const QString& resourcePath) override;
    int close() override;
    // The sysex data must already contain the start byte 0xf0 and the
    // end byte 0xf7, matching PortMidiController's contract.
    bool sendBytes(const QByteArray& data) override;

  signals:
    // Internal plumbing: re-emitted from onDeviceOpened()/
    // onMidiDataReceived() so Qt's cross-thread queued connection
    // delivers the actual handling on this object's own (controller)
    // thread instead of running it directly on whatever Java-side
    // thread the JNI callback arrived on - the same pattern
    // Hss1394Controller uses for its own foreign-thread callback.
    void androidDeviceOpened(bool success);
    void androidMidiDataReceived(QByteArray data, mixxx::Duration timestamp);

  private slots:
    void handleDeviceOpened(bool success);
    void handleMidiDataReceived(const QByteArray& data, mixxx::Duration timestamp);

  private:
    QString m_deviceName;
    QJniObject m_deviceInfo;
    qint64 m_callbackKey;
    QJniObject m_bridge; // org/mixxx/MidiDeviceBridge instance, valid once open() succeeds

    mixxx::MidiByteStreamParser m_parser;
    // Read by m_parser's callbacks, which run synchronously (and only)
    // within feed() calls made from handleMidiDataReceived() - set just
    // before each feed() call so the callbacks see the timestamp of the
    // buffer currently being parsed.
    mixxx::Duration m_currentMessageTimestamp;

    std::mutex m_openMutex;
    std::condition_variable m_openCond;
    bool m_openCompleted;
    bool m_openSucceeded;
};

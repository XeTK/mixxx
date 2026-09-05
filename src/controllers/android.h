#pragma once

#include <QJniObject>
#include <QString>
#include <qtypes.h>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

struct libusb_context;

namespace mixxx {
namespace android {

const QJniObject& getIntent();
bool waitForPermission(const QJniObject& device);
void usbDeviceAccessResult(QJniObject device, bool granted);

extern std::mutex s_androidLock;
extern std::condition_variable s_grantingWaitCond;
extern std::vector<std::pair<QJniObject, bool>> s_grantingResult;
extern QJniObject s_intent;
extern QJniObject s_usbManager;

/// Implemented by AndroidMidiController; see registerMidiDeviceCallback().
class MidiDeviceCallback {
  public:
    virtual ~MidiDeviceCallback() = default;

    /// May be called from an arbitrary Java-side thread (the Handler
    /// org/mixxx/MidiDeviceBridge posts its MidiManager callbacks
    /// through). Implementations must only queue work (e.g. emit a Qt
    /// signal to be handled via a cross-thread connection) rather than
    /// doing anything synchronously here, since this is invoked while
    /// holding a lock shared with unregisterMidiDeviceCallback() -
    /// calling back into that lock (e.g. via a directly-connected slot
    /// that closes the controller) would deadlock.
    virtual void onDeviceOpened(bool success) = 0;
    virtual void onMidiDataReceived(const unsigned char* data, int length) = 0;
};

/// Registers a callback to receive org/mixxx/MidiDeviceBridge's JNI
/// callbacks for one opened MIDI device, returning an opaque key to pass
/// as that bridge's "native pointer" constructor argument. Callbacks are
/// routed through this registry - looked up by key - rather than the
/// Java side holding a raw pointer to the AndroidMidiController and the
/// native glue reinterpret_cast'ing it back: a MIDI message or open
/// callback can arrive after the controller has already been destroyed
/// (e.g. the device was unplugged, or Mixxx closed it), and a registry
/// lookup safely no-ops in that case instead of touching freed memory.
qint64 registerMidiDeviceCallback(MidiDeviceCallback* pCallback);
void unregisterMidiDeviceCallback(qint64 key);

/// Receives org/mixxx/BleMidi's open results. Invoked on the Android main
/// thread - implementations must only queue work (e.g. emit a Qt signal).
using BleMidiResultReceiver = std::function<void(bool success, const QString& deviceName)>;

/// Receives the finished BLE MIDI scan's "name|address" entries. Invoked on
/// the Android main thread.
using BleScanResultReceiver = std::function<void(const QStringList& devices)>;

/// Sets (or, with an empty function, clears) the receiver for BLE MIDI
/// open results. There is only ever one BLE connect flow at a time, so a
/// single receiver is enough.
void setBleMidiResultReceiver(BleMidiResultReceiver receiver);
/// Same for BLE scan results.
void setBleScanResultReceiver(BleScanResultReceiver receiver);

/// "name|address" entries for every bonded Bluetooth device, for the
/// QML BLE MIDI connect UI. Returns nullopt if the BLUETOOTH_CONNECT
/// runtime permission is missing (the caller should request it), an
/// empty list if Bluetooth is off/unavailable.
std::optional<QStringList> listBondedBluetoothDevices();
/// Initiates the BLE MIDI GATT connection to the bonded device with the
/// given MAC address; the result arrives via the receiver set with
/// setBleMidiResultReceiver(). Returns false if the request couldn't even
/// be made (no callback will follow).
bool openBluetoothMidiDevice(const QString& address);
/// Scans for ~8s for advertising BLE MIDI devices (service UUID
/// 03B80E5A-...); the results arrive via the receiver set with
/// setBleScanResultReceiver(). Returns false if the scan couldn't be
/// started (no callback will follow).
bool startMidiBleScan();

} // namespace android
} // namespace mixxx

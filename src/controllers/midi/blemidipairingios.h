#pragma once

#include <functional>

/// BLE MIDI pairing UI for iOS, using Apple's own CoreAudioKit
/// CABTMIDICentralViewController instead of hand-rolling a CBCentralManager/
/// CBPeripheral GATT client and BLE-MIDI packet parser (timestamp headers,
/// running status, characteristic writes for output, ...) from scratch.
///
/// Once a device is paired through this UI, iOS's CoreMIDI framework itself
/// registers it as a normal MIDI source/destination - the same CoreMIDI API
/// PortMidi's existing (already iOS-ported, see the portmidi vcpkg overlay's
/// ios-support.patch) backend already polls for wired USB/Lightning
/// devices - so it shows up via the ordinary, cross-platform
/// PortMidiEnumerator/PortMidiController path with zero additional MIDI I/O
/// code needed here. This mirrors how Android's equivalent flow (see
/// controllers/android.h's openBluetoothMidiDevice()) also just opens the
/// platform's own BLE MIDI connection and lets its OS-level MIDI framework
/// (android.media.midi) take over from there - the same
/// ControllerManager::updateControllerList() re-enumeration call that
/// picks up Android's newly-opened device also picks up iOS's here.
namespace mixxx {
namespace ios {

/// Invoked once the pairing UI (presented by presentBluetoothMidiPairingUI())
/// is dismissed. There is no per-device "connected" callback to report here -
/// CABTMIDICentralViewController is a fully self-contained system screen
/// with no delegate protocol - so the caller's job is just to re-enumerate
/// MIDI devices (ControllerManager::updateControllerList()) and update its
/// own "is a pairing flow in progress" UI state.
using BluetoothPairingDismissedReceiver = std::function<void()>;

/// Sets (or, with an empty function, clears) the receiver for
/// presentBluetoothMidiPairingUI() dismissal. There is only ever one
/// pairing flow at a time, so a single receiver is enough.
void setBluetoothPairingDismissedReceiver(BluetoothPairingDismissedReceiver receiver);

/// Presents Apple's CABTMIDICentralViewController (CoreAudioKit) modally
/// over the app's current topmost view controller - the standard,
/// App-Store-approved system UI for discovering, pairing, and connecting
/// BLE MIDI accessories (it also lists and can disconnect already-paired
/// ones). Returns false if no view controller could be found to present
/// over (nothing else happens in that case - the caller should treat this
/// like an immediate dismissal).
bool presentBluetoothMidiPairingUI();

} // namespace ios
} // namespace mixxx

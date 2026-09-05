package org.mixxx;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiManager;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelUuid;
import android.util.Log;

import java.io.IOException;
import java.util.Collections;

// Establishes the BLE MIDI GATT connection to an already-bonded Bluetooth
// device from within the app, via MidiManager.openBluetoothDevice() - the
// same mechanism the system's "MIDI BLE Connect"-style helpers use.
//
// Rationale: Android's Settings Bluetooth screen pairs (bonds) BLE MIDI
// devices but is notoriously flaky at *keeping* the MIDI GATT connection
// up, and a device that only appears bonded is invisible to
// MidiManager.getDevices() until something opens it. Opening it from here
// both establishes the connection and registers the device with
// MidiManager, so the existing AndroidMidiEnumerator picks it up like any
// USB MIDI device.
public class BleMidi {
    private static final String TAG = "MixxxBleMidi";

    // Keeps the MidiDevice handle for the most recently opened BLE MIDI
    // device alive. android.media.midi drops a BLE device from getDevices()
    // once its last open MidiDevice is closed, and AndroidMidiController
    // opens its own handle afterwards - this reference must outlive that
    // handoff (and, practically, the session).
    private static MidiDevice sOpenBleDevice;

    // Called on the main thread once openBluetoothMidiDevice() has either
    // succeeded or failed. Registered from C++ (see JNI_OnLoad).
    private static native void onBleMidiOpenResult(boolean success, String deviceName);

    // Returns "name|address" strings for every bonded Bluetooth device, or
    // null if the BLUETOOTH_CONNECT runtime permission (API 31+) is missing.
    public static String[] listBondedBluetoothDevices(Context context) {
        BluetoothManager bluetoothManager =
                (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        if (bluetoothManager == null) {
            return new String[0];
        }
        BluetoothAdapter adapter = bluetoothManager.getAdapter();
        if (adapter == null) {
            return new String[0];
        }
        java.util.Set<BluetoothDevice> bonded;
        try {
            bonded = bluetoothManager.getAdapter().getBondedDevices();
        } catch (SecurityException e) {
            Log.w(TAG, "getBondedDevices() needs BLUETOOTH_CONNECT: " + e);
            return null;
        }
        String[] result = new String[bonded.size()];
        int i = 0;
        for (BluetoothDevice device : bonded) {
            String name;
            try {
                name = device.getName();
            } catch (SecurityException e) {
                name = null;
            }
            if (name == null || name.isEmpty()) {
                name = "Bluetooth device";
            }
            result[i++] = name + "|" + device.getAddress();
        }
        return result;
    }

    // Initiates the BLE MIDI GATT connection to the bonded device with the
    // given MAC address. The result is delivered asynchronously via
    // onBleMidiOpenResult(). Returns false if the device/address is invalid
    // or the MIDI service is unavailable (no callback will follow).
    public static boolean openBluetoothMidiDevice(Context context, String address) {
        MidiManager midiManager = (MidiManager) context.getSystemService(Context.MIDI_SERVICE);
        BluetoothAdapter adapter = null;
        if (midiManager != null) {
            BluetoothManager bluetoothManager =
                    (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
            if (bluetoothManager != null) {
                adapter = bluetoothManager.getAdapter();
            }
        }
        if (midiManager == null || adapter == null) {
            return false;
        }
        BluetoothDevice bluetoothDevice;
        try {
            bluetoothDevice = adapter.getRemoteDevice(address);
        } catch (IllegalArgumentException e) {
            Log.w(TAG, "Invalid Bluetooth address: " + address);
            return false;
        }
        if (bluetoothDevice == null) {
            return false;
        }
        try {
            midiManager.openBluetoothDevice(bluetoothDevice,
                    new MidiManager.OnDeviceOpenedListener() {
                        @Override
                        public void onDeviceOpened(MidiDevice device) {
                            if (device != null) {
                                if (sOpenBleDevice != null) {
                                    try {
                                        sOpenBleDevice.close();
                                    } catch (IOException e) {
                                        Log.w(TAG, "close(old) failed: " + e);
                                    }
                                }
                                sOpenBleDevice = device;
                                String name = "";
                                try {
                                    name = device.getInfo().getProperties().getString(
                                            MidiDeviceInfo.PROPERTY_NAME, "");
                                } catch (Exception e) {
                                    Log.w(TAG, "getInfo() failed: " + e);
                                }
                                onBleMidiOpenResult(true, name);
                            } else {
                                Log.w(TAG, "openBluetoothDevice() failed");
                                onBleMidiOpenResult(false, "");
                            }
                        }
                    },
                    new Handler(Looper.getMainLooper()));
            return true;
        } catch (SecurityException e) {
            Log.w(TAG, "openBluetoothDevice() denied: " + e);
            return false;
        }
    }

    // ---- BLE scanning ----
    // Devices that only advertise (never completed OS bonding) don't show
    // up in getBondedDevices(), but can be connected directly: Android's
    // BluetoothMidiService bonds transparently during the GATT connection.
    // This scan finds advertising BLE MIDI devices (service UUID
    // 03B80E5A-... - the Bluetooth SIG MIDI profile) so the app can offer
    // them for connection without the Settings app.

    private static final android.os.ParcelUuid MIDI_UUID =
            android.os.ParcelUuid.fromString("000003b8-0000-1000-8000-00805f9b34fb");
    private static final long SCAN_DURATION_MS = 8000;

    private static BluetoothLeScanner sScanner;
    private static ScanCallback sScanCallback;
    private static final java.util.LinkedHashMap<String, String> sScanResults =
            new java.util.LinkedHashMap<>();

    // Delivered once when the scan finishes: "name|address" entries for
    // every advertising BLE MIDI device seen. Registered from C++.
    // Object[] rather than String[]: Qt's Q_JNI_NATIVE_METHOD maps
    // jobjectArray to [Ljava/lang/Object;, and a mismatch makes the whole
    // class's native registration fail.
    private static native void onBleScanFinished(Object[] devices);

    public static boolean startMidiBleScan(Context context) {
        Log.i(TAG, "startMidiBleScan() called");
        BluetoothManager bluetoothManager =
                (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        if (bluetoothManager == null) {
            Log.w(TAG, "startMidiBleScan: no BluetoothManager");
            return false;
        }
        BluetoothAdapter adapter = bluetoothManager.getAdapter();
        if (adapter == null) {
            Log.w(TAG, "startMidiBleScan: no BluetoothAdapter");
            return false;
        }
        if (!adapter.isEnabled()) {
            Log.w(TAG, "startMidiBleScan: Bluetooth adapter is disabled");
            return false;
        }
        BluetoothLeScanner scanner;
        try {
            scanner = adapter.getBluetoothLeScanner();
        } catch (SecurityException e) {
            Log.w(TAG, "getBluetoothLeScanner() denied: " + e);
            return false;
        }
        if (scanner == null) {
            Log.w(TAG, "startMidiBleScan: getBluetoothLeScanner() returned null");
            return false;
        }
        stopMidiBleScan();
        sScanner = scanner;
        sScanCallback = new ScanCallback() {
            @Override
            public void onScanResult(int callbackType, ScanResult result) {
                BluetoothDevice device = result.getDevice();
                if (device == null) {
                    return;
                }
                String name;
                try {
                    name = device.getName();
                } catch (SecurityException e) {
                    Log.w(TAG, "getName() denied for " + device.getAddress());
                    return;
                }
                if (name == null || name.isEmpty()) {
                    name = "BLE MIDI device";
                }
                Log.i(TAG, "scan result: " + name + " " + device.getAddress());
                sScanResults.put(device.getAddress(), name + "|" + device.getAddress());
            }

            @Override
            public void onScanFailed(int errorCode) {
                Log.w(TAG, "BLE scan failed, errorCode=" + errorCode);
            }
        };
        try {
            // Unfiltered: many BLE MIDI devices (e.g. Pioneer DDJ-FLX2) don't
            // put the MIDI service UUID in their advertisement packet, so a
            // UUID filter matches nothing. We list everything and let the
            // user pick; connecting is what actually validates the device.
            scanner.startScan(
                    Collections.emptyList(),
                    new android.bluetooth.le.ScanSettings.Builder()
                            .setScanMode(android.bluetooth.le.ScanSettings.SCAN_MODE_LOW_LATENCY)
                            .build(),
                    sScanCallback);
            Log.i(TAG, "BLE scan started (unfiltered)");
        } catch (SecurityException e) {
            Log.w(TAG, "startScan() denied: " + e);
            sScanner = null;
            sScanCallback = null;
            return false;
        }
        new Handler(Looper.getMainLooper()).postDelayed(() -> {
            Object[] found = sScanResults.values().toArray(new Object[0]);
            Log.i(TAG, "BLE scan finished, " + found.length + " device(s)");
            stopMidiBleScan();
            onBleScanFinished(found);
        }, SCAN_DURATION_MS);
        return true;
    }

    public static void stopMidiBleScan() {
        if (sScanner != null && sScanCallback != null) {
            try {
                sScanner.stopScan(sScanCallback);
            } catch (SecurityException e) {
                Log.w(TAG, "stopScan() denied: " + e);
            }
        }
        sScanner = null;
        sScanCallback = null;
        sScanResults.clear();
    }
}

package org.mixxx;

import android.content.Context;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiInputPort;
import android.media.midi.MidiManager;
import android.media.midi.MidiOutputPort;
import android.media.midi.MidiReceiver;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.io.IOException;

// Bridges a single android.media.midi.MidiDevice (one physical/virtual
// USB MIDI controller) to native code. One instance per opened device,
// owned for the lifetime of the corresponding C++ AndroidMidiController.
//
// android.media.midi handles the USB permission dialog itself when
// openDevice() is called for a USB-backed device - unlike raw
// UsbManager/UsbDevice access (see UsbPermission.java, used for HID
// controllers), there is no separate permission broadcast to wait for
// here.
public class MidiDeviceBridge {
    private static final String TAG = "MixxxMidiDeviceBridge";

    private final MidiManager mMidiManager;
    private final long mNativePtr;
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private MidiDevice mDevice;
    private MidiInputPort mInputPort; // Mixxx writes here to send TO the controller
    private MidiOutputPort mOutputPort; // controller writes here; we read to receive FROM it

    // Called once openDevice() has either succeeded (with at least one
    // port opened) or failed.
    private native void onDeviceOpened(long nativePtr, boolean success);
    // Called for each raw chunk of bytes received from the controller.
    // May be an arbitrary fragment of one message, several whole
    // messages, or anything in between - see MidiByteStreamParser.
    private native void onMidiDataReceived(long nativePtr, byte[] data, int offset, int count);

    public MidiDeviceBridge(Context context, long nativePtr) {
        mNativePtr = nativePtr;
        mMidiManager = (MidiManager) context.getSystemService(Context.MIDI_SERVICE);
    }

    public static MidiDeviceInfo[] listDevices(Context context) {
        MidiManager midiManager = (MidiManager) context.getSystemService(Context.MIDI_SERVICE);
        if (midiManager == null) {
            return new MidiDeviceInfo[0];
        }
        return midiManager.getDevices();
    }

    public void open(MidiDeviceInfo info) {
        if (mMidiManager == null) {
            onDeviceOpened(mNativePtr, false);
            return;
        }
        mMidiManager.openDevice(info, new MidiManager.OnDeviceOpenedListener() {
            @Override
            public void onDeviceOpened(MidiDevice device) {
                handleDeviceOpened(device);
            }
        }, mHandler);
    }

    private void handleDeviceOpened(MidiDevice device) {
        if (device == null) {
            Log.w(TAG, "openDevice() failed");
            onDeviceOpened(mNativePtr, false);
            return;
        }
        mDevice = device;
        MidiDeviceInfo.PortInfo[] ports = device.getInfo().getPorts();
        for (MidiDeviceInfo.PortInfo port : ports) {
            if (port.getType() == MidiDeviceInfo.PortInfo.TYPE_INPUT && mInputPort == null) {
                // A device's "input port" is where Mixxx sends data TO it.
                mInputPort = device.openInputPort(port.getPortNumber());
            } else if (port.getType() == MidiDeviceInfo.PortInfo.TYPE_OUTPUT && mOutputPort == null) {
                // A device's "output port" is where it sends data back to
                // Mixxx - connecting a MidiReceiver is how that's read.
                mOutputPort = device.openOutputPort(port.getPortNumber());
                if (mOutputPort != null) {
                    mOutputPort.connect(new MidiReceiver() {
                        @Override
                        public void onSend(byte[] data, int offset, int count, long timestamp) {
                            onMidiDataReceived(mNativePtr, data, offset, count);
                        }
                    });
                }
            }
        }
        final boolean success = mInputPort != null || mOutputPort != null;
        if (!success) {
            Log.w(TAG, "Device opened but exposed no usable input/output port");
        }
        onDeviceOpened(mNativePtr, success);
    }

    // Returns false if there is no input port (e.g. an output-only
    // device) or the underlying send() call failed.
    public boolean send(byte[] data, int offset, int count) {
        if (mInputPort == null) {
            return false;
        }
        try {
            mInputPort.send(data, offset, count);
            return true;
        } catch (IOException e) {
            Log.w(TAG, "send() failed: " + e);
            return false;
        }
    }

    public void close() {
        try {
            if (mInputPort != null) {
                mInputPort.close();
            }
            if (mOutputPort != null) {
                mOutputPort.close();
            }
            if (mDevice != null) {
                mDevice.close();
            }
        } catch (IOException e) {
            Log.w(TAG, "close() failed: " + e);
        }
        mInputPort = null;
        mOutputPort = null;
        mDevice = null;
    }
}

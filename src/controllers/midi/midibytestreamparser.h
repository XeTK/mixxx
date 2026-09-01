#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace mixxx {

/// Incremental parser for a raw MIDI 1.0 byte stream that may be
/// fragmented arbitrarily across calls to feed() - a single message can
/// be split across two calls, multiple messages can arrive in one call,
/// and realtime bytes (MIDI clock, start/stop, active sensing) can be
/// interleaved anywhere, including in the middle of a data byte pair or
/// a SysEx message, without disturbing the message in progress.
///
/// Written for Android's android.media.midi.MidiReceiver.onSend(), which
/// delivers raw byte buffers with exactly these fragmentation properties
/// (unlike PortMidi, which hands Mixxx already-framed 32-bit PmMessages).
/// Kept free of any Android/JNI/Qt dependency so it can be unit-tested
/// entirely on desktop.
class MidiByteStreamParser {
  public:
    struct ShortMessage {
        uint8_t status;
        uint8_t data1;
        uint8_t data2;
    };

    /// Called for each complete channel voice/mode or system common
    /// message. Realtime bytes (0xF8-0xFF) are reported with
    /// data1 == data2 == 0, matching PortMidiController's convention
    /// (see portmidicontroller.cpp).
    using ShortMessageCallback = std::function<void(ShortMessage)>;
    /// Called for each complete SysEx message, including the leading
    /// 0xF0 and trailing 0xF7.
    using SysExCallback = std::function<void(std::vector<uint8_t>)>;

    MidiByteStreamParser(ShortMessageCallback onShortMessage, SysExCallback onSysEx);

    void feed(const uint8_t* data, int length);

  private:
    void processStatusByte(uint8_t status);
    void processDataByte(uint8_t data);
    void endSysEx();

    ShortMessageCallback m_onShortMessage;
    SysExCallback m_onSysEx;

    uint8_t m_runningStatus = 0;
    int m_dataBytesExpected = 0;
    int m_dataBytesReceived = 0;
    uint8_t m_pendingData[2] = {0, 0};
    bool m_inSysEx = false;
    std::vector<uint8_t> m_sysExBuffer;
};

} // namespace mixxx

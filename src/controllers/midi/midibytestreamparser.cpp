#include "controllers/midi/midibytestreamparser.h"

namespace mixxx {

namespace {

// Number of data bytes following a given status byte, per the MIDI 1.0
// spec. Returns -1 for realtime bytes (0xF8-0xFF), which are handled
// separately since they never affect running status or the message
// currently being assembled.
int dataByteCountForStatus(uint8_t status) {
    if (status >= 0xF8) {
        return -1;
    }
    switch (status & 0xF0) {
    case 0x80: // Note Off
    case 0x90: // Note On
    case 0xA0: // Polyphonic Key Pressure
    case 0xB0: // Control Change
    case 0xE0: // Pitch Bend Change
        return 2;
    case 0xC0: // Program Change
    case 0xD0: // Channel Pressure
        return 1;
    case 0xF0:
        switch (status) {
        case 0xF1: // MIDI Time Code Quarter Frame
        case 0xF3: // Song Select
            return 1;
        case 0xF2: // Song Position Pointer
            return 2;
        default: // 0xF4-0xF7 (SysEx handled separately, others undefined)
            return 0;
        }
    default:
        return 0;
    }
}

} // namespace

MidiByteStreamParser::MidiByteStreamParser(
        ShortMessageCallback onShortMessage, SysExCallback onSysEx)
        : m_onShortMessage(std::move(onShortMessage)),
          m_onSysEx(std::move(onSysEx)) {
}

void MidiByteStreamParser::feed(const uint8_t* data, int length) {
    for (int i = 0; i < length; ++i) {
        const uint8_t byte = data[i];
        if (byte >= 0xF8) {
            // Realtime bytes may appear anywhere - including mid-SysEx or
            // between the status and data bytes of another message -
            // without disturbing whatever is already in progress.
            if (m_onShortMessage) {
                m_onShortMessage(ShortMessage{byte, 0, 0});
            }
            continue;
        }
        if (byte & 0x80) {
            processStatusByte(byte);
        } else {
            processDataByte(byte);
        }
    }
}

void MidiByteStreamParser::processStatusByte(uint8_t status) {
    if (status == 0xF0) {
        // A stray, unterminated previous SysEx is discarded - any
        // non-realtime status byte implicitly ends SysEx per spec, but
        // there is nothing sensible to emit without its 0xF7.
        m_inSysEx = true;
        m_sysExBuffer.clear();
        m_sysExBuffer.push_back(status);
        m_runningStatus = 0;
        m_dataBytesReceived = 0;
        return;
    }
    if (status == 0xF7) {
        // End of SysEx.
        if (m_inSysEx) {
            m_sysExBuffer.push_back(status);
            endSysEx();
        }
        // A stray 0xF7 with no preceding 0xF0 is meaningless; ignore.
        return;
    }
    if (m_inSysEx) {
        // Any other non-realtime status byte implicitly terminates an
        // in-progress SysEx message without a proper 0xF7. Discard the
        // partial message and reprocess this byte as the start of a new
        // one.
        m_inSysEx = false;
        m_sysExBuffer.clear();
    }
    m_dataBytesExpected = dataByteCountForStatus(status);
    m_dataBytesReceived = 0;
    if (m_dataBytesExpected == 0) {
        if (m_onShortMessage) {
            m_onShortMessage(ShortMessage{status, 0, 0});
        }
        // System common messages don't carry running status forward.
        m_runningStatus = 0;
    } else {
        m_runningStatus = status;
    }
}

void MidiByteStreamParser::processDataByte(uint8_t data) {
    if (m_inSysEx) {
        m_sysExBuffer.push_back(data);
        return;
    }
    if (m_runningStatus == 0) {
        // Stray data byte with no preceding status (e.g. we started
        // listening mid-stream) - nothing to attach it to.
        return;
    }
    m_pendingData[m_dataBytesReceived] = data;
    ++m_dataBytesReceived;
    if (m_dataBytesReceived >= m_dataBytesExpected) {
        if (m_onShortMessage) {
            m_onShortMessage(ShortMessage{
                    m_runningStatus,
                    m_pendingData[0],
                    m_dataBytesExpected >= 2 ? m_pendingData[1] : uint8_t(0)});
        }
        // Keep m_runningStatus for MIDI running-status compression:
        // a subsequent data byte with no new status byte belongs to
        // another message of the same type.
        m_dataBytesReceived = 0;
    }
}

void MidiByteStreamParser::endSysEx() {
    if (m_onSysEx) {
        m_onSysEx(std::move(m_sysExBuffer));
    }
    m_sysExBuffer.clear();
    m_inSysEx = false;
}

} // namespace mixxx

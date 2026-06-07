#pragma once

#include <QList>
#include <QString>
#include <memory>

class TtsEngine {
  public:
    struct AudioOutputDevice {
        QString id;          // platform-specific token/device ID; empty = system default
        QString displayName; // human-readable label for the UI
    };

    struct Voice {
        QString id;          // platform-specific token ID; empty = system default
        QString displayName; // human-readable label for the UI
    };

    virtual ~TtsEngine() = default;

    // Speak text asynchronously, interrupting any current speech.
    virtual void say(const QString& text) = 0;

    // Route subsequent speech to the given audio device.
    // An empty ID restores the system default output.
    virtual void setOutputDevice(const QString& deviceId) {
        Q_UNUSED(deviceId);
    }

    // Switch to the given voice. An empty ID restores the system default voice.
    virtual void setVoice(const QString& voiceId) {
        Q_UNUSED(voiceId);
    }

    // Set speech rate in the range [-10, 10]; 0 is normal speed.
    virtual void setRate(int rate) {
        Q_UNUSED(rate);
    }

    // Route speech to a specific stereo channel pair within the selected device.
    // 0 = channels 1-2 (default), 1 = channels 3-4, 2 = channels 5-6, etc.
    // Has no effect when the device ID is empty (system default device).
    virtual void setOutputChannel(int channelPair) {
        Q_UNUSED(channelPair);
    }

    static std::unique_ptr<TtsEngine> create();
    static QList<AudioOutputDevice> enumerateOutputDevices();
    static QList<Voice> enumerateVoices();
};

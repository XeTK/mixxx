#pragma once

#include <QList>
#include <QString>
#include <memory>

class TtsEngine {
  public:
    struct AudioOutputDevice {
        QString id;
        QString displayName;
    };

    struct Voice {
        QString id;
        QString displayName;
    };

    virtual ~TtsEngine() = default;

    // Speak text asynchronously, interrupting any current speech.
    virtual void say(const QString& text) = 0;

    virtual void setOutputDevice(const QString& deviceId) {
        Q_UNUSED(deviceId);
    }

    virtual void setVoice(const QString& voiceId) {
        Q_UNUSED(voiceId);
    }

    // Rate in the range [-10, 10]; 0 is normal speed.
    virtual void setRate(int rate) {
        Q_UNUSED(rate);
    }

    virtual void setOutputChannel(int channelPair) {
        Q_UNUSED(channelPair);
    }

    static std::unique_ptr<TtsEngine> create();
    static QList<AudioOutputDevice> enumerateOutputDevices();
    static QList<Voice> enumerateVoices();
};

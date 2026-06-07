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

    virtual ~TtsEngine() = default;

    // Speak text asynchronously, interrupting any current speech.
    virtual void say(const QString& text) = 0;

    // Route subsequent speech to the given device (by ID from enumerateOutputDevices).
    // An empty ID restores the system default output.
    virtual void setOutputDevice(const QString& deviceId) {
        Q_UNUSED(deviceId);
    }

    static std::unique_ptr<TtsEngine> create();
    static QList<AudioOutputDevice> enumerateOutputDevices();
};

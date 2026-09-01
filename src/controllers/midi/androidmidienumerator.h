#pragma once

#include "controllers/midi/midienumerator.h"

/// Discovers MIDI DJ controllers via Android's android.media.midi API -
/// see AndroidMidiController for why this exists instead of the usual
/// cross-platform PortMidiEnumerator (unavailable on Android).
class AndroidMidiEnumerator : public MidiEnumerator {
    Q_OBJECT
  public:
    AndroidMidiEnumerator();
    ~AndroidMidiEnumerator() override;

    QList<Controller*> queryDevices() override;

  private:
    QList<Controller*> m_devices;
};

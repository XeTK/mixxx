#pragma once

#include <QString>
#include <atomic>
#include <memory>

#include "engine/enginesidechaincompressor.h"
#include "util/fifo.h"
#include "util/samplebuffer.h"
#include "util/types.h"

class ControlObject;
class ControlProxy;

/// Injects synthesized speech (accessibility announcements) into the engine
/// output.
///
/// Audio is produced on a non-realtime thread (the TTS synthesizer) and pushed
/// into a lock-free FIFO with writeSamples(). process() is called from the
/// audio callback: it drains the FIFO, uses the speech as the key signal for a
/// sidechain compressor to duck the selected output bus, and mixes the speech
/// on top. Routing between the headphone (cue) and main outputs is selectable
/// via the [Tts],route_to_main control.
///
/// Samples are interleaved stereo at the engine sample rate, matching the
/// engine output buffers.
class EngineTts {
  public:
    enum class Route {
        Headphones = 0, // DJ-only cue mix (default)
        Main = 1,       // audience hears it
    };

    explicit EngineTts(const QString& group);
    ~EngineTts();

    /// Audio-callback side. pMain and pHead are the final interleaved-stereo
    /// main and headphone buffers (pHead may be null if no headphone output is
    /// configured). bufferSize is the number of samples; iFrames == bufferSize / 2.
    void process(CSAMPLE* pMain, CSAMPLE* pHead, std::size_t bufferSize, int iFrames);

    /// Check if TTS is currently enabled. Used by announcements to decide whether
    /// to synthesize audio.
    bool isEnabled() const {
        return m_pEnabled->toBool();
    }

    /// Select which output bus speech is mixed into and ducks. Accepts the
    /// EngineTts::Route value as an int (matching the persisted TtsRoute
    /// setting). Thread-safe; may be called from the GUI thread.
    void setRoute(int route);

    /// Producer side (TTS synthesizer thread). pBuffer is interleaved stereo at
    /// the engine sample rate. Returns the number of samples actually written
    /// (may be less than numSamples if the FIFO is full).
    int writeSamples(const CSAMPLE* pBuffer, int numSamples);

    /// Space available for writeSamples(), in samples.
    int writeAvailable() const {
        return m_fifo.writeAvailable();
    }

    /// True when no speech is queued. Producer-safe; used by the synthesizer to
    /// wait for the queue to drain before writing a new utterance.
    bool isEmpty() const {
        return m_fifo.readAvailable() == 0;
    }

    /// Request that any queued speech be discarded when a new utterance
    /// interrupts the current one (barge-in). Producer-safe: the discard itself
    /// happens on the audio thread in process(), keeping the FIFO single-reader.
    void requestFlush() {
        m_flushRequested.store(true, std::memory_order_release);
    }

  private:
    void updateDuckingParameters(double sampleRate);

    FIFO<CSAMPLE> m_fifo;
    mixxx::SampleBuffer m_tts;
    EngineSideChainCompressor m_ducking;
    CSAMPLE_GAIN m_duckGainOld;
    double m_lastSampleRate = 0;
    double m_lastDuckStrength = -1;
    std::atomic<bool> m_flushRequested{false};

    std::unique_ptr<ControlObject> m_pEnabled;     // read-only "is speaking" status
    std::unique_ptr<ControlObject> m_pRouteToMain; // 0 = headphones, 1 = main
    std::unique_ptr<ControlObject> m_pDuckStrength;
    ControlProxy* m_pSampleRate;
};

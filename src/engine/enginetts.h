#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
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
class EngineTts : public QObject {
    Q_OBJECT
  public:
    enum class Route {
        Headphones = 0, // DJ-only cue mix (default)
        Main = 1,       // audience hears it
    };

    explicit EngineTts(const QString& group);
    ~EngineTts() override;

    /// Audio-callback side. pMain and pHead are the final interleaved-stereo
    /// main and headphone buffers (pHead may be null if no headphone output is
    /// configured). bufferSize is the number of samples; iFrames == bufferSize / 2.
    void process(CSAMPLE* pMain, CSAMPLE* pHead, std::size_t bufferSize, int iFrames);

    /// True when the user has TTS turned on. Used by announcements to decide
    /// whether to synthesize audio. Distinct from isSpeaking(), which reflects
    /// whether the FIFO currently has data.
    bool isUserEnabled() const;

    /// True when the engine is actively mixing synthesized speech (FIFO non-empty).
    /// Written every audio callback by process(); read-only for everything else.
    bool isSpeaking() const;

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

    /// --tts-log audibility instrumentation (see util/ttslog.h). The
    /// synthesizer thread brackets the samples belonging to one utterance with
    /// beginUtterance()/endUtterance() so the audio callback can tell, as it
    /// drains the FIFO, whether every sample of that utterance was actually
    /// mixed into the output (COMPLETED) or thrown away by a barge-in flush
    /// (FLUSHED).
    ///
    /// Producer side (same thread as writeSamples()). No-ops unless the
    /// --tts-log hook is active and utteranceId is non-zero, so a normal run
    /// pays for one predictable branch.
    void beginUtterance(quint64 utteranceId);
    void endUtterance(quint64 utteranceId);

    /// Drains the outcomes the audio callback recorded and writes them to the
    /// TTS log. Runs on a timer on the thread that constructed this object;
    /// never on the audio callback. Public so tests can pump it directly
    /// without an event loop.
    void pollAudibilityEvents();

  signals:
    // Emitted from the destructor before any member is destroyed, so owners of
    // a raw EngineTts* (the AnnouncementManager) can drop their pointer and
    // stop calling into a torn-down sink. See ~EngineTts().
    void sinkDestroyed();

  private:
    void updateDuckingParameters(double sampleRate);

    // --- --tts-log audibility instrumentation -----------------------------
    //
    // Two extra lock-free single-producer/single-consumer FIFOs carry the
    // information across thread boundaries, because none of this may happen on
    // the audio callback:
    //
    //   m_utteranceSpans  synthesizer thread -> audio callback
    //       "samples [startSample, endSample) of the write stream are
    //        utterance <id>". Positions are absolute counts of samples ever
    //        written, so they stay valid regardless of ring wrap-around.
    //
    //   m_audibilityEvents  audio callback -> pollAudibilityEvents()
    //       "utterance <id> finished, completed=<0|1>". The audio thread only
    //       ever does an integer compare and a bounded ring write here: no
    //       allocation, no lock, no I/O.
    //
    // A span is retired once the consumed-sample counter reaches its
    // endSample. If a discard (barge-in flush, or the user toggling TTS off
    // mid-utterance) landed inside the span, the utterance was not fully
    // heard and is reported FLUSHED rather than COMPLETED.
    struct UtteranceSpan {
        quint64 id;
        quint64 startSample;
        quint64 endSample;
    };
    struct AudibilityEvent {
        quint64 id;
        quint64 completed; // 1 = fully mixed into the output, 0 = discarded
    };

    /// Audio callback only. Advances the consumed-sample counter and retires
    /// any utterance spans it has passed. `discarded` is true when the samples
    /// were dropped on the floor rather than mixed into the output.
    void noteSamplesConsumed(int numSamples, bool discarded);

    FIFO<CSAMPLE> m_fifo;
    mixxx::SampleBuffer m_tts;
    EngineSideChainCompressor m_ducking;
    CSAMPLE_GAIN m_duckGainOld;
    double m_lastSampleRate = 0;
    double m_lastDuckStrength = -1;
    std::atomic<bool> m_flushRequested{false};

    // True only when --tts-log is active; gates every instrumentation branch
    // so a normal run is unaffected. Const after construction.
    const bool m_logAudibility;

    FIFO<UtteranceSpan> m_utteranceSpans;
    FIFO<AudibilityEvent> m_audibilityEvents;
    std::unique_ptr<QTimer> m_pAudibilityPollTimer;

    // Producer-thread (synthesizer) state. Only ever touched by the producer,
    // but relaxed atomics rather than plain members because macOS delivers
    // AVSpeechSynthesizer buffers on an arbitrary (if serialized) thread, so
    // successive utterances can be produced from different threads.
    std::atomic<quint64> m_samplesWritten{0};
    std::atomic<quint64> m_openUtteranceId{0};
    std::atomic<quint64> m_openUtteranceStart{0};

    // Audio-callback state.
    quint64 m_samplesConsumed{0};
    quint64 m_lastDiscardSample{0};
    UtteranceSpan m_currentSpan{0, 0, 0};
    bool m_haveCurrentSpan{false};

    std::unique_ptr<ControlObject> m_pEnabled;     // user toggle: 1 = TTS on, 0 = TTS off
    std::unique_ptr<ControlObject> m_pSpeaking;    // read-only: 1 while FIFO has data
    std::unique_ptr<ControlObject> m_pRouteToMain; // 0 = headphones, 1 = main
    std::unique_ptr<ControlObject> m_pDuckStrength;
    std::unique_ptr<ControlProxy> m_pSampleRate;
};

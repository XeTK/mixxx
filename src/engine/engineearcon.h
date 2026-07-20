#pragma once

#include <array>
#include <memory>

#include "util/fifo.h"
#include "util/types.h"

class ControlPotmeter;
class ControlProxy;

/// Accessibility earcons: short percussive tones that stand in for spoken
/// announcements of frequent transport events (play, stop, end of track,
/// headphone cue). A rendered gesture starts within one audio buffer, versus
/// the ~100 ms+ latency of synthesized speech, and does not tie up the speech
/// channel — so a blind DJ gets instant "it registered" feedback.
///
/// Sounds are deck-panned (deck 1 left, deck 2 right) to match the beat-click
/// and split-cue geography, so the ear that a sound comes from identifies the
/// deck for free.
///
/// The GUI thread calls trigger(); the audio thread drains the queue in
/// process() and synthesizes the tones. Everything crossing the thread
/// boundary goes through a lock-free FIFO.
class EngineEarcon {
  public:
    enum class Id {
        Play = 0,
        Stop,
        EndOfTrack,
        CueOn,
        CueOff,
        Restart,
        LoopOn,
        LoopOff,
        Clipping,
        CuePreview,
    };
    enum class Pan {
        Left = 0,
        Right = 1,
        Center = 2,
    };

    EngineEarcon();
    ~EngineEarcon();

    /// GUI-thread side: queue an earcon for playback. Lock-free; drops the
    /// request if the queue is momentarily full (never blocks the GUI).
    void trigger(Id id, Pan pan);

    /// Audio-callback side. Buffers are interleaved stereo, iFrames frames.
    /// pHead may be null when no headphone output is configured; sounds then
    /// fall back to the main output like the other accessibility signals.
    void process(CSAMPLE* pMain, CSAMPLE* pHead, int iFrames);

  private:
    // One sine grain of an earcon gesture. posFrames starts negative when the
    // grain is scheduled later within the gesture and counts up each callback;
    // the grain sounds while posFrames is in [0, durFrames).
    struct Voice {
        bool active = false;
        double freqHz = 0.0;
        double posFrames = 0.0;
        double durFrames = 0.0;
        int channel = 0; // 0 = left, 1 = right, 2 = both
    };

    struct Trigger {
        int id;
        int pan;
    };

    void spawn(Id id, Pan pan, double sampleRate);

    static constexpr int kMaxVoices = 24;
    std::array<Voice, kMaxVoices> m_voices;
    FIFO<Trigger> m_fifo;
    std::unique_ptr<ControlPotmeter> m_pVolume;
    std::unique_ptr<ControlProxy> m_pSampleRate;
    std::unique_ptr<ControlProxy> m_pRouteToMain;
};

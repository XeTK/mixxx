#pragma once

#include <QString>
#include <memory>
#include <vector>

#include "util/types.h"

class ControlObject;
class ControlPotmeter;
class ControlProxy;
class ControlPushButton;

/// Accessibility metronome: renders a short click on every beat of each
/// playing deck so a blind DJ can hear the beat grid (for checking gridding
/// and learning to beatmatch). Deck 1 clicks in the left channel, deck 2 in
/// the right, so both grids can be followed at once. Every fourth beat is
/// accented at a higher pitch as a bar marker (assumes 4/4; the bar phase is
/// counted from when the deck starts playing).
///
/// Clicks follow the speech output route ([Tts],route_to_main): the
/// headphone bus by default (falling back to main when no headphone output
/// is configured), or the main output when speech is routed there — so the
/// clicks are always audible wherever the DJ actually hears announcements.
/// Toggled with [BeatClick],enabled (keyboard: Alt+B); level via the
/// persistent [BeatClick],volume control.
///
/// process() is called from the audio callback; everything it touches is
/// lock-free (control atomics and per-deck POD state).
///
/// Per-deck controls (play/bpm/beat_distance) are added via addDeck() rather
/// than passed to the constructor: EngineBeatClick is constructed as part of
/// EngineMixer's own construction, which happens before PlayerManager creates
/// any deck, so binding a ControlProxy to e.g. "[Channel1],play" at
/// construction time would permanently latch onto the AllowMissingOrInvalid
/// default control (silently, since ControlProxy never re-binds) instead of
/// the real one created moments later - reading "not playing" forever
/// regardless of actual deck state. addDeck() is called from
/// EngineMixer::addChannel() once the channel (and its controls) genuinely
/// exist.
class EngineBeatClick {
  public:
    EngineBeatClick();
    ~EngineBeatClick();

    /// Start tracking a deck's play/bpm/beat_distance controls. Call only
    /// once the deck's own controls already exist (i.e. after the channel
    /// has been added to the engine). channel selects the ear: 0 = left,
    /// 1 = right.
    void addDeck(const QString& group, int channel);

    /// Audio-callback side. Buffers are interleaved stereo, iFrames frames.
    /// pHead may be null when no headphone output is configured.
    void process(CSAMPLE* pMain, CSAMPLE* pHead, int iFrames);

  private:
    // Sentinel for "no click sounding or scheduled". Far below any real
    // scheduled offset (which is at most one buffer of frames negative).
    static constexpr double kClickIdle = -1.0e12;

    struct Deck {
        std::unique_ptr<ControlProxy> pPlay;
        std::unique_ptr<ControlProxy> pBeatDistance;
        std::unique_ptr<ControlProxy> pBpm;
        int channel = 0;
        // Frames until the next scheduled click; < 0 means unscheduled.
        double framesToNextClick = -1.0;
        // Frames rendered since the last click, to reject double-fires.
        double framesSinceClick = 1.0e9;
        // Position in the currently sounding click, in frames. Negative
        // values within one buffer mean "starts mid-buffer"; kClickIdle
        // means idle.
        double clickPos = kClickIdle;
        bool clickAccented = false;
        int beatCount = 0;
        bool wasPlaying = false;
    };

    void renderClicks(Deck* pDeck, CSAMPLE* pOut, int iFrames, double sampleRate);

    std::vector<Deck> m_decks;
    std::unique_ptr<ControlPushButton> m_pEnabled;
    std::unique_ptr<ControlPotmeter> m_pVolume;
    std::unique_ptr<ControlProxy> m_pSampleRate;
    std::unique_ptr<ControlProxy> m_pRouteToMain;
};

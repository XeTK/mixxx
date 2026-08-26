#include "engine/engineearcon.h"

#include <cmath>

#include "control/controlpotmeter.h"
#include "control/controlproxy.h"

namespace {
const QString kGroup = QStringLiteral("[Earcon]");

// A single tone within a gesture: frequency, when it starts relative to the
// trigger, and how long it sounds. Kept short and percussive so the sounds
// cut through music without needing to duck it.
struct Grain {
    double freqHz;
    double startMs;
    double durMs;
};

// Percussive gestures. Direction encodes meaning: rising = start/engage,
// falling = stop/disengage; end-of-track is three urgent pips; restart is a
// double-tap distinct from the rising/falling family. Pitches sit in a
// mid-high niche (500-1300 Hz) that stays audible over a mix, with each
// event pair using its own pitch class so gestures don't get confused.
const Grain kPlay[] = {{587.0, 0.0, 55.0}, {880.0, 50.0, 65.0}};
const Grain kStop[] = {{880.0, 0.0, 55.0}, {587.0, 50.0, 70.0}};
const Grain kEndOfTrack[] = {
        {1175.0, 0.0, 45.0}, {1175.0, 70.0, 45.0}, {1175.0, 140.0, 55.0}};
const Grain kCueOn[] = {{784.0, 0.0, 60.0}};
const Grain kCueOff[] = {{523.0, 0.0, 70.0}};
// Two identical short pips: a "tuk-tuk" rewind feel, distinct from the
// rising/falling pairs used for state toggles.
const Grain kRestart[] = {{659.0, 0.0, 35.0}, {659.0, 45.0, 35.0}};
const Grain kLoopOn[] = {{698.0, 0.0, 55.0}, {1047.0, 50.0, 65.0}};
const Grain kLoopOff[] = {{1047.0, 0.0, 55.0}, {698.0, 50.0, 70.0}};
// A low, urgent double-buzz distinct from every other gesture's mid-high
// register, so a clipping warning can never be mistaken for a routine cue.
const Grain kClipping[] = {{350.0, 0.0, 60.0}, {350.0, 70.0, 60.0}};
// Transport cue preview: one very short high tick, terse enough that rapid
// repeated cue taps while beat-matching read as a rhythm, not a nag. Sits
// above the CueOn/CueOff (headphone cue) pitches so the two cue families
// stay distinct.
const Grain kCuePreview[] = {{988.0, 0.0, 40.0}};
// Audio dropout (xrun): a low, harsh triple-buzz, longer and lower than the
// clipping warning so it can never be mistaken for "turn the gain down" —
// this is an engine-level glitch, not a mixing note.
const Grain kXrun[] = {{196.0, 0.0, 90.0}, {196.0, 110.0, 90.0}, {196.0, 220.0, 90.0}};

struct Gesture {
    const Grain* grains;
    int count;
};

Gesture gestureFor(EngineEarcon::Id id) {
    switch (id) {
    case EngineEarcon::Id::Play:
        return {kPlay, 2};
    case EngineEarcon::Id::Stop:
        return {kStop, 2};
    case EngineEarcon::Id::EndOfTrack:
        return {kEndOfTrack, 3};
    case EngineEarcon::Id::CueOn:
        return {kCueOn, 1};
    case EngineEarcon::Id::CueOff:
        return {kCueOff, 1};
    case EngineEarcon::Id::Restart:
        return {kRestart, 2};
    case EngineEarcon::Id::LoopOn:
        return {kLoopOn, 2};
    case EngineEarcon::Id::LoopOff:
        return {kLoopOff, 2};
    case EngineEarcon::Id::Clipping:
        return {kClipping, 2};
    case EngineEarcon::Id::CuePreview:
        return {kCuePreview, 1};
    case EngineEarcon::Id::Xrun:
        return {kXrun, 3};
    }
    return {nullptr, 0};
}

// Deep enough for a burst of events (both decks, several grains each) without
// dropping triggers; rounds up to a power of two internally.
constexpr int kFifoSize = 64;
constexpr double kDefaultVolume = 0.6;
} // namespace

EngineEarcon::EngineEarcon()
        : m_fifo(kFifoSize) {
    m_pVolume = std::make_unique<ControlPotmeter>(
            ConfigKey(kGroup, QStringLiteral("volume")),
            0.0,
            1.0,
            false,
            true,
            false,
            true,
            kDefaultVolume);
    m_pSampleRate = std::make_unique<ControlProxy>(
            QStringLiteral("[App]"), QStringLiteral("samplerate"), nullptr);

    // Follow the speech output route so the cues land wherever the DJ
    // actually hears announcements (the "Speech output" preference).
    m_pRouteToMain = std::make_unique<ControlProxy>(QStringLiteral("[Tts]"),
            QStringLiteral("route_to_main"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
}

EngineEarcon::~EngineEarcon() = default;

void EngineEarcon::trigger(Id id, Pan pan) {
    Trigger msg{static_cast<int>(id), static_cast<int>(pan)};
    // Single-writer (GUI thread); a full queue just means a very fast burst,
    // in which case dropping the newest request is harmless.
    m_fifo.write(&msg, 1);
}

void EngineEarcon::spawn(Id id, Pan pan, double sampleRate) {
    const Gesture gesture = gestureFor(id);
    const int channel = static_cast<int>(pan);
    for (int g = 0; g < gesture.count; ++g) {
        const Grain& grain = gesture.grains[g];
        // Find a free voice; drop the grain if the pool is exhausted.
        for (Voice& voice : m_voices) {
            if (voice.active) {
                continue;
            }
            voice.active = true;
            voice.freqHz = grain.freqHz;
            voice.posFrames = -grain.startMs / 1000.0 * sampleRate;
            voice.durFrames = grain.durMs / 1000.0 * sampleRate;
            voice.channel = channel;
            break;
        }
    }
}

void EngineEarcon::process(CSAMPLE* pMain, CSAMPLE* pHead, int iFrames) {
    // Follow the speech route: headphones by default so the audience never
    // hears the cues (falling back to main), or main when the DJ routes
    // announcements there — same reasoning as EngineBeatClick.
    CSAMPLE* pOut = m_pRouteToMain->toBool()
            ? (pMain ? pMain : pHead)
            : (pHead ? pHead : pMain);

    const double sampleRate = m_pSampleRate->get();

    // Drain queued triggers into voices (also when pOut is null, so a sound
    // triggered during a momentary output gap isn't left half-played).
    Trigger msg;
    while (m_fifo.read(&msg, 1) == 1) {
        if (sampleRate > 0) {
            spawn(static_cast<Id>(msg.id), static_cast<Pan>(msg.pan), sampleRate);
        }
    }

    if (!pOut || sampleRate <= 0) {
        // Nothing to render into; discard any in-flight voices so they don't
        // resume stale later.
        for (Voice& voice : m_voices) {
            voice.active = false;
        }
        return;
    }

    const double gain = static_cast<double>(m_pVolume->get());
    // ~1 ms attack ramp avoids a click at onset; exponential decay gives the
    // percussive "pip" shape.
    const double attackFrames = std::max(1.0, 0.001 * sampleRate);

    for (Voice& voice : m_voices) {
        if (!voice.active) {
            continue;
        }
        const double omega = 2.0 * M_PI * voice.freqHz / sampleRate;
        // Decay time constant a fraction of the grain length so it is nearly
        // silent by the end.
        const double tau = std::max(1.0, voice.durFrames * 0.35);
        for (int i = 0; i < iFrames; ++i) {
            const double pos = voice.posFrames + i;
            if (pos < 0.0 || pos >= voice.durFrames) {
                continue;
            }
            const double attack = std::min(1.0, pos / attackFrames);
            const double env = attack * std::exp(-pos / tau);
            const auto sample = static_cast<CSAMPLE>(
                    gain * env * std::sin(omega * pos));
            if (voice.channel == 0 || voice.channel == 2) {
                pOut[i * 2] += sample;
            }
            if (voice.channel == 1 || voice.channel == 2) {
                pOut[i * 2 + 1] += sample;
            }
        }
        voice.posFrames += iFrames;
        if (voice.posFrames >= voice.durFrames) {
            voice.active = false;
        }
    }
}

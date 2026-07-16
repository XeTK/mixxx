#include "engine/enginebeatclick.h"

#include <cmath>

#include "control/controlobject.h"
#include "control/controlpotmeter.h"
#include "control/controlproxy.h"
#include "control/controlpushbutton.h"

namespace {
const QString kGroup = QStringLiteral("[BeatClick]");

// Click tones: plain beats at 880 Hz, the first beat of each bar at 1760 Hz.
constexpr double kBeatFreqHz = 880.0;
constexpr double kBarFreqHz = 1760.0;
// Exponential amplitude decay time constant and total click length. Slightly
// longer than the original 8 ms: testers found the very short click hard to
// perceive over a loud mix, and 12 ms gives the ear more of the tone's energy
// while staying clearly percussive.
constexpr double kDecaySeconds = 0.012;
constexpr double kClickSeconds = 0.04;
// Ignore predicted beats closer than this fraction of a beat to the last
// click (double-fire protection around the beat boundary).
constexpr double kMinBeatFraction = 0.5;
// Matches kDefaultBeatClickVolumePercent in dlgprefaccessibility.cpp. Raised
// from the original 0.5: testers reported the click was hard to hear over a
// mix at the old default, and the volume is now user-adjustable regardless.
constexpr double kDefaultVolume = 0.75;
} // namespace

EngineBeatClick::EngineBeatClick() {
    m_pEnabled = std::make_unique<ControlPushButton>(
            ConfigKey(kGroup, QStringLiteral("enabled")));
    m_pEnabled->setButtonMode(mixxx::control::ButtonMode::Toggle);

    m_pVolume = std::make_unique<ControlPotmeter>(ConfigKey(kGroup, QStringLiteral("volume")),
            0.0,
            1.0,
            false,
            true,
            false,
            true,
            kDefaultVolume);

    m_pSampleRate = std::make_unique<ControlProxy>(
            QStringLiteral("[App]"), QStringLiteral("samplerate"), nullptr);

    // Follow the speech output route so the clicks land wherever the DJ
    // actually hears announcements (the "Speech output" preference).
    m_pRouteToMain = std::make_unique<ControlProxy>(QStringLiteral("[Tts]"),
            QStringLiteral("route_to_main"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
}

void EngineBeatClick::addDeck(const QString& group, int channel) {
    Deck deck;
    deck.pPlay = std::make_unique<ControlProxy>(
            group, QStringLiteral("play"), nullptr, ControlFlag::AllowMissingOrInvalid);
    deck.pBeatDistance = std::make_unique<ControlProxy>(
            group, QStringLiteral("beat_distance"), nullptr, ControlFlag::AllowMissingOrInvalid);
    deck.pBpm = std::make_unique<ControlProxy>(
            group, QStringLiteral("bpm"), nullptr, ControlFlag::AllowMissingOrInvalid);
    deck.channel = channel;
    m_decks.push_back(std::move(deck));
}

EngineBeatClick::~EngineBeatClick() = default;

void EngineBeatClick::process(CSAMPLE* pMain, CSAMPLE* pHead, int iFrames) {
    // Follow the speech route: headphones by default so the audience never
    // hears the clicks (falling back to main on single-output setups), or
    // main when the DJ routes announcements there. Previously the clicks
    // were hard-wired head-else-main, so a configured-but-unmonitored
    // headphone bus swallowed them while speech stayed audible on main.
    CSAMPLE* pOut = m_pRouteToMain->toBool()
            ? (pMain ? pMain : pHead)
            : (pHead ? pHead : pMain);
    if (!pOut) {
        return;
    }
    if (!m_pEnabled->toBool()) {
        for (Deck& deck : m_decks) {
            deck.clickPos = kClickIdle;
            deck.framesToNextClick = -1.0;
            deck.beatCount = 0;
            deck.wasPlaying = false;
        }
        return;
    }
    const double sampleRate = m_pSampleRate->get();
    if (sampleRate <= 0) {
        return;
    }

    for (Deck& deck : m_decks) {
        const bool playing = deck.pPlay->toBool();
        if (!playing) {
            // Bar phase restarts with playback; a finished click may ring out.
            deck.framesToNextClick = -1.0;
            if (deck.wasPlaying) {
                deck.beatCount = 0;
            }
            deck.wasPlaying = false;
            renderClicks(&deck, pOut, iFrames, sampleRate);
            continue;
        }
        deck.wasPlaying = true;

        const double bpm = deck.pBpm->get();
        const double beatDistance = deck.pBeatDistance->get();
        if (bpm > 0) {
            const double beatFrames = 60.0 / bpm * sampleRate;
            // Predict where the next beat lands and schedule a click there.
            // Recomputed every callback, so seeks and tempo changes resync
            // within one buffer.
            deck.framesToNextClick = (1.0 - beatDistance) * beatFrames;

            if (deck.framesToNextClick < iFrames &&
                    deck.framesSinceClick > beatFrames * kMinBeatFraction) {
                deck.clickPos = -deck.framesToNextClick; // starts mid-buffer
                deck.clickAccented = (deck.beatCount % 4) == 0;
                deck.beatCount++;
                deck.framesSinceClick = -deck.framesToNextClick;
            }
        }
        deck.framesSinceClick += iFrames;
        renderClicks(&deck, pOut, iFrames, sampleRate);
    }
}

void EngineBeatClick::renderClicks(
        Deck* pDeck, CSAMPLE* pOut, int iFrames, double sampleRate) {
    if (pDeck->clickPos < -static_cast<double>(iFrames)) {
        return; // idle (kClickIdle) or nothing scheduled this buffer
    }
    const double clickFrames = kClickSeconds * sampleRate;
    if (pDeck->clickPos >= clickFrames) {
        pDeck->clickPos = kClickIdle;
        return;
    }
    const double gain = static_cast<double>(m_pVolume->get());
    if (gain <= 0) {
        pDeck->clickPos += iFrames;
        return;
    }
    const double freq = pDeck->clickAccented ? kBarFreqHz : kBeatFreqHz;
    const double omega = 2.0 * M_PI * freq / sampleRate;
    const double decayPerFrame = 1.0 / (kDecaySeconds * sampleRate);

    for (int i = 0; i < iFrames; ++i) {
        const double pos = pDeck->clickPos + i;
        if (pos < 0 || pos >= clickFrames) {
            continue;
        }
        const double envelope = std::exp(-pos * decayPerFrame);
        const CSAMPLE sample = static_cast<CSAMPLE>(
                gain * envelope * std::sin(omega * pos));
        pOut[i * 2 + pDeck->channel] += sample;
    }
    pDeck->clickPos += iFrames;
}

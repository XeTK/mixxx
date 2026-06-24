#include "engine/enginetts.h"

#include <algorithm>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "util/defs.h"
#include "util/sample.h"

namespace {
const QString kAppGroup = QStringLiteral("[App]");

// Sidechain compressor key threshold; matches EngineTalkoverDucking.
constexpr CSAMPLE kDuckThreshold = 0.1f;

// Default ducking strength. Gentler than microphone talkover (0.9) so the music
// stays present underneath spoken announcements.
constexpr double kDefaultDuckStrength = 0.5;

// Hold roughly ten seconds of stereo speech at 48 kHz so a long announcement
// never blocks the (non-realtime) synthesizer thread. FIFO rounds up to the
// next power of two.
constexpr int kFifoSamples = 48000 * 2 * 10;
} // namespace

EngineTts::EngineTts(const QString& group)
        : m_fifo(kFifoSamples),
          m_tts(kMaxEngineSamples),
          m_ducking(group),
          m_duckGainOld(1.0f) {
    m_tts.clear();

    // User-controlled on/off toggle. Written by the menu item and keyboard
    // shortcut; read by speak() to decide whether to synthesize.
    m_pEnabled = std::make_unique<ControlObject>(
            ConfigKey(group, "enabled"), true, false, false, 1.0);

    // Read-only status: 1.0 while the FIFO contains speech data, 0.0 otherwise.
    // Written only by process(); never touched by the user toggle path.
    m_pSpeaking = std::make_unique<ControlObject>(
            ConfigKey(group, "speaking"), true, false, false, 0.0);

    m_pRouteToMain = std::make_unique<ControlObject>(
            ConfigKey(group, "route_to_main"), true, false, true);

    m_pDuckStrength = std::make_unique<ControlObject>(
            ConfigKey(group, "duckStrength"), true, false, true, kDefaultDuckStrength);

    m_pSampleRate = std::make_unique<ControlProxy>(
            kAppGroup, QStringLiteral("samplerate"), nullptr);
    updateDuckingParameters(m_pSampleRate->get());
}

EngineTts::~EngineTts() = default;

void EngineTts::updateDuckingParameters(double sampleRate) {
    if (sampleRate <= 0) {
        sampleRate = 44100;
    }
    m_ducking.setParameters(
            kDuckThreshold,
            static_cast<CSAMPLE>(m_pDuckStrength->get()),
            static_cast<unsigned int>(sampleRate / 2 * 0.1),
            static_cast<unsigned int>(sampleRate / 2));
}

void EngineTts::setRoute(int route) {
    m_pRouteToMain->set(
            route == static_cast<int>(Route::Main) ? 1.0 : 0.0);
}

int EngineTts::writeSamples(const CSAMPLE* pBuffer, int numSamples) {
    return m_fifo.write(pBuffer, numSamples);
}

void EngineTts::process(CSAMPLE* pMain, CSAMPLE* pHead, std::size_t bufferSize, int iFrames) {
    // Keep the compressor parameters in sync with the engine sample rate and
    // the user-configurable ducking strength, recomputing only on change.
    const double sampleRate = m_pSampleRate->get();
    const double duckStrength = m_pDuckStrength->get();
    if (sampleRate != m_lastSampleRate || duckStrength != m_lastDuckStrength) {
        m_lastSampleRate = sampleRate;
        m_lastDuckStrength = duckStrength;
        updateDuckingParameters(sampleRate);
    }

    // If the user has disabled TTS, flush any queued speech and bail. This
    // ensures toggling off mid-utterance silences immediately rather than
    // letting the current FIFO contents play out.
    if (!m_pEnabled->toBool()) {
        m_fifo.flushReadData(m_fifo.readAvailable());
        m_flushRequested.store(false, std::memory_order_release);
        m_pSpeaking->forceSet(0.0);
        m_duckGainOld = 1.0f;
        return;
    }

    // Barge-in: a new utterance asked us to drop whatever is still queued. The
    // discard happens here, on the single reader thread.
    if (m_flushRequested.exchange(false, std::memory_order_acquire)) {
        m_fifo.flushReadData(m_fifo.readAvailable());
    }

    const int wanted = static_cast<int>(bufferSize);
    const int read = m_fifo.read(m_tts.data(), std::min(m_fifo.readAvailable(), wanted));
    if (read < wanted) {
        SampleUtil::clear(m_tts.data() + read, wanted - read);
    }
    const bool speaking = read > 0;
    m_pSpeaking->forceSet(speaking ? 1.0 : 0.0);

    // Feed the speech into the compressor as the key signal and compute the
    // gain to apply to the music. Skip the work entirely while idle and fully
    // recovered to avoid touching the output buffers.
    m_ducking.processKey(m_tts.data(), bufferSize);
    const bool fullyRecovered = !speaking && m_duckGainOld >= 1.0f;
    if (fullyRecovered) {
        m_ducking.setAboveThreshold(false);
        return;
    }
    const auto duckGain = static_cast<CSAMPLE_GAIN>(m_ducking.calculateCompressedGain(iFrames));

    const bool toMain =
            static_cast<EngineTts::Route>(static_cast<int>(m_pRouteToMain->get())) ==
            EngineTts::Route::Main;

    if (toMain) {
        // Duck the music the audience hears and add the speech on top. Fold the
        // same into the headphone mix so the DJ monitors what goes out.
        if (pMain) {
            SampleUtil::applyRampingGain(pMain, m_duckGainOld, duckGain, bufferSize);
            SampleUtil::add(pMain, m_tts.data(), bufferSize);
        }
        if (pHead) {
            SampleUtil::applyRampingGain(pHead, m_duckGainOld, duckGain, bufferSize);
            SampleUtil::add(pHead, m_tts.data(), bufferSize);
        }
    } else if (pHead) {
        // DJ-only: duck and add to the headphone (cue) mix; the main output the
        // audience hears is left untouched.
        SampleUtil::applyRampingGain(pHead, m_duckGainOld, duckGain, bufferSize);
        SampleUtil::add(pHead, m_tts.data(), bufferSize);
    }
    m_duckGainOld = duckGain;
}

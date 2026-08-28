#include "engine/enginetts.h"

#include <algorithm>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "control/controlpushbutton.h"
#include "moc_enginetts.cpp"
#include "util/defs.h"
#include "util/sample.h"
#include "util/ttslog.h"

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

// Depth of the --tts-log instrumentation rings. Announcements are one at a
// time by design (a new one barges in on the old), so a handful of entries is
// already generous; these are rounded up to a power of two by FIFO.
constexpr int kAudibilityRingEntries = 64;

// How often the audio callback's audibility outcomes are drained and written
// to the log. Fast enough that an E2E script polling the log sees COMPLETED
// promptly, slow enough to be free.
constexpr int kAudibilityPollMs = 100;
} // namespace

EngineTts::EngineTts(const QString& group)
        : m_fifo(kFifoSamples),
          m_tts(kMaxEngineSamples),
          m_ducking(group),
          m_duckGainOld(1.0f),
          m_logAudibility(mixxx::ttslog::isEnabled()),
          m_utteranceSpans(kAudibilityRingEntries),
          m_audibilityEvents(kAudibilityRingEntries) {
    m_tts.clear();

    if (m_logAudibility) {
        // Only exists under --tts-log. Lives on (and fires on) the thread that
        // constructed this object -- the GUI thread in Mixxx proper -- so the
        // file I/O never touches the audio callback.
        m_pAudibilityPollTimer = std::make_unique<QTimer>(this);
        connect(m_pAudibilityPollTimer.get(),
                &QTimer::timeout,
                this,
                &EngineTts::pollAudibilityEvents);
        m_pAudibilityPollTimer->start(kAudibilityPollMs);
    }

    // User-controlled on/off toggle. Written by the menu item and keyboard
    // shortcut; read by speak() to decide whether to synthesize. A Toggle
    // push button so a single keypress flips the state — a plain control would
    // be momentary (on only while the key is held) because the keyboard filter
    // sends press=1 / release=0.
    auto pEnabled = std::make_unique<ControlPushButton>(
            ConfigKey(group, "enabled"), false, 1.0);
    pEnabled->setButtonMode(mixxx::control::ButtonMode::Toggle);
    m_pEnabled = std::move(pEnabled);

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

EngineTts::~EngineTts() {
    // Write out any audibility outcomes the audio callback recorded but the
    // poll timer has not picked up yet, so a shutdown right after the last
    // announcement doesn't lose its COMPLETED record.
    if (m_logAudibility) {
        pollAudibilityEvents();
    }

    // The [Tts],enabled control is destroyed here, before the AnnouncementManager
    // (which observes it via a ControlProxy whose lambda calls speak()) is torn
    // down. Emit a signal so the manager can drop its raw sink pointer before
    // this object's members are destroyed — otherwise a control change firing
    // the proxy after this point would call isUserEnabled() on freed memory.
    emit sinkDestroyed();
}

bool EngineTts::isUserEnabled() const {
    return m_pEnabled->toBool();
}

bool EngineTts::isSpeaking() const {
    return m_pSpeaking->toBool();
}

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
    const int written = m_fifo.write(pBuffer, numSamples);
    if (m_logAudibility && written > 0) {
        m_samplesWritten.fetch_add(
                static_cast<quint64>(written), std::memory_order_relaxed);
    }
    return written;
}

void EngineTts::beginUtterance(quint64 utteranceId) {
    if (!m_logAudibility || utteranceId == 0) {
        return;
    }
    // A previously opened utterance that never got its endUtterance() was
    // aborted mid-render by barge-in; the backend logs SUPERSEDED for it, and
    // its orphaned samples simply become part of the run-up to this one.
    m_openUtteranceStart.store(m_samplesWritten.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
    m_openUtteranceId.store(utteranceId, std::memory_order_relaxed);
}

void EngineTts::endUtterance(quint64 utteranceId) {
    if (!m_logAudibility || utteranceId == 0 ||
            m_openUtteranceId.load(std::memory_order_relaxed) != utteranceId) {
        return;
    }
    const UtteranceSpan span{utteranceId,
            m_openUtteranceStart.load(std::memory_order_relaxed),
            m_samplesWritten.load(std::memory_order_relaxed)};
    m_openUtteranceId.store(0, std::memory_order_relaxed);
    if (span.endSample == span.startSample) {
        // Nothing was actually written; there is no audio to be audible.
        mixxx::ttslog::logSuppressedById(utteranceId, mixxx::ttslog::kReasonNoAudio);
        return;
    }
    // Dropping the span if the ring is full only costs a missing COMPLETED
    // record, never correctness.
    m_utteranceSpans.write(&span, 1);
}

void EngineTts::noteSamplesConsumed(int numSamples, bool discarded) {
    if (numSamples <= 0) {
        return;
    }
    m_samplesConsumed += static_cast<quint64>(numSamples);
    if (discarded) {
        m_lastDiscardSample = m_samplesConsumed;
    }
    while (true) {
        if (!m_haveCurrentSpan) {
            if (m_utteranceSpans.read(&m_currentSpan, 1) != 1) {
                return;
            }
            m_haveCurrentSpan = true;
        }
        if (m_currentSpan.endSample > m_samplesConsumed) {
            return; // still playing out
        }
        // A discard at or after this utterance's first sample means part (or
        // all) of it never reached the output buffers.
        const bool fullyHeard = m_lastDiscardSample <= m_currentSpan.startSample;
        const AudibilityEvent event{m_currentSpan.id, fullyHeard ? 1u : 0u};
        m_audibilityEvents.write(&event, 1);
        m_haveCurrentSpan = false;
    }
}

void EngineTts::pollAudibilityEvents() {
    AudibilityEvent event{0, 0};
    while (m_audibilityEvents.read(&event, 1) == 1) {
        if (event.completed) {
            mixxx::ttslog::logCompleted(event.id);
        } else {
            mixxx::ttslog::logFlushed(event.id);
        }
    }
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
        const int discarded = m_fifo.readAvailable();
        m_fifo.flushReadData(discarded);
        if (m_logAudibility) {
            noteSamplesConsumed(discarded, true);
        }
        m_flushRequested.store(false, std::memory_order_release);
        m_pSpeaking->forceSet(0.0);
        m_duckGainOld = 1.0f;
        return;
    }

    // Barge-in: a new utterance asked us to drop whatever is still queued. The
    // discard happens here, on the single reader thread.
    if (m_flushRequested.exchange(false, std::memory_order_acquire)) {
        const int discarded = m_fifo.readAvailable();
        m_fifo.flushReadData(discarded);
        if (m_logAudibility) {
            noteSamplesConsumed(discarded, true);
        }
    }

    const int wanted = static_cast<int>(bufferSize);
    const int read = m_fifo.read(m_tts.data(), std::min(m_fifo.readAvailable(), wanted));
    if (read < wanted) {
        SampleUtil::clear(m_tts.data() + read, wanted - read);
    }
    if (m_logAudibility) {
        noteSamplesConsumed(read, false);
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
    } else {
        // DJ-only: duck and add to the headphone (cue) mix; the main output the
        // audience hears is left untouched. If no headphone output is
        // configured (common on a first-run single-output setup), fall back to
        // main so announcements are not silently dropped.
        CSAMPLE* pOut = pHead ? pHead : pMain;
        if (pOut) {
            SampleUtil::applyRampingGain(pOut, m_duckGainOld, duckGain, bufferSize);
            SampleUtil::add(pOut, m_tts.data(), bufferSize);
        }
    }
    m_duckGainOld = duckGain;
}

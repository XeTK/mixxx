#import <AVFAudio/AVFAudio.h>

#include "util/ttsenginemac.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <utility>
#include <vector>

#include "engine/enginetts.h"
#include "util/types.h"

namespace {

// Block-capturable state, separate from MacTtsEngine itself, so a buffer
// callback arriving after the engine is destroyed can't dereference freed
// memory: the block only extends the lifetime of this small struct.
struct SharedState {
    std::mutex mutex;
    EngineTts* pSink = nullptr;
    int sampleRate = 44100;
    std::atomic<long> generation{0};
};

} // namespace

class MacTtsEngine final : public TtsEngine {
  public:
    MacTtsEngine()
            : m_state(std::make_shared<SharedState>()), m_synth([[AVSpeechSynthesizer alloc] init]) {
    }

    ~MacTtsEngine() override {
        [m_synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
        std::lock_guard<std::mutex> lock(m_state->mutex);
        m_state->pSink = nullptr;
    }

    void say(const QString& text) override {
        {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            m_state->pSink = m_pSink;
            m_state->sampleRate = m_sampleRate;
        }
        // Newer utterance supersedes any render in progress and any audio
        // already queued in the sink.
        m_state->generation.fetch_add(1, std::memory_order_release);
        if (m_pSink) {
            m_pSink->requestFlush();
        }
        // Stop any in-flight render so barge-in doesn't leave the old
        // utterance's synthesis running in the background for no reason; the
        // generation check in feed() already guarantees its output is
        // discarded even without this.
        [m_synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];

        AVSpeechUtterance* utterance =
                [AVSpeechUtterance speechUtteranceWithString:text.toNSString()];
        if (m_voice) {
            utterance.voice = m_voice;
        }
        utterance.rate = m_rate;

        const long generation = m_state->generation.load(std::memory_order_acquire);
        const std::shared_ptr<SharedState> state = m_state;
        [m_synth writeUtterance:utterance
                toBufferCallback:^(AVAudioBuffer* buffer) {
                    feed(state, generation, buffer);
                }];
    }

    void setVoice(const QString& voiceId) override {
        m_voice = voiceId.isEmpty()
                ? nil
                : [AVSpeechSynthesisVoice voiceWithIdentifier:voiceId.toNSString()];
    }

    // Rate in [-10, 10]; 0 is normal speed. AVSpeechUtterance.rate is pinned
    // between AVSpeechUtteranceMinimumSpeechRate and ...Maximum, with
    // ...Default as the midpoint we treat as 0.
    void setRate(int rate) override {
        rate = std::clamp(rate, -10, 10);
        if (rate >= 0) {
            const float t = rate / 10.0f;
            m_rate = AVSpeechUtteranceDefaultSpeechRate +
                    t * (AVSpeechUtteranceMaximumSpeechRate - AVSpeechUtteranceDefaultSpeechRate);
        } else {
            const float t = -rate / 10.0f;
            m_rate = AVSpeechUtteranceDefaultSpeechRate -
                    t * (AVSpeechUtteranceDefaultSpeechRate - AVSpeechUtteranceMinimumSpeechRate);
        }
    }

  private:
    // Runs on whatever thread AVSpeechSynthesizer delivers buffers on (not
    // necessarily the GUI thread). Mirrors QtTtsEngine::feed()'s resample and
    // stereo-conversion logic, but AVSpeechSynthesizer's buffers are always
    // planar float, so there's no format switch needed here.
    static void feed(const std::shared_ptr<SharedState>& state, long generation, AVAudioBuffer* buffer) {
        if (generation != state->generation.load(std::memory_order_acquire)) {
            return; // superseded by a newer utterance
        }
        auto* pcm = static_cast<AVAudioPCMBuffer*>(buffer);
        if (!pcm || pcm.frameLength == 0 || !pcm.floatChannelData) {
            return; // completion marker (empty buffer) or unexpected format
        }

        EngineTts* pSink = nullptr;
        int sampleRate = 44100;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            pSink = state->pSink;
            sampleRate = state->sampleRate;
        }
        if (!pSink) {
            return;
        }

        const float* const* channelData = pcm.floatChannelData;
        const NSUInteger stride = pcm.stride;
        const int inFrames = static_cast<int>(pcm.frameLength);
        const double ratio = pcm.format.sampleRate / sampleRate;

        // Nearest-neighbour resample to the engine rate, matching the other
        // backends: speech is forgiving and this avoids a resampler dependency.
        const int outFrames = static_cast<int>(inFrames / ratio);
        std::vector<CSAMPLE> stereo(static_cast<size_t>(outFrames) * 2);
        for (int i = 0; i < outFrames; ++i) {
            const int srcFrame = static_cast<int>(i * ratio);
            const CSAMPLE f = channelData[0][srcFrame * stride];
            stereo[i * 2] = f;
            stereo[i * 2 + 1] = f;
        }

        int toWrite = outFrames * 2;
        int offset = 0;
        while (toWrite > 0) {
            if (generation != state->generation.load(std::memory_order_acquire)) {
                return; // superseded
            }
            const int written = pSink->writeSamples(stereo.data() + offset, toWrite);
            if (written == 0) {
                break; // sink full; drop the rest rather than block this callback
            }
            offset += written;
            toWrite -= written;
        }
    }

    std::shared_ptr<SharedState> m_state;
    AVSpeechSynthesizer* m_synth;
    AVSpeechSynthesisVoice* m_voice = nil;
    float m_rate = AVSpeechUtteranceDefaultSpeechRate;
};

std::unique_ptr<TtsEngine> createMacTtsEngine() {
    return std::make_unique<MacTtsEngine>();
}

namespace {

// Default is the robotic-sounding tier that ships pre-installed; Enhanced and
// Premium (macOS 13+) are neural voices the user must download separately via
// System Settings > Accessibility > Spoken Content, but are otherwise free
// and fully offline once installed.
QString voiceQualityLabel(AVSpeechSynthesisVoiceQuality quality) {
    switch (quality) {
    case AVSpeechSynthesisVoiceQualityPremium:
        return QStringLiteral("Premium");
    case AVSpeechSynthesisVoiceQualityEnhanced:
        return QStringLiteral("Enhanced");
    default:
        return QStringLiteral("Default");
    }
}

} // namespace

QList<TtsEngine::Voice> enumerateMacTtsVoices() {
    std::vector<std::pair<AVSpeechSynthesisVoiceQuality, TtsEngine::Voice>> voices;
    for (AVSpeechSynthesisVoice* voice in [AVSpeechSynthesisVoice speechVoices]) {
        const QString identifier = QString::fromNSString(voice.identifier);
        const QString name = QString::fromNSString(voice.name);
        const QString language = QString::fromNSString(voice.language);
        const QString quality = voiceQualityLabel(voice.quality);
        voices.emplace_back(voice.quality,
                TtsEngine::Voice{identifier,
                        QStringLiteral("%1 — %2 (%3)").arg(name, quality, language)});
    }
    // Higher-quality (Enhanced/Premium) voices first so they're easy to spot
    // instead of buried among the Default voices that ship pre-installed.
    std::stable_sort(voices.begin(), voices.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });

    QList<TtsEngine::Voice> result;
    for (const auto& [quality, voice] : voices) {
        result << voice;
    }
    return result;
}

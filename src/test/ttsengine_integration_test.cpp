#include <gtest/gtest.h>

#include <QCoreApplication>
#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "engine/enginetts.h"
#include "test/mixxxtest.h"
#include "util/ttsengine.h"
#include "util/types.h"

namespace {

// Unique group avoids CO key conflicts with other tests running in the same process.
constexpr const char* kTtsTestGroup = "[TtsIntegrationTest]";

// How long to wait for the async synthesis worker to fill the FIFO.
constexpr auto kSynthesisTimeout = std::chrono::seconds(10);

} // namespace

class TtsEngineIntegrationTest : public MixxxTest {
  protected:
    void SetUp() override {
        // EngineTts reads [App],samplerate via ControlProxy. Provide it so the
        // ducking parameters are initialized at a known rate rather than the 0
        // fallback.
        m_pSampleRate = std::make_unique<ControlObject>(
                ConfigKey(QStringLiteral("[App]"), QStringLiteral("samplerate")));
        m_pSampleRate->set(44100.0);

        m_pEngineTts = std::make_unique<EngineTts>(kTtsTestGroup);
        // Route speech to the main output so we can inspect pMain in process().
        m_pEngineTts->setRoute(static_cast<int>(EngineTts::Route::Main));

        // process() flushes the FIFO and bails out unless the user-enabled
        // toggle is on; a real caller flips this via the preferences/keyboard
        // shortcut before ever calling say().
        m_pTtsEnabled = std::make_unique<ControlProxy>(kTtsTestGroup, QStringLiteral("enabled"));
        m_pTtsEnabled->set(1.0);

        m_pTtsEngine = TtsEngine::create();
        m_pTtsEngine->setSampleRate(44100);
        m_pTtsEngine->setSink(m_pEngineTts.get());
    }

    // Drain one callback's worth of audio through process() and return the main
    // output buffer so the test can inspect it.
    std::vector<CSAMPLE> drainOneBuffer(int frames = 1024) {
        const int bufferSize = frames * 2; // interleaved stereo
        std::vector<CSAMPLE> mainBuf(static_cast<std::size_t>(bufferSize), 0.0f);
        m_pEngineTts->process(mainBuf.data(),
                nullptr,
                static_cast<std::size_t>(bufferSize),
                frames);
        return mainBuf;
    }

    // Poll by repeatedly draining process(), the way the real-time audio
    // thread does, until a non-silent buffer appears or the timeout expires.
    //
    // This must drain, not just peek at isEmpty(): say()'s requestFlush() is
    // only honored on process()'s *next* call, discarding whatever is in the
    // FIFO at that point. In the real app the audio thread calls process()
    // continuously, so that flush always lands before any synthesized audio
    // exists. But a fast synthesis backend (e.g. macOS's AVSpeechSynthesizer,
    // which renders a whole short utterance in ~1 ms) can fill the FIFO
    // before this test ever calls process() — so the first process() call
    // must be part of the polling loop, or it discards real audio via the
    // still-pending flush and the test would wrongly conclude nothing was
    // produced.
    bool waitForNonSilentAudio(std::vector<CSAMPLE>* pOutput,
            std::chrono::milliseconds timeout =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                            kSynthesisTimeout)) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            *pOutput = drainOneBuffer();
            if (std::any_of(pOutput->begin(), pOutput->end(), [](CSAMPLE s) {
                    return s != 0.0f;
                })) {
                return true;
            }
            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }

    std::unique_ptr<ControlObject> m_pSampleRate;
    std::unique_ptr<EngineTts> m_pEngineTts;
    std::unique_ptr<TtsEngine> m_pTtsEngine;
    std::unique_ptr<ControlProxy> m_pTtsEnabled;
};

// Verify that calling say() with the real platform TTS backend actually causes
// PCM samples to appear in the EngineTts output buffer. This is an integration
// test: it exercises the full synthesis → FIFO → process() pipeline rather than
// a test double.
TEST_F(TtsEngineIntegrationTest, SayProducesAudioInOutputBuffer) {
    if (TtsEngine::enumerateVoices().isEmpty()) {
        GTEST_SKIP() << "No TTS voices installed on this system; skipping audio production test";
    }

    m_pTtsEngine->say(QStringLiteral("Hello"));

    std::vector<CSAMPLE> output;
    EXPECT_TRUE(waitForNonSilentAudio(&output))
            << "No non-silent buffer appeared within " << kSynthesisTimeout.count()
            << " s — the TTS backend produced no audio";
}

// A second utterance arriving while the first is still queued must interrupt
// it (barge-in) and produce audio for the new text. Verify we still get a
// non-silent buffer even when say() is called twice in quick succession.
TEST_F(TtsEngineIntegrationTest, SecondSayInterruptsAndProducesAudio) {
    if (TtsEngine::enumerateVoices().isEmpty()) {
        GTEST_SKIP() << "No TTS voices installed on this system; skipping audio production test";
    }

    m_pTtsEngine->say(QStringLiteral("First message"));
    m_pTtsEngine->say(QStringLiteral("Second message"));

    std::vector<CSAMPLE> output;
    EXPECT_TRUE(waitForNonSilentAudio(&output))
            << "No non-silent buffer appeared after barge-in say()";
}

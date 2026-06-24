#include <gtest/gtest.h>

#include <QCoreApplication>
#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

#include "control/controlobject.h"
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

        m_pTtsEngine = TtsEngine::create();
        m_pTtsEngine->setSampleRate(44100);
        m_pTtsEngine->setSink(m_pEngineTts.get());
    }

    // Block until the EngineTts FIFO holds audio data, or the timeout expires.
    // Returns true if audio arrived.
    bool waitForAudio(
            std::chrono::milliseconds timeout =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                            kSynthesisTimeout)) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (!m_pEngineTts->isEmpty()) {
                return true;
            }
            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return false;
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

    std::unique_ptr<ControlObject> m_pSampleRate;
    std::unique_ptr<EngineTts> m_pEngineTts;
    std::unique_ptr<TtsEngine> m_pTtsEngine;
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

    ASSERT_TRUE(waitForAudio())
            << "FIFO was still empty after " << kSynthesisTimeout.count()
            << " s — the TTS backend produced no audio";

    const std::vector<CSAMPLE> output = drainOneBuffer();

    const bool hasSignal = std::any_of(
            output.begin(), output.end(), [](CSAMPLE s) { return s != 0.0f; });
    EXPECT_TRUE(hasSignal)
            << "Output buffer was all zeros after TTS synthesis — audio was "
               "produced in the FIFO but process() did not mix it into the output";
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

    ASSERT_TRUE(waitForAudio())
            << "FIFO was still empty after barge-in — no audio produced";

    const std::vector<CSAMPLE> output = drainOneBuffer();
    const bool hasSignal = std::any_of(
            output.begin(), output.end(), [](CSAMPLE s) { return s != 0.0f; });
    EXPECT_TRUE(hasSignal) << "Output buffer was silent after barge-in say()";
}

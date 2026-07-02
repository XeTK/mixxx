#include "engine/enginetts.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "control/controlobject.h"
#include "test/mixxxtest.h"
#include "util/types.h"

namespace {
constexpr const char* kGroup = "[EngineTtsTest]";
constexpr int kFrames = 1024;
constexpr int kBufferSize = kFrames * 2; // interleaved stereo
} // namespace

class EngineTtsTest : public MixxxTest {
  protected:
    void SetUp() override {
        m_pAppSampleRate = std::make_unique<ControlObject>(
                ConfigKey(QStringLiteral("[App]"), QStringLiteral("samplerate")));
        m_pAppSampleRate->set(44100.0);
        m_pEngineTts = std::make_unique<EngineTts>(kGroup);
    }

    // Fill the FIFO with `count` interleaved stereo samples all set to `value`.
    void writeSamples(CSAMPLE value, int count) {
        std::vector<CSAMPLE> buf(static_cast<std::size_t>(count), value);
        m_pEngineTts->writeSamples(buf.data(), count);
    }

    // Run one process() call. pMain and pHead are pre-filled to `fill` and
    // returned as vectors so tests can inspect them.
    void process(std::vector<CSAMPLE>& main, std::vector<CSAMPLE>& head) {
        m_pEngineTts->process(
                main.data(), head.data(), kBufferSize, kFrames);
    }

    void processMainOnly(std::vector<CSAMPLE>& main) {
        m_pEngineTts->process(main.data(), nullptr, kBufferSize, kFrames);
    }

    std::unique_ptr<ControlObject> m_pAppSampleRate;
    std::unique_ptr<EngineTts> m_pEngineTts;
};

// ---------------------------------------------------------------------------
// Regression: process() must never overwrite the user-enabled toggle.
//
// Before the fix, process() called m_pEnabled->forceSet(speaking ? 1 : 0),
// which reset the user's on/off choice on every audio callback. This caused
// all announcements after the first to be silently dropped (the FIFO drains,
// process() sets enabled=0, speak() sees isUserEnabled()==false and returns).
// ---------------------------------------------------------------------------

TEST_F(EngineTtsTest, ProcessDoesNotResetUserToggle_WhenDisabled) {
    // Simulate the user turning TTS off via the menu / keyboard shortcut.
    ControlProxy userToggle(QString(kGroup),
            QStringLiteral("enabled"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
    userToggle.set(0.0);
    ASSERT_FALSE(m_pEngineTts->isUserEnabled());

    // Run several process() cycles with and without speech in the FIFO.
    std::vector<CSAMPLE> main(kBufferSize, 0.0f);
    writeSamples(1.0f, kBufferSize);
    processMainOnly(main); // FIFO has data → isSpeaking true
    std::fill(main.begin(), main.end(), 0.0f);
    processMainOnly(main); // FIFO now empty → isSpeaking false

    // The user toggle must be untouched by both calls.
    EXPECT_FALSE(m_pEngineTts->isUserEnabled())
            << "process() overwrote the user-enabled toggle — announcements "
               "would be silently dropped after the FIFO drains";
}

TEST_F(EngineTtsTest, ProcessDoesNotResetUserToggle_WhenEnabled) {
    // Enabled is the default; verify repeated process() calls don't flip it.
    ControlProxy userToggle(QString(kGroup),
            QStringLiteral("enabled"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
    ASSERT_TRUE(m_pEngineTts->isUserEnabled());

    std::vector<CSAMPLE> main(kBufferSize, 0.0f);
    processMainOnly(main); // empty FIFO
    processMainOnly(main);

    EXPECT_TRUE(m_pEngineTts->isUserEnabled())
            << "process() reset the user-enabled toggle on an idle audio callback";
}

TEST_F(EngineTtsTest, IsSpeaking_ReflectsFifoState) {
    EXPECT_FALSE(m_pEngineTts->isSpeaking());

    writeSamples(1.0f, kBufferSize);
    std::vector<CSAMPLE> main(kBufferSize, 0.0f);
    processMainOnly(main); // FIFO has data

    EXPECT_TRUE(m_pEngineTts->isSpeaking());

    // After the FIFO drains (second process with nothing new written):
    processMainOnly(main);
    EXPECT_FALSE(m_pEngineTts->isSpeaking());
}

// ---------------------------------------------------------------------------
// Empty FIFO — process() must not touch either output buffer.
// ---------------------------------------------------------------------------

TEST_F(EngineTtsTest, EmptyFifo_MainBufferUntouched) {
    std::vector<CSAMPLE> main(kBufferSize, 1.0f);
    processMainOnly(main);

    const bool unchanged = std::all_of(
            main.begin(), main.end(), [](CSAMPLE s) { return s == 1.0f; });
    EXPECT_TRUE(unchanged) << "process() modified pMain despite empty FIFO";
}

TEST_F(EngineTtsTest, EmptyFifo_HeadBufferUntouched) {
    std::vector<CSAMPLE> main(kBufferSize, 1.0f);
    std::vector<CSAMPLE> head(kBufferSize, 1.0f);
    process(main, head);

    const bool headUnchanged = std::all_of(
            head.begin(), head.end(), [](CSAMPLE s) { return s == 1.0f; });
    EXPECT_TRUE(headUnchanged) << "process() modified pHead despite empty FIFO";
}

// ---------------------------------------------------------------------------
// Routing — Route::Headphones (default): speech goes to pHead only.
// ---------------------------------------------------------------------------

TEST_F(EngineTtsTest, RouteHeadphones_SpeechAppearsInHead) {
    // Default route is headphones (m_pRouteToMain defaults to 0).
    writeSamples(1.0f, kBufferSize);

    std::vector<CSAMPLE> main(kBufferSize, 0.0f);
    std::vector<CSAMPLE> head(kBufferSize, 0.0f);
    process(main, head);

    const bool headHasSignal = std::any_of(
            head.begin(), head.end(), [](CSAMPLE s) { return s != 0.0f; });
    EXPECT_TRUE(headHasSignal) << "pHead was silent when Route::Headphones";
}

TEST_F(EngineTtsTest, RouteHeadphones_MainBufferUntouched) {
    writeSamples(1.0f, kBufferSize);

    std::vector<CSAMPLE> main(kBufferSize, 0.0f);
    std::vector<CSAMPLE> head(kBufferSize, 0.0f);
    process(main, head);

    const bool mainUntouched = std::all_of(
            main.begin(), main.end(), [](CSAMPLE s) { return s == 0.0f; });
    EXPECT_TRUE(mainUntouched)
            << "pMain was modified when Route::Headphones — speech should be "
               "DJ-only and not reach the main output";
}

// ---------------------------------------------------------------------------
// Routing — Route::Main: speech goes to both pMain and pHead.
// ---------------------------------------------------------------------------

TEST_F(EngineTtsTest, RouteMain_SpeechAppearsInMain) {
    m_pEngineTts->setRoute(static_cast<int>(EngineTts::Route::Main));
    writeSamples(1.0f, kBufferSize);

    std::vector<CSAMPLE> main(kBufferSize, 0.0f);
    std::vector<CSAMPLE> head(kBufferSize, 0.0f);
    process(main, head);

    const bool mainHasSignal = std::any_of(
            main.begin(), main.end(), [](CSAMPLE s) { return s != 0.0f; });
    EXPECT_TRUE(mainHasSignal) << "pMain was silent when Route::Main";
}

TEST_F(EngineTtsTest, RouteMain_SpeechAlsoAppearsInHead) {
    m_pEngineTts->setRoute(static_cast<int>(EngineTts::Route::Main));
    writeSamples(1.0f, kBufferSize);

    std::vector<CSAMPLE> main(kBufferSize, 0.0f);
    std::vector<CSAMPLE> head(kBufferSize, 0.0f);
    process(main, head);

    const bool headHasSignal = std::any_of(
            head.begin(), head.end(), [](CSAMPLE s) { return s != 0.0f; });
    EXPECT_TRUE(headHasSignal)
            << "pHead was silent when Route::Main — DJ should monitor what goes "
               "to the audience";
}

// ---------------------------------------------------------------------------
// Routing — null pHead with Route::Main must not crash.
// ---------------------------------------------------------------------------

TEST_F(EngineTtsTest, RouteMain_NullHead_DoesNotCrash) {
    m_pEngineTts->setRoute(static_cast<int>(EngineTts::Route::Main));
    writeSamples(1.0f, kBufferSize);

    std::vector<CSAMPLE> main(kBufferSize, 0.0f);
    EXPECT_NO_FATAL_FAILURE(
            m_pEngineTts->process(main.data(), nullptr, kBufferSize, kFrames));
}

// ---------------------------------------------------------------------------
// Barge-in flush — requestFlush() followed by process() discards queued audio
// and leaves the output buffers untouched (engine is silent and fully recovered).
// ---------------------------------------------------------------------------

TEST_F(EngineTtsTest, BargeIn_FlushDiscardsQueuedAudio) {
    writeSamples(1.0f, kBufferSize);
    ASSERT_FALSE(m_pEngineTts->isEmpty()) << "precondition: FIFO must be non-empty";

    m_pEngineTts->requestFlush();

    std::vector<CSAMPLE> main(kBufferSize, 1.0f);
    processMainOnly(main);

    EXPECT_TRUE(m_pEngineTts->isEmpty()) << "FIFO was not flushed";

    const bool mainUntouched = std::all_of(
            main.begin(), main.end(), [](CSAMPLE s) { return s == 1.0f; });
    EXPECT_TRUE(mainUntouched)
            << "pMain was modified after flush — process() should be a no-op "
               "when the FIFO is empty and fully recovered";
}

// ---------------------------------------------------------------------------
// Ducking — when speech is present, the gain applied to pMain is less than 1.0
// so music in the main buffer is attenuated.
// ---------------------------------------------------------------------------

TEST_F(EngineTtsTest, RouteMain_Ducking_AttenuatesMusicBuffer) {
    m_pEngineTts->setRoute(static_cast<int>(EngineTts::Route::Main));

    // Fill pMain with a known constant so we can detect attenuation.
    // Process many buffers so the compressor ramps past its attack phase.
    const CSAMPLE kMusicLevel = 1.0f;
    const CSAMPLE kSpeechLevel = 1.0f; // well above duck threshold (0.1)
    std::vector<CSAMPLE> main(kBufferSize);
    std::vector<CSAMPLE> head(kBufferSize, 0.0f);

    // Run enough buffers to let the attack ramp finish (attack_time ≈ 2205 frames
    // at 44.1 kHz, so ~3 buffers of 1024 frames covers >50% attenuation).
    for (int i = 0; i < 4; ++i) {
        writeSamples(kSpeechLevel, kBufferSize);
        std::fill(main.begin(), main.end(), kMusicLevel);
        std::fill(head.begin(), head.end(), 0.0f);
        process(main, head);
    }

    // After ducking, the last sample of pMain should reflect a gain < 1.0
    // applied to kMusicLevel plus the speech contribution. Without ducking the
    // music component would remain at kMusicLevel; with ducking it's below that.
    // We check the last sample where the ramp has had the most time to decay.
    const CSAMPLE lastSample = main.back();
    // With music=1 and speech=1 and no ducking: last = 1*1 + 1 = 2.
    // With ducking after 4 full buffers the gain should be noticeably below 1.
    EXPECT_LT(lastSample, 2.0f * 0.95f)
            << "last sample of pMain showed no ducking after 4 buffers of speech";
}

// ---------------------------------------------------------------------------
// Disable mid-utterance (regression for the FIFO-flush-on-disable fix)
//
// Before the fix, toggling TTS off while the FIFO held audio let the current
// utterance play out. These tests verify the fix: process() flushes and bails
// the moment isUserEnabled() returns false.
// ---------------------------------------------------------------------------

TEST_F(EngineTtsTest, Disabled_FlushesQueuedAudio) {
    writeSamples(1.0f, kBufferSize);
    ASSERT_FALSE(m_pEngineTts->isEmpty()) << "precondition: FIFO must be non-empty";

    ControlProxy userToggle(QString(kGroup),
            QStringLiteral("enabled"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
    userToggle.set(0.0);

    std::vector<CSAMPLE> main(kBufferSize, 0.0f);
    processMainOnly(main);

    EXPECT_TRUE(m_pEngineTts->isEmpty())
            << "FIFO was not flushed when TTS was disabled mid-utterance";
}

TEST_F(EngineTtsTest, Disabled_OutputBufferUntouched) {
    writeSamples(1.0f, kBufferSize);

    ControlProxy userToggle(QString(kGroup),
            QStringLiteral("enabled"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
    userToggle.set(0.0);

    std::vector<CSAMPLE> main(kBufferSize, 1.0f);
    processMainOnly(main);

    const bool unchanged = std::all_of(
            main.begin(), main.end(), [](CSAMPLE s) { return s == 1.0f; });
    EXPECT_TRUE(unchanged)
            << "process() modified pMain while TTS was user-disabled";
}

TEST_F(EngineTtsTest, Disabled_SpeakingCOResetToFalse) {
    writeSamples(1.0f, kBufferSize);

    ControlProxy userToggle(QString(kGroup),
            QStringLiteral("enabled"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
    userToggle.set(0.0);

    std::vector<CSAMPLE> main(kBufferSize, 0.0f);
    processMainOnly(main);

    EXPECT_FALSE(m_pEngineTts->isSpeaking())
            << "isSpeaking should be false after process() when TTS is disabled";
}

TEST_F(EngineTtsTest, Disabled_EmptyFifo_DoesNotCrash) {
    ControlProxy userToggle(QString(kGroup),
            QStringLiteral("enabled"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
    userToggle.set(0.0);
    ASSERT_TRUE(m_pEngineTts->isEmpty());

    std::vector<CSAMPLE> main(kBufferSize, 0.0f);
    EXPECT_NO_FATAL_FAILURE(processMainOnly(main));
}

TEST_F(EngineTtsTest, ReenableAfterDisable_SpeechResumes) {
    m_pEngineTts->setRoute(static_cast<int>(EngineTts::Route::Main));

    ControlProxy userToggle(QString(kGroup),
            QStringLiteral("enabled"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);

    // Disable and confirm the FIFO is flushed.
    userToggle.set(0.0);
    writeSamples(1.0f, kBufferSize);
    std::vector<CSAMPLE> main(kBufferSize, 0.0f);
    processMainOnly(main);
    ASSERT_TRUE(m_pEngineTts->isEmpty()) << "precondition: FIFO should be empty after disable";

    // Re-enable and write new speech; it must appear in the output.
    userToggle.set(1.0);
    writeSamples(1.0f, kBufferSize);
    std::fill(main.begin(), main.end(), 0.0f);
    processMainOnly(main);

    const bool hasSignal = std::any_of(
            main.begin(), main.end(), [](CSAMPLE s) { return s != 0.0f; });
    EXPECT_TRUE(hasSignal)
            << "No audio in pMain after re-enabling TTS — speech did not resume";
}

// ---------------------------------------------------------------------------
// Null pHead with Route::Headphones and queued speech.
//
// Route::Headphones is the default. When no headphone output is configured
// pHead is null — common on a first-run single-output setup. Speech must
// fall back to the main output instead of being silently dropped, or a
// blind user on a fresh install hears nothing at all.
// ---------------------------------------------------------------------------

TEST_F(EngineTtsTest, RouteHeadphones_NullHead_SpeechFallsBackToMain) {
    // Default route is headphones; pHead is null (no headphone output configured).
    writeSamples(1.0f, kBufferSize);

    std::vector<CSAMPLE> main(kBufferSize, 0.0f);
    EXPECT_NO_FATAL_FAILURE(processMainOnly(main)); // passes nullptr for pHead

    const bool mainHasSignal = std::any_of(
            main.begin(), main.end(), [](CSAMPLE s) { return s != 0.0f; });
    EXPECT_TRUE(mainHasSignal)
            << "speech was dropped with Route::Headphones and null pHead — "
               "it must fall back to the main output so announcements are "
               "audible on single-output setups";
}

TEST_F(EngineTtsTest, RouteHeadphones_HeadConfigured_MainStillUntouched) {
    // The fallback must not change behaviour when a headphone output exists.
    writeSamples(1.0f, kBufferSize);

    std::vector<CSAMPLE> main(kBufferSize, 0.0f);
    std::vector<CSAMPLE> head(kBufferSize, 0.0f);
    process(main, head);

    const bool mainUntouched = std::all_of(
            main.begin(), main.end(), [](CSAMPLE s) { return s == 0.0f; });
    EXPECT_TRUE(mainUntouched)
            << "pMain was modified with Route::Headphones and a configured "
               "headphone output — speech must stay DJ-only";
}

#include <gtest/gtest.h>

#include <cmath>
#include <span>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "engine/engineearcon.h"
#include "engine/enginemixer.h"
#include "engine/enginetts.h"
#include "test/mockedenginebackendtest.h"
#include "track/beats.h"
#include "track/track.h"
#include "util/types.h"

// ---------------------------------------------------------------------------
// Rebase guard: the accessibility sinks must stay wired into EngineMixer.
//
// All accessibility audio - synthesized speech, earcons and the beat-click
// metronome - only reaches the sound card because EngineMixer::process() calls
// EngineTts::process(), EngineBeatClick::process() and EngineEarcon::process()
// on the final main/headphone buffers. Those three call sites live in
// enginemixer.cpp, one of the hottest upstream files this fork patches.
//
// If a rebase conflict silently drops one of those blocks, every existing
// accessibility DSP test still passes: enginetts_test.cpp,
// engineearcon_test.cpp and enginebeatclick_test.cpp all instantiate the sink
// directly and call process()/trigger() on their own scratch buffers, so they
// never exercise EngineMixer at all. The synthesizer keeps running, the
// controls keep toggling, the UI keeps behaving - and a blind user hears
// absolutely nothing, with no error anywhere.
//
// These tests close that gap: they push audio into each sink through the real
// EngineMixer that BaseSignalPathTest builds, run a real audio callback, and
// then inspect the mixer's own output buffers. They fail if, and only if, the
// signal stops reaching the output.
//
// Fixture choice: MockedEngineBackendTest installs a MockScaler on every deck.
// The mock never writes into the deck's output buffer (which EngineMixer
// zeroes on registration), so decks render pure silence even while "playing".
// That gives us decks we can put into a realistic playing state for the beat
// click while guaranteeing that any non-zero sample in the mixer output can
// only have come from the accessibility sink under test. Each test asserts
// that silence baseline explicitly rather than assuming it.
// ---------------------------------------------------------------------------

namespace {

// Sum of |sample| over one engine buffer.
//
// Tolerances: the sinks under test emit signals whose energy is orders of
// magnitude above these thresholds (constant-0.5 speech over 1024 samples sums
// to ~512; a 0.75-amplitude beat click with a 12 ms decay sums to ~100+), so
// the thresholds only need to separate "something" from "bit-exact nothing".
// Every path that can touch these buffers is either an exact copy or a gain
// multiply, so true silence is exactly 0.0f; kSilent is a paranoia margin for
// denormals, and kAudible is ~5 orders of magnitude below the real signal.
constexpr double kSilent = 1e-6;
constexpr double kAudible = 1e-3;

double energy(std::span<const CSAMPLE> buffer, int channel = -1) {
    double sum = 0;
    const std::size_t start = channel < 0 ? 0 : static_cast<std::size_t>(channel);
    const std::size_t step = channel < 0 ? 1 : 2;
    for (std::size_t i = start; i < buffer.size(); i += step) {
        sum += std::abs(static_cast<double>(buffer[i]));
    }
    return sum;
}

} // namespace

class A11yMixPathTest : public MockedEngineBackendTest {
  protected:
    void SetUp() override {
        MockedEngineBackendTest::SetUp();
        // Every sink is off by default in this fixture's world view; tests
        // enable exactly the one they exercise.
        ControlObject::set(ConfigKey(QStringLiteral("[BeatClick]"),
                                   QStringLiteral("enabled")),
                0.0);
    }

    // The mixer only fills the first kProcessBufferSize samples of its
    // (max-sized) output buffers.
    std::span<const CSAMPLE> mainOut() const {
        return m_pEngineMixer->getMainBuffer().subspan(
                0, static_cast<std::size_t>(kProcessBufferSize));
    }

    std::span<const CSAMPLE> headOut() const {
        return m_pEngineMixer->getHeadphoneBuffer().subspan(
                0, static_cast<std::size_t>(kProcessBufferSize));
    }

    bool anyDeckPlaying() const {
        for (const QString& group : {m_sGroup1, m_sGroup2, m_sGroup3}) {
            if (ControlObject::get(ConfigKey(group, QStringLiteral("play"))) > 0.0) {
                return true;
            }
        }
        return false;
    }

    // Run one callback and assert both hardware buses came out bit-silent.
    // This is the load-bearing precondition for every test below: without it a
    // "buffer is non-silent" assertion could be satisfied by unrelated deck
    // audio and would prove nothing about the sink under test.
    void assertOutputIsSilent(const char* why) {
        ProcessBuffer();
        ASSERT_LT(energy(mainOut()), kSilent) << why << " (main)";
        ASSERT_LT(energy(headOut()), kSilent) << why << " (headphone)";
    }

    void writeSpeech(CSAMPLE value, int numSamples) {
        std::vector<CSAMPLE> buf(static_cast<std::size_t>(numSamples), value);
        ASSERT_EQ(numSamples,
                m_pEngineMixer->getTts()->writeSamples(buf.data(), numSamples));
    }

    void enableTts() {
        ControlObject::set(
                ConfigKey(QStringLiteral("[Tts]"), QStringLiteral("enabled")), 1.0);
    }
};

// ---------------------------------------------------------------------------
// 1. Text-to-speech -> main output.
// ---------------------------------------------------------------------------

TEST_F(A11yMixPathTest, SpeechRoutedToMain_ReachesMixerMainOutput) {
    EngineTts* pTts = m_pEngineMixer->getTts();
    ASSERT_NE(nullptr, pTts) << "EngineMixer does not own an EngineTts at all";

    enableTts();
    pTts->setRoute(static_cast<int>(EngineTts::Route::Main));

    // Nothing is playing and nothing is queued: the engine must be silent, so
    // any signal seen below can only be the speech we inject.
    ASSERT_FALSE(anyDeckPlaying());
    assertOutputIsSilent("no deck is playing and no speech is queued");

    writeSpeech(0.5f, kProcessBufferSize);
    ProcessBuffer();

    EXPECT_GT(energy(mainOut()), kAudible)
            << "Speech written to EngineTts never reached EngineMixer's main "
               "output buffer. EngineMixer::process() has most likely lost its "
               "m_pTts->process() call (rebase conflict in enginemixer.cpp) - "
               "the synthesizer still runs but no announcement is audible.";
}

TEST_F(A11yMixPathTest, SpeechDisabled_MixerOutputStaysSilent) {
    // Negative control for the test above: with TTS off, queued audio must not
    // appear. If this ever fails, the positive test proves nothing.
    EngineTts* pTts = m_pEngineMixer->getTts();
    ASSERT_NE(nullptr, pTts);

    pTts->setRoute(static_cast<int>(EngineTts::Route::Main));
    ControlObject::set(
            ConfigKey(QStringLiteral("[Tts]"), QStringLiteral("enabled")), 0.0);

    writeSpeech(0.5f, kProcessBufferSize);
    ProcessBuffer();

    EXPECT_LT(energy(mainOut()), kSilent)
            << "speech was mixed into main although TTS is user-disabled";
    EXPECT_LT(energy(headOut()), kSilent)
            << "speech was mixed into the headphone bus although TTS is "
               "user-disabled";
}

// ---------------------------------------------------------------------------
// 2. Text-to-speech -> headphone output (the default DJ-only route).
//
// EngineTts documents a fallback to main when no headphone bus is configured;
// BaseSignalPathTest force-enables the headphone bus, so this covers the
// non-fallback branch: speech must land on head and stay off main.
// ---------------------------------------------------------------------------

TEST_F(A11yMixPathTest, SpeechRoutedToHeadphones_ReachesMixerHeadphoneOutput) {
    EngineTts* pTts = m_pEngineMixer->getTts();
    ASSERT_NE(nullptr, pTts);

    enableTts();
    pTts->setRoute(static_cast<int>(EngineTts::Route::Headphones));

    ASSERT_FALSE(anyDeckPlaying());
    assertOutputIsSilent("no deck is playing and no speech is queued");

    writeSpeech(0.5f, kProcessBufferSize);
    ProcessBuffer();

    EXPECT_GT(energy(headOut()), kAudible)
            << "Speech never reached EngineMixer's headphone buffer on the "
               "default (DJ-only) route - EngineMixer::process() has most "
               "likely lost its m_pTts->process() call.";
    EXPECT_LT(energy(mainOut()), kSilent)
            << "DJ-only speech leaked into the main output the audience hears";
}

// ---------------------------------------------------------------------------
// 3. Earcons -> mixer output.
// ---------------------------------------------------------------------------

TEST_F(A11yMixPathTest, TriggeredEarcon_ReachesMixerOutput) {
    EngineEarcon* pEarcon = m_pEngineMixer->getEarcon();
    ASSERT_NE(nullptr, pEarcon) << "EngineMixer does not own an EngineEarcon at all";

    ASSERT_FALSE(anyDeckPlaying());
    // Negative control: untriggered earcons must leave the buses alone.
    assertOutputIsSilent("no earcon has been triggered");

    // Deck 1's earcon is panned left. An earcon gesture spreads grains out to
    // ~140 ms, so render enough callbacks (512 frames each at 44.1 kHz) to
    // cover the whole gesture and accumulate its energy.
    pEarcon->trigger(EngineEarcon::Id::Play, EngineEarcon::Pan::Left);

    double headLeft = 0;
    for (int i = 0; i < 30; ++i) {
        ProcessBuffer();
        headLeft += energy(headOut(), 0);
    }

    EXPECT_GT(headLeft, kAudible)
            << "A triggered earcon never reached EngineMixer's headphone "
               "output. EngineMixer::process() has most likely lost its "
               "m_pEarcon->process() call (rebase conflict in "
               "enginemixer.cpp) - transport feedback goes completely silent.";
}

TEST_F(A11yMixPathTest, EarconFollowsSpeechRouteToMain_ReachesMixerMainOutput) {
    EngineEarcon* pEarcon = m_pEngineMixer->getEarcon();
    ASSERT_NE(nullptr, pEarcon);

    // Earcons follow [Tts],route_to_main. The control already exists (EngineTts
    // is constructed before EngineEarcon inside EngineMixer), so the engine's
    // proxy is bound to it and picks this up live.
    ControlObject::set(ConfigKey(QStringLiteral("[Tts]"),
                               QStringLiteral("route_to_main")),
            1.0);

    ASSERT_FALSE(anyDeckPlaying());
    assertOutputIsSilent("no earcon has been triggered");

    pEarcon->trigger(EngineEarcon::Id::Play, EngineEarcon::Pan::Left);

    double mainLeft = 0;
    for (int i = 0; i < 30; ++i) {
        ProcessBuffer();
        mainLeft += energy(mainOut(), 0);
    }

    EXPECT_GT(mainLeft, kAudible)
            << "An earcon never reached EngineMixer's main output with speech "
               "routed to main - m_pEarcon->process() is missing from "
               "EngineMixer::process(), or it is called with a null main "
               "buffer.";
}

// ---------------------------------------------------------------------------
// 4. Beat-click metronome -> mixer output.
//
// Unlike the other two sinks, EngineBeatClick is driven entirely by deck
// controls, so this needs a genuinely playing deck with a beat grid. The mock
// scaler keeps that deck silent, so the click is still the only possible
// source of signal - asserted below before the click is enabled.
// ---------------------------------------------------------------------------

TEST_F(A11yMixPathTest, BeatClick_ReachesMixerOutput) {
    // 120 BPM grid on deck 1 (which EngineMixer::addChannel() registers with
    // EngineBeatClick as the left-ear deck).
    mixxx::BeatsPointer pBeats = mixxx::Beats::fromConstTempo(
            m_pTrack1->getSampleRate(), mixxx::audio::kStartFramePos, mixxx::Bpm(120));
    ASSERT_TRUE(m_pTrack1->trySetBeats(pBeats));

    ControlObject::set(ConfigKey(m_sGroup1, QStringLiteral("play")), 1.0);
    ProcessBuffer();
    ASSERT_GT(ControlObject::get(ConfigKey(m_sGroup1, QStringLiteral("play"))), 0.0)
            << "precondition: deck 1 must actually be playing for the "
               "metronome to have anything to click to";

    // With the click still disabled, a playing (mock-scaled, hence silent)
    // deck must produce a bit-silent mixer output. This is what makes the
    // assertion below attributable to the click and nothing else.
    for (int i = 0; i < 60; ++i) {
        assertOutputIsSilent("deck 1 is playing but renders silence and the "
                             "beat click is disabled");
    }

    ControlObject::set(
            ConfigKey(QStringLiteral("[BeatClick]"), QStringLiteral("enabled")),
            1.0);

    // At 120 BPM / 44.1 kHz a beat is 22050 frames apart and a callback is 512
    // frames, so allow well over one beat period of callbacks.
    double headLeft = 0;
    for (int i = 0; i < 120 && headLeft <= kAudible; ++i) {
        ProcessBuffer();
        headLeft += energy(headOut(), 0);
    }

    EXPECT_GT(headLeft, kAudible)
            << "The beat click never reached EngineMixer's headphone output "
               "over more than one beat period. EngineMixer::process() has "
               "most likely lost its m_pBeatClick->process() call (rebase "
               "conflict in enginemixer.cpp) - the metronome a blind DJ uses "
               "to check the beat grid goes silent.";
}

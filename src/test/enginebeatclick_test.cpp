#include "engine/enginebeatclick.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "test/mixxxtest.h"
#include "util/types.h"

namespace {
constexpr const char* kDeckA = "[TestClickA]";
constexpr const char* kDeckB = "[TestClickB]";
constexpr int kFrames = 1024;
constexpr int kSamples = kFrames * 2;

double channelEnergy(const std::vector<CSAMPLE>& buffer, int channel) {
    double sum = 0;
    for (int i = channel; i < static_cast<int>(buffer.size()); i += 2) {
        sum += std::abs(buffer[i]);
    }
    return sum;
}
} // namespace

class EngineBeatClickTest : public MixxxTest {
  protected:
    void SetUp() override {
        m_pSampleRate = std::make_unique<ControlObject>(
                ConfigKey(QStringLiteral("[App]"), QStringLiteral("samplerate")));
        m_pSampleRate->set(44100.0);
        for (const char* group : {kDeckA, kDeckB}) {
            m_controls.push_back(std::make_unique<ControlObject>(
                    ConfigKey(QLatin1String(group), QStringLiteral("play"))));
            m_controls.push_back(std::make_unique<ControlObject>(ConfigKey(
                    QLatin1String(group), QStringLiteral("beat_distance"))));
            m_controls.push_back(std::make_unique<ControlObject>(
                    ConfigKey(QLatin1String(group), QStringLiteral("bpm"))));
        }
        m_pClick = std::make_unique<EngineBeatClick>();
        m_pClick->addDeck(kDeckA, 0);
        m_pClick->addDeck(kDeckB, 1);
        m_head.assign(kSamples, 0.0f);
        m_main.assign(kSamples, 0.0f);
    }

    void setControl(const char* group, const char* name, double value) {
        ControlProxy(QLatin1String(group), QLatin1String(name)).set(value);
    }

    // Put the deck 500 frames before the next beat at 120 BPM.
    void cueDeckBeforeBeat(const char* group) {
        const double beatFrames = 60.0 / 120.0 * 44100.0; // 22050
        setControl(group, "play", 1.0);
        setControl(group, "bpm", 120.0);
        setControl(group, "beat_distance", 1.0 - 500.0 / beatFrames);
    }

    void enable() {
        ControlProxy(QStringLiteral("[BeatClick]"), QStringLiteral("enabled"))
                .set(1.0);
    }

    std::unique_ptr<ControlObject> m_pSampleRate;
    std::vector<std::unique_ptr<ControlObject>> m_controls;
    std::unique_ptr<EngineBeatClick> m_pClick;
    std::vector<CSAMPLE> m_head;
    std::vector<CSAMPLE> m_main;
};

TEST_F(EngineBeatClickTest, Disabled_Silent) {
    cueDeckBeforeBeat(kDeckA);
    m_pClick->process(m_main.data(), m_head.data(), kFrames);
    EXPECT_EQ(0.0, channelEnergy(m_head, 0));
    EXPECT_EQ(0.0, channelEnergy(m_head, 1));
}

TEST_F(EngineBeatClickTest, DeckA_ClicksLeftOnly) {
    enable();
    cueDeckBeforeBeat(kDeckA);
    m_pClick->process(m_main.data(), m_head.data(), kFrames);

    EXPECT_GT(channelEnergy(m_head, 0), 0.0) << "deck A click missing from left";
    EXPECT_EQ(0.0, channelEnergy(m_head, 1)) << "deck A click leaked to right";
    EXPECT_EQ(0.0, channelEnergy(m_main, 0))
            << "click reached main although headphones exist";
}

TEST_F(EngineBeatClickTest, DeckB_ClicksRightOnly) {
    enable();
    cueDeckBeforeBeat(kDeckB);
    m_pClick->process(m_main.data(), m_head.data(), kFrames);

    EXPECT_GT(channelEnergy(m_head, 1), 0.0) << "deck B click missing from right";
    EXPECT_EQ(0.0, channelEnergy(m_head, 0)) << "deck B click leaked to left";
}

TEST(EngineBeatClickConstructionOrderTest, DeckAddedAfterConstruction_StillClicks) {
    // Regression: EngineBeatClick is constructed as part of EngineMixer's own
    // construction, which happens before PlayerManager creates any deck - so
    // a ControlProxy bound to e.g. "[Channel1],play" at *construction* time
    // would permanently latch onto the AllowMissingOrInvalid default control
    // (ControlProxy never re-binds), reading "not playing" forever regardless
    // of real deck state. This reproduces that ordering: construct the engine
    // first, create the deck's controls after, then addDeck().
    auto pSampleRate = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[App]"), QStringLiteral("samplerate")));
    pSampleRate->set(44100.0);

    auto pClick = std::make_unique<EngineBeatClick>();
    ControlProxy(QStringLiteral("[BeatClick]"), QStringLiteral("enabled")).set(1.0);

    // Deck controls created only now, after EngineBeatClick already exists.
    constexpr const char* kDeck = "[TestClickOrderDeck]";
    auto pPlay = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kDeck), QStringLiteral("play")));
    auto pBeatDistance = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kDeck), QStringLiteral("beat_distance")));
    auto pBpm = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kDeck), QStringLiteral("bpm")));

    pClick->addDeck(kDeck, 0);

    const double beatFrames = 60.0 / 120.0 * 44100.0; // 22050
    pPlay->set(1.0);
    pBpm->set(120.0);
    pBeatDistance->set(1.0 - 500.0 / beatFrames);

    std::vector<CSAMPLE> head(kSamples, 0.0f);
    std::vector<CSAMPLE> main(kSamples, 0.0f);
    pClick->process(main.data(), head.data(), kFrames);

    EXPECT_GT(channelEnergy(head, 0), 0.0)
            << "click never renders when the deck's controls are created "
               "after EngineBeatClick itself, matching real app startup order";
}

TEST_F(EngineBeatClickTest, NoHeadphones_FallsBackToMain) {
    enable();
    cueDeckBeforeBeat(kDeckA);
    m_pClick->process(m_main.data(), nullptr, kFrames);

    EXPECT_GT(channelEnergy(m_main, 0), 0.0)
            << "click dropped on single-output setup";
}

TEST_F(EngineBeatClickTest, SpeechRoutedToMain_ClicksFollow) {
    // Regression: the clicks were hard-wired head-else-main, so a DJ who
    // routed speech to the main output (and had a headphone bus configured
    // but not monitored) heard announcements fine but never heard a click.
    auto pRoute = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[Tts]"), QStringLiteral("route_to_main")));
    pRoute->set(1.0);
    // Recreate the engine so its route proxy binds to the CO created above.
    // Destroy the old instance first: assignment would construct the new
    // engine (and its [BeatClick] controls) while the old one still owns
    // controls with the same keys, leaving the new ones dead.
    m_pClick.reset();
    m_pClick = std::make_unique<EngineBeatClick>();
    m_pClick->addDeck(kDeckA, 0);
    m_pClick->addDeck(kDeckB, 1);
    enable();
    cueDeckBeforeBeat(kDeckA);
    m_pClick->process(m_main.data(), m_head.data(), kFrames);

    EXPECT_GT(channelEnergy(m_main, 0), 0.0)
            << "click must follow speech to the main output";
    EXPECT_EQ(0.0, channelEnergy(m_head, 0))
            << "click still went to headphones despite the main route";
}

TEST_F(EngineBeatClickTest, StoppedDeck_Silent) {
    enable();
    cueDeckBeforeBeat(kDeckA);
    setControl(kDeckA, "play", 0.0);
    m_pClick->process(m_main.data(), m_head.data(), kFrames);
    EXPECT_EQ(0.0, channelEnergy(m_head, 0));
}

TEST_F(EngineBeatClickTest, NoDoubleClickWithinBeat) {
    enable();
    cueDeckBeforeBeat(kDeckA);
    m_pClick->process(m_main.data(), m_head.data(), kFrames);
    ASSERT_GT(channelEnergy(m_head, 0), 0.0);

    // Immediately after the beat, beat_distance is small again; the next
    // predicted beat is far away, and the double-fire guard holds regardless.
    setControl(kDeckA, "beat_distance", 0.05);
    // Let the 40 ms click (1764 frames) finish ringing across the next
    // buffers, then require full silence.
    std::vector<CSAMPLE> ringOut(kSamples, 0.0f);
    m_pClick->process(m_main.data(), ringOut.data(), kFrames);
    m_pClick->process(m_main.data(), ringOut.data(), kFrames);
    std::vector<CSAMPLE> silent(kSamples, 0.0f);
    m_pClick->process(m_main.data(), silent.data(), kFrames);
    EXPECT_EQ(0.0, channelEnergy(silent, 0))
            << "spurious click shortly after the beat";
}

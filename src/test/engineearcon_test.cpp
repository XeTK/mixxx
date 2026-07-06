#include "engine/engineearcon.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "test/mixxxtest.h"
#include "util/types.h"

namespace {
constexpr int kFrames = 1024;
constexpr int kSamples = kFrames * 2;

double channelEnergy(const std::vector<CSAMPLE>& buffer, int channel) {
    double sum = 0;
    for (int i = channel; i < static_cast<int>(buffer.size()); i += 2) {
        sum += std::abs(buffer[i]);
    }
    return sum;
}

// Render several buffers so a whole gesture (grains with start offsets up to
// ~140 ms) has time to sound.
void renderBuffers(EngineEarcon* pEarcon,
        std::vector<CSAMPLE>* pMain,
        std::vector<CSAMPLE>* pHead,
        int buffers) {
    for (int i = 0; i < buffers; ++i) {
        pEarcon->process(pMain ? pMain->data() : nullptr,
                pHead ? pHead->data() : nullptr,
                kFrames);
    }
}
} // namespace

class EngineEarconTest : public MixxxTest {
  protected:
    void SetUp() override {
        m_pSampleRate = std::make_unique<ControlObject>(
                ConfigKey(QStringLiteral("[App]"), QStringLiteral("samplerate")));
        m_pSampleRate->set(44100.0);
        m_pEarcon = std::make_unique<EngineEarcon>();
        m_head.assign(kSamples, 0.0f);
        m_main.assign(kSamples, 0.0f);
    }

    std::unique_ptr<ControlObject> m_pSampleRate;
    std::unique_ptr<EngineEarcon> m_pEarcon;
    std::vector<CSAMPLE> m_head;
    std::vector<CSAMPLE> m_main;
};

TEST_F(EngineEarconTest, NoTrigger_Silent) {
    renderBuffers(m_pEarcon.get(), &m_main, &m_head, 10);
    EXPECT_EQ(0.0, channelEnergy(m_head, 0));
    EXPECT_EQ(0.0, channelEnergy(m_head, 1));
}

TEST_F(EngineEarconTest, LeftPan_LeftEarOnly) {
    m_pEarcon->trigger(EngineEarcon::Id::Play, EngineEarcon::Pan::Left);
    renderBuffers(m_pEarcon.get(), &m_main, &m_head, 10);

    EXPECT_GT(channelEnergy(m_head, 0), 0.0) << "left ear should carry the earcon";
    EXPECT_EQ(0.0, channelEnergy(m_head, 1)) << "earcon leaked to the right ear";
    EXPECT_EQ(0.0, channelEnergy(m_main, 0)) << "earcon reached main despite headphones";
}

TEST_F(EngineEarconTest, RightPan_RightEarOnly) {
    m_pEarcon->trigger(EngineEarcon::Id::CueOn, EngineEarcon::Pan::Right);
    renderBuffers(m_pEarcon.get(), &m_main, &m_head, 10);

    EXPECT_GT(channelEnergy(m_head, 1), 0.0);
    EXPECT_EQ(0.0, channelEnergy(m_head, 0));
}

TEST_F(EngineEarconTest, CenterPan_BothEars) {
    m_pEarcon->trigger(EngineEarcon::Id::EndOfTrack, EngineEarcon::Pan::Center);
    renderBuffers(m_pEarcon.get(), &m_main, &m_head, 12);

    EXPECT_GT(channelEnergy(m_head, 0), 0.0);
    EXPECT_GT(channelEnergy(m_head, 1), 0.0);
}

TEST_F(EngineEarconTest, NoHeadphones_FallsBackToMain) {
    m_pEarcon->trigger(EngineEarcon::Id::Play, EngineEarcon::Pan::Left);
    renderBuffers(m_pEarcon.get(), &m_main, nullptr, 10);

    EXPECT_GT(channelEnergy(m_main, 0), 0.0)
            << "earcon dropped on a single-output setup";
}

TEST_F(EngineEarconTest, GestureFinishes_ReturnsToSilence) {
    m_pEarcon->trigger(EngineEarcon::Id::EndOfTrack, EngineEarcon::Pan::Left);
    // Drain the whole gesture (3 pips out to ~195 ms ≈ 9 buffers at 44.1k).
    renderBuffers(m_pEarcon.get(), &m_main, &m_head, 15);

    std::vector<CSAMPLE> tail(kSamples, 0.0f);
    m_pEarcon->process(m_main.data(), tail.data(), kFrames);
    EXPECT_EQ(0.0, channelEnergy(tail, 0)) << "earcon still sounding after it should end";
}

TEST_F(EngineEarconTest, VolumeZero_Silent) {
    ControlProxy(QStringLiteral("[Earcon]"), QStringLiteral("volume")).set(0.0);
    m_pEarcon->trigger(EngineEarcon::Id::Play, EngineEarcon::Pan::Left);
    renderBuffers(m_pEarcon.get(), &m_main, &m_head, 10);
    EXPECT_EQ(0.0, channelEnergy(m_head, 0));
}

TEST_F(EngineEarconTest, RestartGesture_Sounds) {
    m_pEarcon->trigger(EngineEarcon::Id::Restart, EngineEarcon::Pan::Left);
    renderBuffers(m_pEarcon.get(), &m_main, &m_head, 10);
    EXPECT_GT(channelEnergy(m_head, 0), 0.0);
    EXPECT_EQ(0.0, channelEnergy(m_head, 1));
}

TEST_F(EngineEarconTest, LoopOnAndOffGestures_Sound) {
    m_pEarcon->trigger(EngineEarcon::Id::LoopOn, EngineEarcon::Pan::Right);
    renderBuffers(m_pEarcon.get(), &m_main, &m_head, 10);
    EXPECT_GT(channelEnergy(m_head, 1), 0.0);
    EXPECT_EQ(0.0, channelEnergy(m_head, 0));

    std::vector<CSAMPLE> head2(kSamples, 0.0f);
    m_pEarcon->trigger(EngineEarcon::Id::LoopOff, EngineEarcon::Pan::Left);
    renderBuffers(m_pEarcon.get(), &m_main, &head2, 10);
    EXPECT_GT(channelEnergy(head2, 0), 0.0);
}

TEST_F(EngineEarconTest, ClippingGesture_CenterPanned_BothEars) {
    m_pEarcon->trigger(EngineEarcon::Id::Clipping, EngineEarcon::Pan::Center);
    renderBuffers(m_pEarcon.get(), &m_main, &m_head, 10);
    EXPECT_GT(channelEnergy(m_head, 0), 0.0);
    EXPECT_GT(channelEnergy(m_head, 1), 0.0);
}

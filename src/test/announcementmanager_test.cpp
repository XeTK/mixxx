#include "util/announcementmanager.h"

#include <gtest/gtest.h>

#include <QCoreApplication>

#include "control/controlobject.h"
#include "mixer/playermanager.h"
#include "preferences/usersettings.h"
#include "test/mixxxtest.h"
#include "track/track.h"
#include "util/ttsengine.h"

namespace {

// Spy TtsEngine that records every call to say().
class SpyTtsEngine : public TtsEngine {
  public:
    void say(const QString& text) override {
        lastText = text;
        callCount++;
    }

    QString lastText;
    int callCount{0};
};

// Minimal PlayerManager stub: reports zero decks, ignores deck lookups.
class StubPlayerManager : public PlayerManagerInterface {
  public:
    StubPlayerManager()
            : m_numDecks(ConfigKey(QStringLiteral("[App]"),
                      QStringLiteral("num_decks_stub")),
                      true) {
    }

    BaseTrackPlayer* getPlayer(const QString&) const override { return nullptr; }
    BaseTrackPlayer* getPlayer(const ChannelHandle&) const override { return nullptr; }
    BaseTrackPlayer* getDeckBase(int) const override { return nullptr; }
    PreviewDeck* getPreviewDeck(int) const override { return nullptr; }
    Sampler* getSampler(int) const override { return nullptr; }
    int numberOfDecks() const override { return 0; }
    int numberOfSamplers() const override { return 0; }
    int numberOfPreviewDecks() const override { return 0; }

    ControlObject m_numDecks;
};

// Build a track with the given metadata for use in tests.
TrackPointer makeTrack(const QString& artist,
        const QString& title,
        double bpm = 0.0,
        const QString& key = QString()) {
    auto pTrack = Track::newTemporary();
    pTrack->setArtist(artist);
    pTrack->setTitle(title);
    if (bpm > 0.0) {
        pTrack->trySetBpm(bpm);
    }
    if (!key.isEmpty()) {
        pTrack->setKeyText(key);
    }
    return pTrack;
}

} // namespace

class AnnouncementManagerTest : public MixxxTest {
  protected:
    void SetUp() override {
        m_pPlayerManager = std::make_unique<StubPlayerManager>();
    }

    // Build an AnnouncementManager with a SpyTtsEngine and the current config.
    // Returns the spy so tests can inspect calls.
    SpyTtsEngine* makeManager() {
        auto spy = std::make_unique<SpyTtsEngine>();
        SpyTtsEngine* pSpy = spy.get();
        m_pManager = std::make_unique<AnnouncementManager>(
                nullptr, // no Library needed – slots are driven directly
                m_pPlayerManager.get(),
                config(),
                std::move(spy));
        return pSpy;
    }

    std::unique_ptr<StubPlayerManager> m_pPlayerManager;
    std::unique_ptr<AnnouncementManager> m_pManager;
};

// ---------------------------------------------------------------------------
// formatForBrowsing
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, FormatForBrowsing_ArtistAndTitle) {
    auto pTrack = makeTrack(QStringLiteral("Aphex Twin"), QStringLiteral("Windowlicker"));
    EXPECT_QSTRING_EQ("Aphex Twin, Windowlicker", AnnouncementManager::formatForBrowsing(pTrack));
}

TEST_F(AnnouncementManagerTest, FormatForBrowsing_ArtistOnly) {
    auto pTrack = makeTrack(QStringLiteral("Aphex Twin"), QStringLiteral(""));
    EXPECT_QSTRING_EQ("Aphex Twin", AnnouncementManager::formatForBrowsing(pTrack));
}

TEST_F(AnnouncementManagerTest, FormatForBrowsing_TitleOnly) {
    auto pTrack = makeTrack(QStringLiteral(""), QStringLiteral("Windowlicker"));
    EXPECT_QSTRING_EQ("Windowlicker", AnnouncementManager::formatForBrowsing(pTrack));
}

TEST_F(AnnouncementManagerTest, FormatForBrowsing_TrimsWhitespace) {
    auto pTrack = makeTrack(QStringLiteral("  Aphex Twin  "), QStringLiteral("  Windowlicker  "));
    EXPECT_QSTRING_EQ("Aphex Twin, Windowlicker", AnnouncementManager::formatForBrowsing(pTrack));
}

// ---------------------------------------------------------------------------
// formatForLoad
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, FormatForLoad_FullInfo) {
    auto pTrack = makeTrack(
            QStringLiteral("Aphex Twin"),
            QStringLiteral("Windowlicker"),
            128.0,
            QStringLiteral("A minor"));
    EXPECT_QSTRING_EQ(
            "Loaded. Aphex Twin. Windowlicker. 128 B P M. Key: A minor.",
            AnnouncementManager::formatForLoad(pTrack));
}

TEST_F(AnnouncementManagerTest, FormatForLoad_NoBpm) {
    auto pTrack = makeTrack(
            QStringLiteral("Aphex Twin"), QStringLiteral("Windowlicker"), 0.0, QStringLiteral("A minor"));
    const QString result = AnnouncementManager::formatForLoad(pTrack);
    EXPECT_TRUE(result.contains(QStringLiteral("Loaded.")));
    EXPECT_FALSE(result.contains(QStringLiteral("B P M")));
    EXPECT_TRUE(result.contains(QStringLiteral("Key: A minor")));
}

TEST_F(AnnouncementManagerTest, FormatForLoad_NoKey) {
    auto pTrack = makeTrack(QStringLiteral("Aphex Twin"), QStringLiteral("Windowlicker"), 128.0);
    const QString result = AnnouncementManager::formatForLoad(pTrack);
    EXPECT_TRUE(result.contains(QStringLiteral("128 B P M")));
    EXPECT_FALSE(result.contains(QStringLiteral("Key:")));
}

TEST_F(AnnouncementManagerTest, FormatForLoad_BpmRoundsToNearest) {
    auto pTrack = makeTrack(QStringLiteral(""), QStringLiteral(""), 128.6);
    EXPECT_TRUE(AnnouncementManager::formatForLoad(pTrack).contains(QStringLiteral("129 B P M")));
}

TEST_F(AnnouncementManagerTest, FormatForLoad_MissingArtistSkipped) {
    auto pTrack = makeTrack(QStringLiteral(""), QStringLiteral("Windowlicker"), 128.0);
    const QString result = AnnouncementManager::formatForLoad(pTrack);
    EXPECT_TRUE(result.startsWith(QStringLiteral("Loaded. Windowlicker.")));
}

// ---------------------------------------------------------------------------
// Announcement gating via settings
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, AnnounceSelection_EnabledByDefault) {
    SpyTtsEngine* pSpy = makeManager();
    auto pTrack = makeTrack(QStringLiteral("Artist"), QStringLiteral("Title"));

    m_pManager->slotTrackSelected(pTrack);
    m_pManager->slotAnnounceSelectedTrack(); // drive debounce synchronously

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Artist, Title", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, AnnounceSelection_DisabledViaSettings) {
    config()->setValue(ConfigKey("[Accessibility]", "AnnounceTrackSelection"), false);
    SpyTtsEngine* pSpy = makeManager();
    auto pTrack = makeTrack(QStringLiteral("Artist"), QStringLiteral("Title"));

    m_pManager->slotTrackSelected(pTrack);
    m_pManager->slotAnnounceSelectedTrack();

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, AnnounceLoad_EnabledByDefault) {
    SpyTtsEngine* pSpy = makeManager();
    auto pTrack = makeTrack(QStringLiteral("Artist"), QStringLiteral("Title"), 120.0);

    m_pManager->slotNewTrackLoaded(pTrack);

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_TRUE(pSpy->lastText.startsWith(QStringLiteral("Loaded.")));
}

TEST_F(AnnouncementManagerTest, AnnounceLoad_DisabledViaSettings) {
    config()->setValue(ConfigKey("[Accessibility]", "AnnounceTrackLoad"), false);
    SpyTtsEngine* pSpy = makeManager();
    auto pTrack = makeTrack(QStringLiteral("Artist"), QStringLiteral("Title"));

    m_pManager->slotNewTrackLoaded(pTrack);

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, AnnounceLoad_NullTrackIgnored) {
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotNewTrackLoaded(TrackPointer());
    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, AnnounceSelection_NullTrackIgnored) {
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotTrackSelected(TrackPointer());
    m_pManager->slotAnnounceSelectedTrack();
    EXPECT_EQ(0, pSpy->callCount);
}

#include "util/announcementmanager.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include "audio/types.h"
#include "control/controlaudiotaperpot.h"
#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "engine/enginetts.h"
#include "library/library_decl.h"
#include "library/trackmodel.h"
#include "mixer/playermanager.h"
#include "preferences/usersettings.h"
#include "test/mixxxtest.h"
#include "track/keyutils.h"
#include "track/track.h"
#include "util/duration.h"
#include "util/ttsengine.h"

namespace {

// Spy TtsEngine that records every call to say(), setVoice(), and setRate().
class SpyTtsEngine : public TtsEngine {
  public:
    void say(const QString& text) override {
        lastText = text;
        callCount++;
    }

    void setVoice(const QString& voiceId) override {
        lastVoiceId = voiceId;
        setVoiceCount++;
    }

    void setRate(int rate) override {
        lastRate = rate;
        setRateCount++;
    }

    QString lastText;
    int callCount{0};
    QString lastVoiceId;
    int setVoiceCount{0};
    int lastRate{0};
    int setRateCount{0};
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
    int numberOfDecks() const override {
        return m_deckCount;
    }
    int numberOfSamplers() const override { return 0; }
    int numberOfPreviewDecks() const override { return 0; }

    ControlObject m_numDecks;
    // Settable so smart-cue tests can pretend decks exist without real
    // BaseTrackPlayers (the deck lookups still return null).
    int m_deckCount = 0;
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
        pTrack->setAudioProperties(
                mixxx::audio::ChannelCount(2),
                mixxx::audio::SampleRate(44100),
                mixxx::audio::Bitrate(),
                mixxx::Duration::fromSeconds(60));
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
                std::move(spy),
                nullptr,  // no EngineTts sink needed for logic tests
                nullptr); // no EngineEarcon sink needed for logic tests
        return pSpy;
    }

    // Simulate the user navigating into the track list so that track-selection
    // announcements are enabled (they are suppressed when focus is elsewhere).
    // Resets the spy after the navigation so the focus announcement itself is
    // not counted in subsequent assertions.
    void focusTrackList(SpyTtsEngine* pSpy) {
        m_pManager->slotLibraryFocusChanged(
                static_cast<double>(FocusWidget::Sidebar));
        m_pManager->slotLibraryFocusChanged(
                static_cast<double>(FocusWidget::TracksTable));
        pSpy->callCount = 0;
        pSpy->lastText.clear();
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
    // formatForLoad uses the ChromaticKey enum for pronounceable names.
    EXPECT_QSTRING_EQ(
            "Loaded deck, Alpha. Aphex Twin. Windowlicker. 128 B P M. Key: A Minor.",
            AnnouncementManager::formatForLoad(pTrack, 0));
}

TEST_F(AnnouncementManagerTest, FormatForLoad_LancelotNotation) {
    auto pNotation = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[Library]"), QStringLiteral("key_notation")));
    pNotation->set(static_cast<double>(KeyUtils::KeyNotation::Lancelot));
    auto pTrack = makeTrack(
            QStringLiteral("Aphex Twin"),
            QStringLiteral("Windowlicker"),
            128.0,
            QStringLiteral("C major")); // Lancelot (Camelot) code is "8B".

    EXPECT_QSTRING_EQ(
            "Loaded deck, Alpha. Aphex Twin. Windowlicker. 128 B P M. Key: 8, Bravo.",
            AnnouncementManager::formatForLoad(pTrack, 0));
}

TEST_F(AnnouncementManagerTest, FormatForLoad_DeckLetter) {
    auto pTrack = makeTrack(QStringLiteral(""), QStringLiteral(""));
    EXPECT_TRUE(AnnouncementManager::formatForLoad(pTrack, 0).startsWith(
            QStringLiteral("Loaded deck, Alpha")));
    EXPECT_TRUE(AnnouncementManager::formatForLoad(pTrack, 1).startsWith(
            QStringLiteral("Loaded deck, Bravo")));
    EXPECT_TRUE(AnnouncementManager::formatForLoad(pTrack, 2).startsWith(
            QStringLiteral("Loaded deck, Charlie")));
}

TEST_F(AnnouncementManagerTest, FormatForLoad_NoBpm) {
    auto pTrack = makeTrack(
            QStringLiteral("Aphex Twin"), QStringLiteral("Windowlicker"), 0.0, QStringLiteral("A minor"));
    const QString result = AnnouncementManager::formatForLoad(pTrack, 0);
    EXPECT_TRUE(result.contains(QStringLiteral("Loaded deck, Alpha")));
    EXPECT_FALSE(result.contains(QStringLiteral("B P M")));
    EXPECT_TRUE(result.contains(QStringLiteral("Key:")));
}

TEST_F(AnnouncementManagerTest, FormatForLoad_NoKey) {
    auto pTrack = makeTrack(QStringLiteral("Aphex Twin"), QStringLiteral("Windowlicker"), 128.0);
    const QString result = AnnouncementManager::formatForLoad(pTrack, 0);
    EXPECT_TRUE(result.contains(QStringLiteral("128 B P M")));
    EXPECT_FALSE(result.contains(QStringLiteral("Key:")));
}

TEST_F(AnnouncementManagerTest, FormatForLoad_BpmRoundsToNearest) {
    auto pTrack = makeTrack(QStringLiteral(""), QStringLiteral(""), 128.6);
    EXPECT_TRUE(AnnouncementManager::formatForLoad(pTrack, 0).contains(
            QStringLiteral("129 B P M")));
}

TEST_F(AnnouncementManagerTest, FormatForLoad_MissingArtistSkipped) {
    auto pTrack = makeTrack(QStringLiteral(""), QStringLiteral("Windowlicker"), 128.0);
    const QString result = AnnouncementManager::formatForLoad(pTrack, 0);
    EXPECT_TRUE(result.startsWith(QStringLiteral("Loaded deck, Alpha. Windowlicker.")));
}

// ---------------------------------------------------------------------------
// formatForSamplerLoad
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, FormatForSamplerLoad_FullInfo) {
    auto pTrack = makeTrack(
            QStringLiteral("Aphex Twin"),
            QStringLiteral("Windowlicker"),
            128.0,
            QStringLiteral("A minor"));
    // samplerIndex is 0-based; the spoken name is 1-based ("Sampler 3" for
    // index 2), matching the pad numbering printed on the hardware.
    EXPECT_QSTRING_EQ(
            "Sampler 3 loaded. Aphex Twin. Windowlicker. 128 B P M. Key: A Minor.",
            AnnouncementManager::formatForSamplerLoad(pTrack, 2));
}

TEST_F(AnnouncementManagerTest, FormatForSamplerLoad_SamplerNumber) {
    auto pTrack = makeTrack(QStringLiteral(""), QStringLiteral(""));
    EXPECT_TRUE(AnnouncementManager::formatForSamplerLoad(pTrack, 0).startsWith(
            QStringLiteral("Sampler 1 loaded")));
    EXPECT_TRUE(AnnouncementManager::formatForSamplerLoad(pTrack, 15).startsWith(
            QStringLiteral("Sampler 16 loaded")));
}

TEST_F(AnnouncementManagerTest, FormatForSamplerLoad_NoBpm) {
    auto pTrack = makeTrack(QStringLiteral("Aphex Twin"),
            QStringLiteral("Windowlicker"),
            0.0,
            QStringLiteral("A minor"));
    const QString result = AnnouncementManager::formatForSamplerLoad(pTrack, 0);
    EXPECT_TRUE(result.contains(QStringLiteral("Sampler 1 loaded")));
    EXPECT_FALSE(result.contains(QStringLiteral("B P M")));
    EXPECT_TRUE(result.contains(QStringLiteral("Key:")));
}

TEST_F(AnnouncementManagerTest, FormatForSamplerLoad_NoKey) {
    auto pTrack = makeTrack(QStringLiteral("Aphex Twin"), QStringLiteral("Windowlicker"), 128.0);
    const QString result = AnnouncementManager::formatForSamplerLoad(pTrack, 0);
    EXPECT_TRUE(result.contains(QStringLiteral("128 B P M")));
    EXPECT_FALSE(result.contains(QStringLiteral("Key:")));
}

// ---------------------------------------------------------------------------
// Announcement gating via settings
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, AnnounceSelection_EnabledByDefault) {
    SpyTtsEngine* pSpy = makeManager();
    focusTrackList(pSpy);
    auto pTrack = makeTrack(QStringLiteral("Artist"), QStringLiteral("Title"));

    m_pManager->slotTrackSelected(pTrack);
    m_pManager->slotAnnounceSelectedTrack(); // drive debounce synchronously

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Artist, Title", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, AnnounceSelection_DisabledViaSettings) {
    config()->setValue(ConfigKey("[Accessibility]", "AnnounceTrackSelection"), false);
    SpyTtsEngine* pSpy = makeManager();
    focusTrackList(pSpy);
    auto pTrack = makeTrack(QStringLiteral("Artist"), QStringLiteral("Title"));

    m_pManager->slotTrackSelected(pTrack);
    m_pManager->slotAnnounceSelectedTrack();

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, AnnounceSelection_SuppressedWhenSidebarFocused) {
    SpyTtsEngine* pSpy = makeManager();
    // Focus is FocusWidget::None by default — same suppression as sidebar.
    auto pTrack = makeTrack(QStringLiteral("Artist"), QStringLiteral("Title"));

    m_pManager->slotTrackSelected(pTrack);
    m_pManager->slotAnnounceSelectedTrack();

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, AnnounceLoad_EnabledByDefault) {
    SpyTtsEngine* pSpy = makeManager();
    auto pTrack = makeTrack(QStringLiteral("Artist"), QStringLiteral("Title"), 120.0);

    m_pManager->slotNewTrackLoaded(pTrack, 0);

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_TRUE(pSpy->lastText.startsWith(QStringLiteral("Loaded deck, Alpha.")));
}

TEST_F(AnnouncementManagerTest, AnnounceLoad_DisabledViaSettings) {
    config()->setValue(ConfigKey("[Accessibility]", "AnnounceTrackLoad"), false);
    SpyTtsEngine* pSpy = makeManager();
    auto pTrack = makeTrack(QStringLiteral("Artist"), QStringLiteral("Title"));

    m_pManager->slotNewTrackLoaded(pTrack, 0);

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, AnnounceLoad_NullTrackIgnored) {
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotNewTrackLoaded(TrackPointer(), 0);
    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, AnnounceSamplerLoad_EnabledByDefault) {
    SpyTtsEngine* pSpy = makeManager();
    auto pTrack = makeTrack(QStringLiteral("Artist"), QStringLiteral("Title"), 120.0);

    m_pManager->slotNewSamplerTrackLoaded(pTrack, 2);

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_TRUE(pSpy->lastText.startsWith(QStringLiteral("Sampler 3 loaded.")));
}

TEST_F(AnnouncementManagerTest, AnnounceSamplerLoad_DisabledViaSettings) {
    config()->setValue(ConfigKey("[Accessibility]", "AnnounceTrackLoad"), false);
    SpyTtsEngine* pSpy = makeManager();
    auto pTrack = makeTrack(QStringLiteral("Artist"), QStringLiteral("Title"));

    m_pManager->slotNewSamplerTrackLoaded(pTrack, 2);

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, AnnounceSamplerLoad_NullTrackIgnored) {
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotNewSamplerTrackLoaded(TrackPointer(), 2);
    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, AnnounceSelection_NullTrackIgnored) {
    SpyTtsEngine* pSpy = makeManager();
    focusTrackList(pSpy);
    m_pManager->slotTrackSelected(TrackPointer());
    m_pManager->slotAnnounceSelectedTrack();
    EXPECT_EQ(0, pSpy->callCount);
}

// ---------------------------------------------------------------------------
// Row selection announcements (Library::trackRowSelected)
//
// The track table sends the model's full spoken description of a selected
// row - not just artist/title - via TrackModel::rowAccessibleText(), routed
// through the same debounce/gating as ordinary track selection. See
// BaseTrackTableModel::rowAccessibleText() for what that text contains
// (rating, color, played state, BPM lock).
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, AnnounceRowSelection_EnabledByDefault) {
    SpyTtsEngine* pSpy = makeManager();
    focusTrackList(pSpy);

    m_pManager->slotTrackRowSelected(QStringLiteral("Artist, Title, 3 stars"), 2, 10);
    m_pManager->slotAnnounceSelectedTrack(); // drive debounce synchronously

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Artist, Title, 3 stars, 3 of 10", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, AnnounceRowSelection_DisabledViaSettings) {
    config()->setValue(ConfigKey("[Accessibility]", "AnnounceTrackSelection"), false);
    SpyTtsEngine* pSpy = makeManager();
    focusTrackList(pSpy);

    m_pManager->slotTrackRowSelected(QStringLiteral("Artist, Title"), 0, 5);
    m_pManager->slotAnnounceSelectedTrack();

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, AnnounceRowSelection_SuppressedWhenSidebarFocused) {
    SpyTtsEngine* pSpy = makeManager();
    // Focus is FocusWidget::None by default — same suppression as sidebar.
    m_pManager->slotTrackRowSelected(QStringLiteral("Artist, Title"), 0, 5);
    m_pManager->slotAnnounceSelectedTrack();

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, AnnounceRowSelection_EmptyTextIgnored) {
    SpyTtsEngine* pSpy = makeManager();
    focusTrackList(pSpy);

    m_pManager->slotTrackRowSelected(QString(), 0, 5);
    m_pManager->slotAnnounceSelectedTrack();

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, AnnounceRowSelection_EmptyTextFallsBackToTrackSelected) {
    // Signal order for a model that does not override rowAccessibleText():
    // trackSelected() fires first, then rowSelected() with nothing to say.
    // The artist/title announcement must survive, and no bare position
    // ("1 of 5") may be spoken in its place.
    SpyTtsEngine* pSpy = makeManager();
    focusTrackList(pSpy);
    auto pTrack = makeTrack(QStringLiteral("Artist"), QStringLiteral("Title"));

    m_pManager->slotTrackSelected(pTrack);
    m_pManager->slotTrackRowSelected(QStringLiteral("   "), 0, 5);
    m_pManager->slotAnnounceSelectedTrack();

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Artist, Title", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, AnnounceRowSelection_PositionOmittedForSingleRow) {
    SpyTtsEngine* pSpy = makeManager();
    focusTrackList(pSpy);

    // Only one row in the whole table: "1 of 1" would be noise.
    m_pManager->slotTrackRowSelected(QStringLiteral("Artist, Title"), 0, 1);
    m_pManager->slotAnnounceSelectedTrack();

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Artist, Title", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, AnnounceRowSelection_TakesPriorityOverPlainTrackSelected) {
    // Mirrors the real signal order from WTrackTableView::slotGuiTick50ms:
    // trackSelected(pTrack) fires first, then rowSelected() with the fuller
    // text. The row text should win.
    SpyTtsEngine* pSpy = makeManager();
    focusTrackList(pSpy);
    auto pTrack = makeTrack(QStringLiteral("Artist"), QStringLiteral("Title"));

    m_pManager->slotTrackSelected(pTrack);
    m_pManager->slotTrackRowSelected(QStringLiteral("Artist, Title, 5 stars"), 0, 3);
    m_pManager->slotAnnounceSelectedTrack();

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Artist, Title, 5 stars, 1 of 3", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, AnnounceSelection_PlainTrackSelectedClearsPendingRowText) {
    // A later plain trackSelected() (e.g. multi-selection collapsing to a
    // single track) must not leave a stale row announcement behind.
    SpyTtsEngine* pSpy = makeManager();
    focusTrackList(pSpy);
    auto pTrack = makeTrack(QStringLiteral("Artist"), QStringLiteral("Title"));

    m_pManager->slotTrackRowSelected(QStringLiteral("Old row text"), 0, 3);
    m_pManager->slotTrackSelected(pTrack);
    m_pManager->slotAnnounceSelectedTrack();

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Artist, Title", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// Playstate announcements (play / stop / end-of-track)
//
// These tests drive the CO observers that connectGroupControls() wires up.
// The test group COs are created explicitly so no real deck is needed.
// ---------------------------------------------------------------------------

class AnnouncementManagerPlaystateTest : public AnnouncementManagerTest {
  protected:
    static constexpr const char* kGroup = "[TestChannel1]";

    // Call after makeManager() to create the group COs and wire up the
    // observers. hasTrack controls whether the deck is seen as loaded.
    void setupGroup(bool hasTrack = true) {
        m_pPlay = std::make_unique<ControlObject>(
                ConfigKey(QLatin1String(kGroup), QStringLiteral("play")));
        m_pEndOfTrack = std::make_unique<ControlObject>(
                ConfigKey(QLatin1String(kGroup), QStringLiteral("end_of_track")));
        m_pPfl = std::make_unique<ControlObject>(
                ConfigKey(QLatin1String(kGroup), QStringLiteral("pfl")));
        m_pEject = std::make_unique<ControlObject>(
                ConfigKey(QLatin1String(kGroup), QStringLiteral("eject")));
        m_pPeakIndicator = std::make_unique<ControlObject>(
                ConfigKey(QLatin1String(kGroup), QStringLiteral("peak_indicator")));
        m_pManager->connectGroupControls(QString::fromLatin1(kGroup));
        m_pManager->setDeckHasTrack(QString::fromLatin1(kGroup), hasTrack);
    }

    void setPlay(double v) {
        m_pPlay->set(v);
        QCoreApplication::processEvents();
    }

    void setEndOfTrack(double v) {
        m_pEndOfTrack->set(v);
        QCoreApplication::processEvents();
    }

    void setPfl(double v) {
        m_pPfl->set(v);
        QCoreApplication::processEvents();
    }

    void pressEject() {
        m_pEject->set(1.0);
        QCoreApplication::processEvents();
        m_pEject->set(0.0);
        QCoreApplication::processEvents();
    }

    void setPeakIndicator(double v) {
        m_pPeakIndicator->set(v);
        QCoreApplication::processEvents();
    }

    std::unique_ptr<ControlObject> m_pPlay;
    std::unique_ptr<ControlObject> m_pEndOfTrack;
    std::unique_ptr<ControlObject> m_pPfl;
    std::unique_ptr<ControlObject> m_pEject;
    std::unique_ptr<ControlObject> m_pPeakIndicator;
};

TEST_F(AnnouncementManagerPlaystateTest, PlayStarted_AnnouncesPlaying) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPlay(1.0);

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Playing", pSpy->lastText);
}

TEST_F(AnnouncementManagerPlaystateTest, PlayStarted_NoTrackLoaded_Silent) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup(/*hasTrack=*/false);

    setPlay(1.0);

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerPlaystateTest, PlayStarted_SettingDisabled_Silent) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnouncePlay")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPlay(1.0);

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerPlaystateTest, PlayStopped_AnnouncesStopped) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPlay(1.0); // → "Playing"
    pSpy->callCount = 0;
    setPlay(0.0); // → "Stopped"

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Stopped", pSpy->lastText);
}

TEST_F(AnnouncementManagerPlaystateTest, PlayStopped_SettingDisabled_Silent) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceStop")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPlay(1.0);
    pSpy->callCount = 0;
    setPlay(0.0);

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerPlaystateTest, EndOfTrack_AnnouncesEndOfTrack) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setEndOfTrack(1.0);

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("End of track", pSpy->lastText);
}

TEST_F(AnnouncementManagerPlaystateTest, EndOfTrack_SettingDisabled_Silent) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceEndOfTrack")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setEndOfTrack(1.0);

    EXPECT_EQ(0, pSpy->callCount);
}

// When a track reaches its end the end_of_track CO fires first (→ "End of
// track"), then play drops to 0.  The stop handler must NOT also say "Stopped".
TEST_F(AnnouncementManagerPlaystateTest, EndOfTrack_StopSuppressed) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPlay(1.0);       // "Playing" — callCount = 1
    setEndOfTrack(1.0); // "End of track" — callCount = 2
    setPlay(0.0);       // play→0 while end_of_track=1; "Stopped" must be suppressed

    EXPECT_EQ(2, pSpy->callCount);
    EXPECT_QSTRING_EQ("End of track", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// Sampler play / stop / eject announcements
//
// These drive the CO observers that connectSamplerControls() wires up — the
// lighter-weight sampler counterpart of connectGroupControls() above. The
// test group COs are created explicitly so no real Sampler is needed.
// ---------------------------------------------------------------------------

class AnnouncementManagerSamplerTest : public AnnouncementManagerTest {
  protected:
    static constexpr const char* kGroup = "[TestSampler1]";
    // 0-based index; the spoken name is "Sampler 3" (1-based, matching pad
    // numbering).
    static constexpr int kSamplerIndex = 2;

    // Call after makeManager() to create the group COs and wire up the
    // observers. hasTrack controls whether the sampler is seen as loaded.
    void setupGroup(bool hasTrack = true) {
        m_pPlay = std::make_unique<ControlObject>(
                ConfigKey(QLatin1String(kGroup), QStringLiteral("play")));
        m_pEject = std::make_unique<ControlObject>(
                ConfigKey(QLatin1String(kGroup), QStringLiteral("eject")));
        m_pManager->connectSamplerControls(QString::fromLatin1(kGroup), kSamplerIndex);
        m_pManager->setDeckHasTrack(QString::fromLatin1(kGroup), hasTrack);
    }

    void setPlay(double v) {
        m_pPlay->set(v);
        QCoreApplication::processEvents();
    }

    void setEject(double v) {
        m_pEject->set(v);
        QCoreApplication::processEvents();
    }

    std::unique_ptr<ControlObject> m_pPlay;
    std::unique_ptr<ControlObject> m_pEject;
};

TEST_F(AnnouncementManagerSamplerTest, PlayStarted_AnnouncesPlaying) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPlay(1.0);

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Sampler 3 playing", pSpy->lastText);
}

TEST_F(AnnouncementManagerSamplerTest, PlayStarted_NoTrackLoaded_Silent) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup(/*hasTrack=*/false);

    setPlay(1.0);

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerSamplerTest, PlayStarted_SettingDisabled_Silent) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnouncePlay")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPlay(1.0);

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerSamplerTest, PlayStopped_AnnouncesStopped) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPlay(1.0); // → "Sampler 3 playing"
    pSpy->callCount = 0;
    setPlay(0.0); // → "Sampler 3 stopped"

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Sampler 3 stopped", pSpy->lastText);
}

TEST_F(AnnouncementManagerSamplerTest, PlayStopped_SettingDisabled_Silent) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceStop")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPlay(1.0);
    pSpy->callCount = 0;
    setPlay(0.0);

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerSamplerTest, Eject_AnnouncesEjected) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setEject(1.0);

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Sampler 3 ejected", pSpy->lastText);
}

TEST_F(AnnouncementManagerSamplerTest, Eject_NoTrackLoaded_Silent) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup(/*hasTrack=*/false);

    setEject(1.0);

    EXPECT_EQ(0, pSpy->callCount);
}

// The engine itself refuses to eject a playing pad; the announcement should
// not claim it happened.
TEST_F(AnnouncementManagerSamplerTest, Eject_WhilePlaying_Silent) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPlay(1.0); // → "Sampler 3 playing"
    pSpy->callCount = 0;
    setEject(1.0);

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerSamplerTest, Eject_SettingDisabled_Silent) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceTrackLoad")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setEject(1.0);

    EXPECT_EQ(0, pSpy->callCount);
}

// ---------------------------------------------------------------------------
// Startup announcement (slotSkinLoaded)
// ---------------------------------------------------------------------------

// On boot, the skin loads (running slotSkinLoaded()) before setupDevices()
// has ever run, so slotSoundDevicesReady() (wired to
// SoundManager::devicesSetup()) has not fired yet. Speaking "Mixxx ready"
// immediately in that state would write into EngineTts's FIFO with nothing
// pulling it yet -- issue #49. It must be queued and only spoken once a
// sound device is confirmed open.
TEST_F(AnnouncementManagerTest, SkinLoaded_BeforeAudioReady_QueuesAnnouncement) {
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotSkinLoaded();
    EXPECT_EQ(0, pSpy->callCount);

    m_pManager->slotSoundDevicesReady();
    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Mixxx ready", pSpy->lastText);
}

// A skin reload while Mixxx is already running (e.g. rebootMixxxView()) finds
// audio already confirmed running, so the announcement is spoken immediately
// rather than queued.
TEST_F(AnnouncementManagerTest, SkinLoaded_AfterAudioReady_AnnouncesImmediately) {
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotSoundDevicesReady();
    m_pManager->slotSkinLoaded();
    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Mixxx ready", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, SkinLoaded_SettingDisabled_NeverAnnouncesEvenOnceReady) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"),
                               QStringLiteral("AnnounceStartup")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotSkinLoaded();
    m_pManager->slotSoundDevicesReady();
    EXPECT_EQ(0, pSpy->callCount);
}

// slotSoundDevicesReady() firing with nothing queued (the common case: audio
// comes up before AnnounceStartup would ever have anything pending, or the
// user has startup announcements disabled) must not speak anything.
TEST_F(AnnouncementManagerTest, SoundDevicesReady_NoPendingAnnouncement_Silent) {
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotSoundDevicesReady();
    EXPECT_EQ(0, pSpy->callCount);
}

// devicesSetup() can fire again later, e.g. the user reopens Preferences and
// reconfigures sound hardware. That must not re-announce "Mixxx ready".
TEST_F(AnnouncementManagerTest, SoundDevicesReady_FiresAgain_DoesNotReannounce) {
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotSkinLoaded();
    m_pManager->slotSoundDevicesReady();
    pSpy->callCount = 0;
    pSpy->lastText.clear();

    m_pManager->slotSoundDevicesReady();
    EXPECT_EQ(0, pSpy->callCount);
}

// ---------------------------------------------------------------------------
// Library focus announcements (slotLibraryFocusChanged / slotSidebarItemActivated)
// ---------------------------------------------------------------------------

// Transitions from None are suppressed (window focus restore), so seed focus
// with a non-None widget before testing navigation announcements.
TEST_F(AnnouncementManagerTest, LibraryFocus_SearchbarAnnounced) {
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotLibraryFocusChanged(static_cast<double>(FocusWidget::Sidebar));
    pSpy->callCount = 0; // None→Sidebar: no speech (suppressed), but dedup IS cleared
    m_pManager->slotLibraryFocusChanged(static_cast<double>(FocusWidget::Searchbar));
    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Search bar", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, LibraryFocus_SidebarAnnounced) {
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotLibraryFocusChanged(static_cast<double>(FocusWidget::TracksTable));
    pSpy->callCount = 0;
    m_pManager->slotLibraryFocusChanged(static_cast<double>(FocusWidget::Sidebar));
    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Sidebar", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, LibraryFocus_TracksTableAnnounced) {
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotLibraryFocusChanged(static_cast<double>(FocusWidget::Sidebar));
    pSpy->callCount = 0;
    m_pManager->slotLibraryFocusChanged(static_cast<double>(FocusWidget::TracksTable));
    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Track list", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, LibraryFocus_NoneToWidgetSuppressed) {
    // Transitioning from None (window lost focus) back to a widget must not
    // announce — that's a window-focus restore, not intentional navigation.
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotLibraryFocusChanged(static_cast<double>(FocusWidget::TracksTable));
    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, LibraryFocus_UnknownValueSilent) {
    SpyTtsEngine* pSpy = makeManager();
    // Seed with a non-None value so the None suppression doesn't interfere.
    m_pManager->slotLibraryFocusChanged(static_cast<double>(FocusWidget::Sidebar));
    pSpy->callCount = 0;
    m_pManager->slotLibraryFocusChanged(static_cast<double>(FocusWidget::None));
    m_pManager->slotLibraryFocusChanged(static_cast<double>(FocusWidget::ContextMenu));
    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, LibraryFocus_SettingDisabled_Silent) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceLibraryFocus")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotLibraryFocusChanged(static_cast<double>(FocusWidget::Sidebar));
    m_pManager->slotLibraryFocusChanged(static_cast<double>(FocusWidget::Searchbar));
    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, SidebarItemActivated_SpeaksTitle) {
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotSidebarItemActivated(QStringLiteral("My Set"));
    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("My Set", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, SidebarItemActivated_SettingDisabled_Silent) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceLibraryFocus")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotSidebarItemActivated(QStringLiteral("My Set"));
    EXPECT_EQ(0, pSpy->callCount);
}

// ---------------------------------------------------------------------------
// Regression: sidebar item must be announced after first focus-enter even
// when prevFocus == None (startup case).
//
// activateDefaultSelection() fires slotSidebarItemActivated("Tracks") at
// startup, setting m_lastAnnouncedSidebarItem = "Tracks". The very first
// focus-change to the sidebar has prevFocus == None, so the old code returned
// early before clearing the dedup. Any subsequent click on "Tracks" was then
// silently skipped. The fix: clear m_lastAnnouncedSidebarItem unconditionally
// when newFocus == Sidebar, before the None-suppression guard.
// ---------------------------------------------------------------------------
TEST_F(AnnouncementManagerTest, SidebarDedup_ClearedOnFirstFocusEnter) {
    SpyTtsEngine* pSpy = makeManager();

    // Simulate activateDefaultSelection setting the dedup to "Tracks".
    m_pManager->slotSidebarItemActivated(QStringLiteral("Tracks"));
    ASSERT_EQ(1, pSpy->callCount); // announced once
    pSpy->callCount = 0;

    // First sidebar focus-enter: prevFocus == None → speech suppressed,
    // but m_lastAnnouncedSidebarItem must be cleared.
    m_pManager->slotLibraryFocusChanged(static_cast<double>(FocusWidget::Sidebar));
    EXPECT_EQ(0, pSpy->callCount); // still no speech

    // Now clicking/navigating to "Tracks" must be spoken (dedup was cleared).
    m_pManager->slotSidebarItemActivated(QStringLiteral("Tracks"));
    EXPECT_EQ(1, pSpy->callCount)
            << "\"Tracks\" was silently deduped after first sidebar focus-enter — "
               "activateDefaultSelection poisoned m_lastAnnouncedSidebarItem at startup";
    EXPECT_QSTRING_EQ("Tracks", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// speak() voice and rate sync
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, Speak_SyncsVoiceOnChange) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"),
                               QStringLiteral("TtsVoice")),
            QStringLiteral("voice-1"));
    SpyTtsEngine* pSpy = makeManager();
    focusTrackList(pSpy);
    auto pTrack = makeTrack(QStringLiteral("Artist"), QStringLiteral("Title"));

    m_pManager->slotTrackSelected(pTrack);
    m_pManager->slotAnnounceSelectedTrack();

    EXPECT_EQ(1, pSpy->setVoiceCount);
    EXPECT_QSTRING_EQ("voice-1", pSpy->lastVoiceId);
}

TEST_F(AnnouncementManagerTest, Speak_SkipsVoiceSyncWhenUnchanged) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"),
                               QStringLiteral("TtsVoice")),
            QStringLiteral("voice-1"));
    SpyTtsEngine* pSpy = makeManager();
    focusTrackList(pSpy);
    auto pTrack = makeTrack(QStringLiteral("Artist"), QStringLiteral("Title"));

    m_pManager->slotTrackSelected(pTrack);
    m_pManager->slotAnnounceSelectedTrack();
    m_pManager->slotTrackSelected(pTrack);
    m_pManager->slotAnnounceSelectedTrack();

    // Voice was set only once (during focusTrackList), not again on second speak.
    EXPECT_EQ(1, pSpy->setVoiceCount);
    EXPECT_EQ(2, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, Speak_SyncsRateOnChange) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"),
                               QStringLiteral("TtsRate")),
            5);
    SpyTtsEngine* pSpy = makeManager();
    focusTrackList(pSpy);
    auto pTrack = makeTrack(QStringLiteral("Artist"), QStringLiteral("Title"));

    m_pManager->slotTrackSelected(pTrack);
    m_pManager->slotAnnounceSelectedTrack();

    EXPECT_EQ(1, pSpy->setRateCount);
    EXPECT_EQ(5, pSpy->lastRate);
}

// ---------------------------------------------------------------------------
// Search announcements (slotSearchTextChanged / slotAnnounceSearch)
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, AnnounceSearch_EnabledByDefault) {
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotSearchTextChanged(QStringLiteral("house"));
    m_pManager->slotAnnounceSearch(); // drive debounce synchronously
    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Searching: house", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, AnnounceSearch_DisabledViaSettings) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceSearch")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotSearchTextChanged(QStringLiteral("house"));
    m_pManager->slotAnnounceSearch();
    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, AnnounceSearch_ClearedSearchText) {
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotSearchTextChanged(QStringLiteral(""));
    m_pManager->slotAnnounceSearch();
    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Search cleared", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// Track-list sort announcements (slotAnnounceSort)
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, AnnounceSort_ColumnAndOrder) {
    // The [Library] sort controls must exist before the manager attaches its
    // observers in init().
    auto pSortColumn = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Library]"), QStringLiteral("sort_column")));
    auto pSortOrder = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Library]"), QStringLiteral("sort_order")));
    SpyTtsEngine* pSpy = makeManager();

    pSortColumn->set(static_cast<double>(TrackModel::SortColumnId::Title));
    pSortOrder->set(0.0); // ascending
    m_pManager->slotAnnounceSort(); // drive debounce synchronously
    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Sorting by title ascending", pSpy->lastText);

    pSortOrder->set(1.0); // descending
    m_pManager->slotAnnounceSort();
    EXPECT_EQ(2, pSpy->callCount);
    EXPECT_QSTRING_EQ("Sorting by title descending", pSpy->lastText);

    pSortColumn->set(static_cast<double>(TrackModel::SortColumnId::Bpm));
    pSortOrder->set(0.0);
    m_pManager->slotAnnounceSort();
    EXPECT_QSTRING_EQ("Sorting by BPM ascending", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, AnnounceSort_DisabledViaSettings) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceSort")),
            false);
    auto pSortColumn = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Library]"), QStringLiteral("sort_column")));
    auto pSortOrder = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Library]"), QStringLiteral("sort_order")));
    SpyTtsEngine* pSpy = makeManager();

    pSortColumn->set(static_cast<double>(TrackModel::SortColumnId::Title));
    pSortOrder->set(0.0);
    m_pManager->slotAnnounceSort();
    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, AnnounceSort_UnknownColumnSilent) {
    auto pSortColumn = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Library]"), QStringLiteral("sort_column")));
    auto pSortOrder = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Library]"), QStringLiteral("sort_order")));
    SpyTtsEngine* pSpy = makeManager();

    // Invalid / internal columns have no spoken name and must stay silent.
    pSortColumn->set(static_cast<double>(TrackModel::SortColumnId::Invalid));
    pSortOrder->set(0.0);
    m_pManager->slotAnnounceSort();
    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, AnnounceSort_ColumnNameMapping) {
    // Spot-check the static name mapping used to build the announcement.
    EXPECT_QSTRING_EQ("title",
            AnnouncementManager::sortColumnName(TrackModel::SortColumnId::Title));
    EXPECT_QSTRING_EQ("BPM",
            AnnouncementManager::sortColumnName(TrackModel::SortColumnId::Bpm));
    EXPECT_QSTRING_EQ("album artist",
            AnnouncementManager::sortColumnName(TrackModel::SortColumnId::AlbumArtist));
    EXPECT_TRUE(AnnouncementManager::sortColumnName(
            TrackModel::SortColumnId::Invalid)
            .isEmpty());
}

TEST_F(AnnouncementManagerTest, AnnounceSearch_UpdatesPendingTextBeforeAnnounce) {
    // If the text changes multiple times before the debounce fires, only the
    // final value should be announced.
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotSearchTextChanged(QStringLiteral("ho"));
    m_pManager->slotSearchTextChanged(QStringLiteral("hou"));
    m_pManager->slotSearchTextChanged(QStringLiteral("house"));
    m_pManager->slotAnnounceSearch();
    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Searching: house", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// Pfl / cue-button announcements
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerPlaystateTest, PflOn_AnnouncesCue) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPfl(1.0);

    EXPECT_EQ(1, pSpy->callCount);
    // deckIndex is -1 in this harness, so deckName() falls back to the group.
    EXPECT_QSTRING_EQ("[TestChannel1] headphone cue on", pSpy->lastText);
}

TEST_F(AnnouncementManagerPlaystateTest, PflOff_AnnouncesCueOff) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPfl(1.0);
    pSpy->callCount = 0;
    pSpy->lastText.clear();
    setPfl(0.0);

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("[TestChannel1] headphone cue off", pSpy->lastText);
}

TEST_F(AnnouncementManagerPlaystateTest, PflOn_SettingDisabled_Silent) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceCue")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPfl(1.0);

    EXPECT_EQ(0, pSpy->callCount);
}

// ---------------------------------------------------------------------------
// AnnouncePlay / AnnounceCue independence (regression: they were previously
// gated by the same setting, making them impossible to control separately)
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerPlaystateTest, AnnouncePlayDisabled_CueStillSpoken) {
    // Disabling AnnouncePlay must not silence cue-button announcements.
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnouncePlay")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPfl(1.0);

    EXPECT_EQ(1, pSpy->callCount)
            << "Cue was silenced when AnnouncePlay was disabled — "
               "the two settings must be independent";
    EXPECT_QSTRING_EQ("[TestChannel1] headphone cue on", pSpy->lastText);
}

TEST_F(AnnouncementManagerPlaystateTest, AnnounceCueDisabled_PlayStillSpoken) {
    // Disabling AnnounceCue must not silence play announcements.
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceCue")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    setupGroup(/*hasTrack=*/true);

    setPlay(1.0);

    EXPECT_EQ(1, pSpy->callCount)
            << "Play was silenced when AnnounceCue was disabled — "
               "the two settings must be independent";
    EXPECT_QSTRING_EQ("Playing", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// Eject confirmation / eject-blocked-while-playing (issue #66)
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerPlaystateTest, Eject_AnnouncesEjected) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup(/*hasTrack=*/true);

    pressEject();

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("[TestChannel1] track ejected", pSpy->lastText);
}

TEST_F(AnnouncementManagerPlaystateTest, Eject_NoTrackLoaded_Silent) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup(/*hasTrack=*/false);

    pressEject();

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerPlaystateTest, Eject_SettingDisabled_Silent) {
    // The eject confirmation reuses AnnounceTrackLoad (successful loads and
    // ejects are the same "what's on this deck now" family of feedback).
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceTrackLoad")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    setupGroup(/*hasTrack=*/true);

    pressEject();

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerPlaystateTest, Eject_WhilePlaying_AnnouncesBlocked) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup(/*hasTrack=*/true);
    setPlay(1.0);
    pSpy->callCount = 0;
    pSpy->lastText.clear();

    pressEject();

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("[TestChannel1] is playing, eject blocked. Stop the deck first.",
            pSpy->lastText);
}

TEST_F(AnnouncementManagerPlaystateTest, Eject_WhilePlaying_BlockedEvenWithSettingDisabled) {
    // The blocked message is a safety signal (mirrors the load-blocked
    // announcement in WTrackTableView, which is also unconditional), not a
    // stylistic preference, so it is not gated by AnnounceTrackLoad.
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceTrackLoad")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    setupGroup(/*hasTrack=*/true);
    setPlay(1.0);
    pSpy->callCount = 0;
    pSpy->lastText.clear();

    pressEject();

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("[TestChannel1] is playing, eject blocked. Stop the deck first.",
            pSpy->lastText);
}

// ---------------------------------------------------------------------------
// Per-deck gain-staging clipping (issue #66)
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerPlaystateTest, ChannelClipping_Announced) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPeakIndicator(1.0);

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("[TestChannel1] clipping", pSpy->lastText);
}

TEST_F(AnnouncementManagerPlaystateTest, ChannelClipping_SettingDisabled_Silent) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceClipping")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPeakIndicator(1.0);

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerPlaystateTest, ChannelClipping_ThrottledOnRepeatedPeaks) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPeakIndicator(1.0);
    ASSERT_EQ(1, pSpy->callCount);

    setPeakIndicator(0.0);
    setPeakIndicator(1.0);

    EXPECT_EQ(1, pSpy->callCount)
            << "per-channel clipping warning must be throttled during sustained clipping";
}

// ---------------------------------------------------------------------------
// Sidebar item deduplication
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, SidebarItemActivated_EmptyTitle_Silent) {
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotSidebarItemActivated(QString());
    EXPECT_EQ(0, pSpy->callCount)
            << "Empty sidebar title must not produce an announcement";
}

TEST_F(AnnouncementManagerTest, AnnounceSelection_RapidChanges_OnlyLastTrackSpoken) {
    // If the user arrows quickly through tracks, only the last one should be
    // announced when the debounce fires — not each intermediate selection.
    SpyTtsEngine* pSpy = makeManager();
    focusTrackList(pSpy);

    m_pManager->slotTrackSelected(makeTrack(QStringLiteral("Artist 1"), QStringLiteral("First")));
    m_pManager->slotTrackSelected(makeTrack(QStringLiteral("Artist 2"), QStringLiteral("Second")));
    m_pManager->slotTrackSelected(makeTrack(QStringLiteral("Artist 3"), QStringLiteral("Third")));
    m_pManager->slotAnnounceSelectedTrack(); // fire debounce synchronously

    EXPECT_EQ(1, pSpy->callCount)
            << "Expected one announcement for the last selected track, "
               "got "
            << pSpy->callCount;
    EXPECT_QSTRING_EQ("Artist 3, Third", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, SidebarItemActivated_DuplicateSuppressed) {
    SpyTtsEngine* pSpy = makeManager();

    m_pManager->slotSidebarItemActivated(QStringLiteral("My Set"));
    m_pManager->slotSidebarItemActivated(QStringLiteral("My Set"));

    EXPECT_EQ(1, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, SidebarItemActivated_DifferentTitleAfterDupAnnounces) {
    SpyTtsEngine* pSpy = makeManager();

    m_pManager->slotSidebarItemActivated(QStringLiteral("My Set"));
    m_pManager->slotSidebarItemActivated(QStringLiteral("Another Set"));

    EXPECT_EQ(2, pSpy->callCount);
    EXPECT_QSTRING_EQ("Another Set", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, SidebarItemActivated_DedupResetWhenSidebarRefocused) {
    SpyTtsEngine* pSpy = makeManager();

    m_pManager->slotSidebarItemActivated(QStringLiteral("My Set"));
    pSpy->callCount = 0;

    // Re-entering sidebar from another panel resets the dedup state.
    m_pManager->slotLibraryFocusChanged(
            static_cast<double>(FocusWidget::TracksTable));
    m_pManager->slotLibraryFocusChanged(
            static_cast<double>(FocusWidget::Sidebar));
    pSpy->callCount = 0; // discard "Track list" and "Sidebar" focus announcements

    m_pManager->slotSidebarItemActivated(QStringLiteral("My Set"));

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("My Set", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// TTS route sync — uses the 3-arg test constructor with a real EngineTts sink
// so that speak() can drive m_pTtsSink->setRoute() and we can inspect the
// resulting [Tts],route_to_main control object.
// ---------------------------------------------------------------------------

class AnnouncementManagerRouteSyncTest : public AnnouncementManagerTest {
  protected:
    static constexpr const char* kSinkGroup = "[AnnounceMgrRouteSyncTest]";

    void SetUp() override {
        AnnouncementManagerTest::SetUp();
        m_pAppSampleRate = std::make_unique<ControlObject>(
                ConfigKey(QStringLiteral("[App]"), QStringLiteral("samplerate")));
        m_pAppSampleRate->set(44100.0);
        m_pEngineTts = std::make_unique<EngineTts>(kSinkGroup);
    }

    SpyTtsEngine* makeManagerWithSink() {
        auto spy = std::make_unique<SpyTtsEngine>();
        SpyTtsEngine* pSpy = spy.get();
        m_pManager = std::make_unique<AnnouncementManager>(
                nullptr,
                m_pPlayerManager.get(),
                config(),
                std::move(spy),
                m_pEngineTts.get(),
                nullptr);
        return pSpy;
    }

    double readRouteControl() const {
        ControlProxy cp(QLatin1String(kSinkGroup),
                QStringLiteral("route_to_main"),
                nullptr,
                ControlFlag::AllowMissingOrInvalid);
        return cp.get();
    }

    std::unique_ptr<ControlObject> m_pAppSampleRate;
    std::unique_ptr<EngineTts> m_pEngineTts;
};

TEST_F(AnnouncementManagerRouteSyncTest, Speak_SyncsRouteToMain) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("TtsRoute")), 1);
    SpyTtsEngine* pSpy = makeManagerWithSink();
    m_pManager->slotSoundDevicesReady(); // audio confirmed running: skinLoaded speaks immediately

    m_pManager->slotSkinLoaded(); // triggers speak()

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_EQ(1.0, readRouteControl());
}

TEST_F(AnnouncementManagerRouteSyncTest, Speak_SyncsRouteToHeadphones) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("TtsRoute")), 0);
    SpyTtsEngine* pSpy = makeManagerWithSink();
    m_pManager->slotSoundDevicesReady();

    m_pManager->slotSkinLoaded();

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_EQ(0.0, readRouteControl());
}

TEST_F(AnnouncementManagerRouteSyncTest, Speak_SkipsRouteSyncWhenUnchanged) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("TtsRoute")), 1);
    SpyTtsEngine* pSpy = makeManagerWithSink();
    m_pManager->slotSoundDevicesReady();

    m_pManager->slotSkinLoaded(); // first speak — sets route
    // Change the CO back to 0 externally to detect a second setRoute() call.
    m_pEngineTts->setRoute(0);
    EXPECT_EQ(0.0, readRouteControl());

    m_pManager->slotSkinLoaded(); // second speak — same setting, no re-sync
    // If route was re-synced, control would be back at 1. If not, still 0.
    EXPECT_EQ(0.0, readRouteControl());
    EXPECT_EQ(2, pSpy->callCount);
}

TEST_F(AnnouncementManagerRouteSyncTest, Speak_SkippedWhenSinkUserDisabled) {
    // When the user disables TTS via the menu/shortcut, the EngineTts enabled CO
    // goes to 0. speak() must bail before calling say() so no synthesis happens.
    SpyTtsEngine* pSpy = makeManagerWithSink();
    m_pManager->slotSoundDevicesReady(); // so slotSkinLoaded() below actually calls speak()

    ControlProxy enabledCO(QLatin1String(kSinkGroup),
            QStringLiteral("enabled"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
    enabledCO.set(0.0);
    ASSERT_FALSE(m_pEngineTts->isUserEnabled());

    m_pManager->slotSkinLoaded(); // would normally speak "Mixxx ready"

    EXPECT_EQ(0, pSpy->callCount)
            << "speak() was not skipped when the engine sink is user-disabled";
}

TEST_F(AnnouncementManagerRouteSyncTest, Speak_AfterSinkDestroyed_DoesNotCrash) {
    // Regression test for issue #30: when the EngineTts sink is destroyed while
    // the AnnouncementManager still holds a raw pointer to it (shutdown), the
    // manager must drop the pointer and stop speaking instead of dereferencing
    // freed memory. Destroying the sink emits sinkDestroyed(); a subsequent
    // speak() must bail without calling into the torn-down sink.
    SpyTtsEngine* pSpy = makeManagerWithSink();
    m_pManager->slotSoundDevicesReady(); // so slotSkinLoaded() below actually calls speak()

    // Destroy the engine sink. ~EngineTts() emits sinkDestroyed(), which the
    // manager is connected to, so it nulls its raw pointer and flags the sink
    // as gone.
    m_pEngineTts.reset();

    // A control change (or any other path) firing speak() after the sink is
    // gone must not dereference the destroyed EngineTts.
    m_pManager->slotSkinLoaded();

    EXPECT_EQ(0, pSpy->callCount)
            << "speak() must not synthesize after the engine sink is destroyed";
}

// ---------------------------------------------------------------------------
// On-demand deck status ([ChannelN],tts_status) and repeat ([Tts],repeat)
// ---------------------------------------------------------------------------

class AnnouncementManagerStatusTest : public AnnouncementManagerPlaystateTest {
  protected:
    // Create the deck COs formatDeckStatus() reads. Values are set by tests.
    void createStatusControls() {
        m_pDuration = std::make_unique<ControlObject>(
                ConfigKey(QLatin1String(kGroup), QStringLiteral("duration")));
        m_pPlayPos = std::make_unique<ControlObject>(
                ConfigKey(QLatin1String(kGroup), QStringLiteral("playposition")));
        m_pBpm = std::make_unique<ControlObject>(
                ConfigKey(QLatin1String(kGroup), QStringLiteral("bpm")));
        m_pRateRatio = std::make_unique<ControlObject>(
                ConfigKey(QLatin1String(kGroup), QStringLiteral("rate_ratio")));
        m_pRateRatio->set(1.0);
    }

    std::unique_ptr<ControlObject> m_pDuration;
    std::unique_ptr<ControlObject> m_pPlayPos;
    std::unique_ptr<ControlObject> m_pBpm;
    std::unique_ptr<ControlObject> m_pRateRatio;
};

TEST_F(AnnouncementManagerStatusTest, FormatDeckStatus_NoTrackLoaded) {
    makeManager();

    EXPECT_QSTRING_EQ("Deck, Alpha. No track loaded.",
            m_pManager->formatDeckStatus(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, FormatDeckStatus_FullStatus) {
    makeManager();
    setupGroup(); // play/end_of_track/pfl COs + hasTrack = true
    createStatusControls();

    m_pPlay->set(1.0);
    m_pDuration->set(180.0);
    m_pPlayPos->set(0.5); // 90 seconds remaining
    m_pBpm->set(128.4);
    m_pRateRatio->set(1.02); // pitch up 2 percent

    EXPECT_QSTRING_EQ(
            "Deck, Alpha. Playing. 1 minute 30 seconds remaining. 128 B P M. "
            "Pitch up 2 percent.",
            m_pManager->formatDeckStatus(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, FormatDeckStatus_StoppedPitchDown) {
    makeManager();
    setupGroup();
    createStatusControls();

    m_pPlay->set(0.0);
    m_pDuration->set(45.0);
    m_pPlayPos->set(0.0);
    m_pRateRatio->set(0.95); // pitch down 5 percent

    EXPECT_QSTRING_EQ(
            "Deck, Bravo. Stopped. 45 seconds remaining. Pitch down 5 percent.",
            m_pManager->formatDeckStatus(QString::fromLatin1(kGroup), 1));
}

TEST_F(AnnouncementManagerStatusTest, StatusButton_TriggersAnnouncement) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup(); // connectGroupControls creates [TestChannel1],tts_status

    ControlProxy statusButton(QLatin1String(kGroup),
            QStringLiteral("tts_status"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
    statusButton.set(1.0);
    QCoreApplication::processEvents();

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_TRUE(pSpy->lastText.contains(QStringLiteral("Stopped")))
            << "status readout missing playback state: "
            << pSpy->lastText.toStdString();
}

TEST_F(AnnouncementManagerStatusTest, RepeatButton_RepeatsLastAnnouncement) {
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotSoundDevicesReady(); // so slotSkinLoaded() below actually speaks

    m_pManager->slotSkinLoaded(); // speaks "Mixxx ready"
    ASSERT_EQ(1, pSpy->callCount);

    ControlProxy repeatButton(QStringLiteral("[Tts]"),
            QStringLiteral("repeat"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
    repeatButton.set(1.0);
    QCoreApplication::processEvents();

    EXPECT_EQ(2, pSpy->callCount);
    EXPECT_QSTRING_EQ("Mixxx ready", pSpy->lastText);
}

TEST_F(AnnouncementManagerStatusTest, RepeatButton_NothingSpokenYet_Silent) {
    SpyTtsEngine* pSpy = makeManager();

    ControlProxy repeatButton(QStringLiteral("[Tts]"),
            QStringLiteral("repeat"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
    repeatButton.set(1.0);
    QCoreApplication::processEvents();

    EXPECT_EQ(0, pSpy->callCount);
}

// ---------------------------------------------------------------------------
// Performance announcements: sync/keylock/quantize, loops, hotcues, tempo,
// mixer moves, recording.
// ---------------------------------------------------------------------------

class AnnouncementManagerPerformanceTest : public AnnouncementManagerPlaystateTest {
  protected:
    // COs must exist before connectGroupControls() so the proxies attach.
    void createPerformanceControls() {
        m_pSync = std::make_unique<ControlObject>(
                ConfigKey(QLatin1String(kGroup), QStringLiteral("sync_enabled")));
        m_pLoopEnabled = std::make_unique<ControlObject>(
                ConfigKey(QLatin1String(kGroup), QStringLiteral("loop_enabled")));
        m_pBeatloopSize = std::make_unique<ControlObject>(
                ConfigKey(QLatin1String(kGroup), QStringLiteral("beatloop_size")));
        m_pHotcue1Status = std::make_unique<ControlObject>(
                ConfigKey(QLatin1String(kGroup), QStringLiteral("hotcue_1_status")));
        m_pRateRatio = std::make_unique<ControlObject>(
                ConfigKey(QLatin1String(kGroup), QStringLiteral("rate_ratio")));
        m_pRateRatio->set(1.0);
        m_pVolume = std::make_unique<ControlObject>(
                ConfigKey(QLatin1String(kGroup), QStringLiteral("volume")));
    }

    std::unique_ptr<ControlObject> m_pSync;
    std::unique_ptr<ControlObject> m_pLoopEnabled;
    std::unique_ptr<ControlObject> m_pBeatloopSize;
    std::unique_ptr<ControlObject> m_pHotcue1Status;
    std::unique_ptr<ControlObject> m_pRateRatio;
    std::unique_ptr<ControlObject> m_pVolume;
};

namespace {
// Spin the event loop long enough for the sync latch-probe timer (450 ms)
// to fire.
void waitPastSyncLatchWindow() {
    QEventLoop loop;
    QTimer::singleShot(600, &loop, &QEventLoop::quit);
    loop.exec();
}

// Spin the event loop long enough for the shared control-debounce timer
// (400 ms, see kControlDebounceMs) to actually fire via its real QTimer
// timeout, instead of tests calling slotAnnouncePendingControl() directly.
void waitPastControlDebounce() {
    QEventLoop loop;
    QTimer::singleShot(500, &loop, &QEventLoop::quit);
    loop.exec();
}
} // namespace

TEST_F(AnnouncementManagerPerformanceTest, SyncShortPress_BeatSyncAnnounced) {
    // sync_enabled is hold-to-latch: a short press pulses 1 then reverts to
    // 0 and performs a one-shot beat sync. The pulse must not be narrated as
    // "sync on ... sync off".
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pSync->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, pSpy->callCount)
            << "no announcement until the latch window resolves";

    m_pSync->set(0.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] beat synced. Hold sync to lock", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, BeatjumpSize_AnnouncedDebounced) {
    auto pSize = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("beatjump_size")));
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    pSize->set(4.0);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("[TestChannel1] beat jump size 4", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, BeatjumpForward_AnnouncedWithSize) {
    auto pSize = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("beatjump_size")));
    pSize->set(8.0);
    auto pForward = std::make_unique<ControlObject>(ConfigKey(
            QLatin1String(kGroup), QStringLiteral("beatjump_forward")));
    auto pBackward = std::make_unique<ControlObject>(ConfigKey(
            QLatin1String(kGroup), QStringLiteral("beatjump_backward")));
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    pForward->set(1.0);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("[TestChannel1] jump forward 8 beats", pSpy->lastText);

    pBackward->set(1.0);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("[TestChannel1] jump back 8 beats", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, Beatjump_LoopSettingDisabled_Silent) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceLoop")),
            false);
    auto pSize = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("beatjump_size")));
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    pSize->set(4.0);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_EQ(0, pSpy->callCount);
}

// ---------------------------------------------------------------------------
// Effects announcements: per-effect enables, effect selection, unit routing,
// filter (QuickEffect) preset.
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerPerformanceTest, EffectSlotEnable_Announced) {
    auto pEnabled = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[EffectRack1_EffectUnit1_Effect2]"),
            QStringLiteral("enabled")));
    SpyTtsEngine* pSpy = makeManager(); // proxy attaches in init()

    pEnabled->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Unit 1 effect 2 on", pSpy->lastText);

    pEnabled->set(0.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Unit 1 effect 2 off", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, EffectSlotEnable_ResolverSuppliesName) {
    auto pEnabled = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[EffectRack1_EffectUnit3_Effect1]"),
            QStringLiteral("enabled")));
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->setEffectNameResolvers(
            [](int, int) { return QStringLiteral("Echo"); },
            [](const QString&) { return QString(); });

    pEnabled->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Unit 3 Echo on", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, EffectLoaded_AnnouncedDebounced) {
    auto pLoaded = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[EffectRack1_EffectUnit2_Effect1]"),
            QStringLiteral("loaded_effect")));
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->setEffectNameResolvers(
            [](int, int) { return QStringLiteral("Flanger"); },
            [](const QString&) { return QString(); });

    pLoaded->set(4.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, pSpy->callCount) << "effect selection must be debounced";
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("Unit 2: Flanger loaded", pSpy->lastText);

    pLoaded->set(0.0);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("Unit 2 effect 1 cleared", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, FocusedEffect_Announced) {
    // focused_effect (the DDJ-400's BEAT FX </> paddles) moves which slot in
    // the unit is focused; it does not load/unload an effect, so the
    // loaded_effect observer never fires for it.
    auto pFocused = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[EffectRack1_EffectUnit1]"),
            QStringLiteral("focused_effect")));
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->setEffectNameResolvers(
            [](int, int) { return QStringLiteral("Echo"); },
            [](const QString&) { return QString(); });

    pFocused->set(2.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, pSpy->callCount) << "focused_effect must be debounced";
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("Unit 1: Echo focused", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, FocusedEffect_NoResolverFallsBackToSlotNumber) {
    auto pFocused = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[EffectRack1_EffectUnit2]"),
            QStringLiteral("focused_effect")));
    SpyTtsEngine* pSpy = makeManager();

    pFocused->set(3.0);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("Unit 2: effect 3 focused", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, FocusedEffect_EffectsSettingDisabled_Silent) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceEffects")),
            false);
    auto pFocused = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[EffectRack1_EffectUnit1]"),
            QStringLiteral("focused_effect")));
    SpyTtsEngine* pSpy = makeManager();

    pFocused->set(2.0);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerPerformanceTest, EffectUnitRouting_Announced) {
    auto pRouting = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[EffectRack1_EffectUnit1]"),
            QStringLiteral("group_[TestChannel1]_enable")));
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    pRouting->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] effect unit 1 on", pSpy->lastText);

    pRouting->set(0.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] effect unit 1 off", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, QuickEffectPreset_AnnouncedWithName) {
    auto pPreset = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[QuickEffectRack1_[TestChannel1]]"),
            QStringLiteral("loaded_chain_preset")));
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->setEffectNameResolvers(
            [](int, int) { return QString(); },
            [](const QString&) { return QStringLiteral("Moog Filter"); });
    setupGroup();

    pPreset->set(2.0);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("[TestChannel1] filter: Moog Filter", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, Effects_SettingDisabled_Silent) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"),
                    QStringLiteral("AnnounceEffects")),
            false);
    auto pEnabled = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[EffectRack1_EffectUnit1_Effect1]"),
            QStringLiteral("enabled")));
    SpyTtsEngine* pSpy = makeManager();

    pEnabled->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerPerformanceTest, SyncHold_LockAnnounced) {
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pSync->set(1.0);
    QCoreApplication::processEvents();
    waitPastSyncLatchWindow();
    EXPECT_QSTRING_EQ("[TestChannel1] sync locked", pSpy->lastText);

    m_pSync->set(0.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] sync off", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, SyncToggle_SettingDisabled_Silent) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceSync")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pSync->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerPerformanceTest, LoopOn_AnnouncedWithSize) {
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pBeatloopSize->set(8.0);
    m_pLoopEnabled->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] loop 8 beats", pSpy->lastText);

    m_pLoopEnabled->set(0.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] loop off", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, LoopScale_Halve_AnnouncesNewSize) {
    // loop_scale (the DDJ-400's CUE/LOOP CALL) halves/doubles the loop
    // bounds directly without ever touching beatloop_size, so the
    // beatloop_size observer never fires for it; it must be observed on its
    // own.
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    // bIgnoreNops=false, matching LoopingControl's own loop_scale CO: every
    // press re-fires even if it sets the same scale factor as last time.
    auto pLoopScale = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("loop_scale")), false);
    setupGroup();

    m_pBeatloopSize->set(8.0);
    m_pLoopEnabled->set(1.0);
    QCoreApplication::processEvents();
    pSpy->callCount = 0;

    pLoopScale->set(0.5);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, pSpy->callCount) << "loop scale must be debounced";
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("[TestChannel1] loop size 4", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, LoopScale_RepeatedPresses_TrackActualSize) {
    // Repeated CUE/LOOP CALL presses must keep announcing the real resulting
    // size, not repeat the same stale beatloop_size-derived value.
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    auto pLoopScale = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("loop_scale")), false);
    setupGroup();

    m_pBeatloopSize->set(8.0);
    m_pLoopEnabled->set(1.0);
    QCoreApplication::processEvents();

    pLoopScale->set(0.5);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("[TestChannel1] loop size 4", pSpy->lastText);

    pLoopScale->set(0.5); // CUE/LOOP CALL <- pressed again
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("[TestChannel1] loop size 2", pSpy->lastText);

    pLoopScale->set(2.0); // CUE/LOOP CALL -> pressed
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("[TestChannel1] loop size 4", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, LoopScale_LoopSettingDisabled_Silent) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceLoop")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    auto pLoopScale = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("loop_scale")), false);
    setupGroup();

    m_pBeatloopSize->set(8.0);
    pLoopScale->set(0.5);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerPerformanceTest, HotcueSetAndCleared_Announced) {
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pHotcue1Status->set(1.0); // Status::Set
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] hotcue 1 set", pSpy->lastText);

    m_pHotcue1Status->set(0.0); // Status::Empty
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] hotcue 1 cleared", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, Hotcue_SettingDisabled_Silent) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceHotcue")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pHotcue1Status->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerPerformanceTest, TempoChange_DebouncedThenSpoken) {
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pRateRatio->set(1.05);
    QCoreApplication::processEvents();
    // Name on touch: the fader names itself the moment it moves; the value
    // is debounced until it stops.
    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("[TestChannel1] pitch", pSpy->lastText);

    m_pManager->slotAnnouncePendingControl();
    EXPECT_EQ(2, pSpy->callCount);
    EXPECT_QSTRING_EQ("up 5 percent", pSpy->lastText);
}

// Regression test for issue #114: on real hardware the pitch fader keeps
// moving for a bit (several rapid rate_ratio ticks, each re-arming the
// shared debounce QTimer) before coming to rest, and only *then* does the
// timer's real timeout fire slotAnnouncePendingControl() -- nothing in the
// test manually invokes it. Every other debounce test in this file (see
// TempoChange_DebouncedThenSpoken above) calls slotAnnouncePendingControl()
// directly instead of waiting for the QTimer, which would hide a bug in the
// timer hookup itself. This test drives the real timer via the event loop,
// the way production actually behaves.
TEST_F(AnnouncementManagerPerformanceTest, TempoChange_RealDebounceTimerFiresValue) {
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pRateRatio->set(1.02);
    QCoreApplication::processEvents();
    m_pRateRatio->set(1.03);
    QCoreApplication::processEvents();
    m_pRateRatio->set(1.05);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] pitch", pSpy->lastText) << "name on touch";

    waitPastControlDebounce();
    EXPECT_QSTRING_EQ("up 5 percent", pSpy->lastText)
            << "the debounced value must follow the touch-name once the "
               "fader settles, spoke: "
            << pSpy->lastText.toStdString();
}

// Regression test for issue #114: "Pitch announcements appear stuck on the
// touch-name ('Alpha pitch') and never follow up with the value". Root
// cause: the pitch fader and any other debounced control (a volume knob
// here) shared a single "latest pending announcement" slot. Touching the
// second control before the first one's debounce timer fired silently
// discarded the first control's queued value -- the touch-name was still
// heard (that part is synchronous), but its debounced value never was.
// Each keyed control now gets its own pending slot (see m_pendingControls),
// and a batch of controls that settle in the same debounce window are all
// announced, joined into a single utterance so barge-in can't eat one of
// them either.
TEST_F(AnnouncementManagerPerformanceTest, TempoChange_InterleavedControlDoesNotOverwritePendingValue) {
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pRateRatio->set(1.05);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] pitch", pSpy->lastText) << "name on touch";

    // Before the pitch fader's debounce settles, a second, unrelated
    // debounced control is touched too -- e.g. the DJ reaching for a volume
    // knob mid-blend.
    m_pVolume->set(0.75);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] volume", pSpy->lastText)
            << "name on touch for the second control";

    waitPastControlDebounce();
    EXPECT_TRUE(pSpy->lastText.contains(QStringLiteral("up 5 percent")))
            << "pitch's debounced value must not be silently discarded by "
               "the later volume touch, spoke: "
            << pSpy->lastText.toStdString();
    EXPECT_TRUE(pSpy->lastText.contains(QStringLiteral("three quarters")))
            << "the volume's own debounced value must still be heard too, "
               "spoke: "
            << pSpy->lastText.toStdString();
}

TEST_F(AnnouncementManagerPerformanceTest, VolumeChange_MixerOnByDefault_Spoken) {
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pVolume->set(0.5);
    QCoreApplication::processEvents();
    // Name on touch: the fader names itself the moment it moves; the value
    // is debounced until it stops.
    EXPECT_QSTRING_EQ("[TestChannel1] volume", pSpy->lastText);
    m_pManager->slotAnnouncePendingControl();
    EXPECT_EQ(2, pSpy->callCount)
            << "mixer announcements are on by default (AnnounceMixer defaults on)";
    EXPECT_QSTRING_EQ("a half", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, VolumeChange_MixerDisabled_Silent) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pVolume->set(0.5);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_EQ(0, pSpy->callCount)
            << "mixer announcements can be turned off (AnnounceMixer=false)";
}

TEST_F(AnnouncementManagerPerformanceTest, VolumeChange_MixerEnabled_Spoken) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pVolume->set(0.5);
    QCoreApplication::processEvents();
    // Two utterances: the name on touch, the value at rest.
    EXPECT_QSTRING_EQ("[TestChannel1] volume", pSpy->lastText);
    m_pManager->slotAnnouncePendingControl();
    EXPECT_EQ(2, pSpy->callCount);
    EXPECT_QSTRING_EQ("a half", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, VolumeChange_SpokenAsFraction) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pVolume->set(0.75);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("three quarters", pSpy->lastText);

    // Default fraction detail is eighths: 5/16 snaps to the nearest eighth
    // (2.5 eighths rounds away from zero to 3). The same control moving
    // again within the context window speaks only the new value.
    m_pVolume->set(0.3125); // 5/16
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("3 eighths", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, FractionDetail_SixteenthsWhenConfigured) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"),
                    QStringLiteral("MixerFractionDetail")),
            2);
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pVolume->set(0.3125); // 5/16 spoken exactly at sixteenth detail
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("5 sixteenths", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, FractionDetail_QuartersWhenConfigured) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"),
                    QStringLiteral("MixerFractionDetail")),
            0);
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pVolume->set(0.3125); // 5/16 snaps to the nearest quarter
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("a quarter", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, FilterChange_CenterSplitFraction) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    SpyTtsEngine* pSpy = makeManager();
    // The QuickEffect super knob lives in its own group.
    auto pSuper = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[QuickEffectRack1_[TestChannel1]]"),
            QStringLiteral("super1")));
    pSuper->set(0.5);
    setupGroup();

    pSuper->set(0.25); // halfway toward full cut = minus a half
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("minus a half", pSpy->lastText);

    // Same knob still moving: name-once, value only.
    pSuper->set(0.5);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("center", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, MainVolume_Announced) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    auto pMainGain = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Master]"), QStringLiteral("gain")));
    pMainGain->set(1.0);
    SpyTtsEngine* pSpy = makeManager(); // proxy attaches in init()

    // [Master],gain is a ControlAudioTaperPot with neutral parameter 0.5; the
    // test CO has no taper behavior attached, so getParameter() is identity
    // and the raw value IS the knob parameter here. Spoken as plain knob
    // travel — a center-split "minus" readout made testers think the volume
    // itself had gone negative.
    pMainGain->set(0.25);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Main volume", pSpy->lastText) << "name on touch";
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("a quarter", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, Recording_StartAndStop_Announced) {
    // The recording proxy attaches in init(), so the CO must exist before
    // the manager is created.
    auto pRecordingStatus = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Recording]"), QStringLiteral("status")));
    SpyTtsEngine* pSpy = makeManager();

    pRecordingStatus->set(2.0); // RECORD_ON
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Recording started", pSpy->lastText);

    pRecordingStatus->set(0.0); // RECORD_OFF
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Recording stopped", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// Granular info readouts (tts_time / tts_bpm / tts_key / tts_bar)
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerStatusTest, FormatTimeRemaining_NoTrack) {
    makeManager();
    EXPECT_QSTRING_EQ("Deck, Alpha. No track loaded.",
            m_pManager->formatTimeRemaining(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, FormatTimeRemaining_WithTrack) {
    makeManager();
    setupGroup();
    createStatusControls();
    m_pDuration->set(200.0);
    m_pPlayPos->set(0.35); // 130 seconds remaining

    EXPECT_QSTRING_EQ("Deck, Alpha. 2 minutes 10 seconds remaining.",
            m_pManager->formatTimeRemaining(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, FormatBpm_RoundsAndSpells) {
    makeManager();
    createStatusControls();
    m_pBpm->set(174.4);

    EXPECT_QSTRING_EQ("Deck, Alpha. 174 B P M.",
            m_pManager->formatBpm(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, FormatBpm_NoneAvailable) {
    makeManager();
    EXPECT_QSTRING_EQ("Deck, Alpha. No B P M.",
            m_pManager->formatBpm(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, FormatKey_SpokenName) {
    makeManager();
    auto pKey = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("key")));
    pKey->set(22.0); // ChromaticKey A_MINOR

    EXPECT_QSTRING_EQ("Deck, Alpha. Key: A Minor.",
            m_pManager->formatKey(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, FormatKey_Unknown) {
    makeManager();
    EXPECT_QSTRING_EQ("Deck, Alpha. Key unknown.",
            m_pManager->formatKey(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, FormatKey_LancelotNotation) {
    makeManager();
    auto pNotation = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[Library]"), QStringLiteral("key_notation")));
    pNotation->set(static_cast<double>(KeyUtils::KeyNotation::Lancelot));
    auto pKey = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("key")));
    pKey->set(1.0); // ChromaticKey C_MAJOR; Lancelot (Camelot) code is "8B".

    EXPECT_QSTRING_EQ("Deck, Alpha. Key: 8, Bravo.",
            m_pManager->formatKey(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, FormatKey_OpenKeyNotation) {
    makeManager();
    auto pNotation = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[Library]"), QStringLiteral("key_notation")));
    pNotation->set(static_cast<double>(KeyUtils::KeyNotation::OpenKey));
    auto pKey = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("key")));
    pKey->set(12.0); // ChromaticKey B_MAJOR; Open Key code is "6d".

    EXPECT_QSTRING_EQ("Deck, Alpha. Key: 6, Delta.",
            m_pManager->formatKey(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, FormatKey_LancelotAndTraditionalNotation) {
    makeManager();
    auto pNotation = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[Library]"), QStringLiteral("key_notation")));
    pNotation->set(static_cast<double>(KeyUtils::KeyNotation::LancelotAndTraditional));
    auto pKey = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("key")));
    pKey->set(1.0); // ChromaticKey C_MAJOR

    EXPECT_QSTRING_EQ("Deck, Alpha. Key: 8, Bravo. C Major.",
            m_pManager->formatKey(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, FormatBarPosition_NoDeck) {
    // The stub PlayerManager has no decks, so no track can be resolved.
    makeManager();
    EXPECT_QSTRING_EQ("Deck, Alpha. No track loaded.",
            m_pManager->formatBarPosition(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, InfoButton_TriggersSingleFactReadout) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup(); // creates the tts_* trigger buttons for the group

    ControlProxy bpmButton(QLatin1String(kGroup),
            QStringLiteral("tts_bpm"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
    bpmButton.set(1.0);
    QCoreApplication::processEvents();

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_TRUE(pSpy->lastText.contains(QStringLiteral("B P M")))
            << pSpy->lastText.toStdString();
}

// ---------------------------------------------------------------------------
// Blind-tester feedback batch: end-of-track time, cue preview, back-to-start,
// tempo announcing the resulting BPM.
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerPerformanceTest, EndOfTrack_IncludesTimeRemaining) {
    SpyTtsEngine* pSpy = makeManager();
    auto pDuration = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("duration")));
    auto pPlayPos = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("playposition")));
    setupGroup();
    pDuration->set(300.0);
    pPlayPos->set(0.85); // 45 seconds remaining

    setEndOfTrack(1.0);

    EXPECT_QSTRING_EQ("End of track. 45 seconds remaining.", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, CuePreview_SaysCueAndSuppressesStop) {
    SpyTtsEngine* pSpy = makeManager();
    auto pCueDefault = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("cue_default")));
    setupGroup();

    pCueDefault->set(1.0); // user holds the cue button
    setPlay(1.0);          // CueControl starts the preview
    EXPECT_QSTRING_EQ("Cue", pSpy->lastText);
    const int callsAfterCue = pSpy->callCount;

    pCueDefault->set(0.0); // release
    setPlay(0.0);          // preview ends
    EXPECT_EQ(callsAfterCue, pSpy->callCount)
            << "cue-preview end must not announce Stopped; spoke: "
            << pSpy->lastText.toStdString();
}

TEST_F(AnnouncementManagerPerformanceTest, NormalPlay_StillSaysPlaying) {
    SpyTtsEngine* pSpy = makeManager();
    auto pCueDefault = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("cue_default")));
    setupGroup();

    setPlay(1.0); // cue button not held
    EXPECT_QSTRING_EQ("Playing", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, BackToStart_Announced) {
    SpyTtsEngine* pSpy = makeManager();
    auto pStart = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("start")));
    setupGroup(); // proxies attach after the CO exists

    pStart->set(1.0);
    QCoreApplication::processEvents();

    EXPECT_QSTRING_EQ("[TestChannel1] back to start", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, BackToStart_StartStopControl_Announced) {
    // start_stop (jump to start without playing) is the DDJ-400's Shift+CUE
    // remap; it must be narrated the same way as start/cue_gotoandstop.
    SpyTtsEngine* pSpy = makeManager();
    auto pStartStop = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("start_stop")));
    setupGroup();

    pStartStop->set(1.0);
    QCoreApplication::processEvents();

    EXPECT_QSTRING_EQ("[TestChannel1] back to start", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, TempoChange_IncludesNewBpm) {
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    auto pBpm = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("bpm")));
    setupGroup();
    pBpm->set(130.9);

    m_pRateRatio->set(1.05);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();

    EXPECT_QSTRING_EQ("up 5 percent. 131 B P M", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// Deck naming and concise-announcement preferences
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerStatusTest, DeckNamesAsNumbers_Preference) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"),
                    QStringLiteral("DeckNamesAsNumbers")),
            true);
    makeManager();
    createStatusControls();
    m_pBpm->set(174.0);

    EXPECT_QSTRING_EQ("Deck 1. 174 B P M.",
            m_pManager->formatBpm(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, Concise_SingleFactsSpeakValueOnly) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"),
                    QStringLiteral("ConciseAnnouncements")),
            true);
    makeManager();
    createStatusControls();
    m_pBpm->set(174.0);
    auto pKey = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("key")));
    pKey->set(22.0); // A minor

    EXPECT_QSTRING_EQ("174.",
            m_pManager->formatBpm(QString::fromLatin1(kGroup), 0));
    EXPECT_QSTRING_EQ("A Minor.",
            m_pManager->formatKey(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, Concise_TimeRemainingDropsDeck) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"),
                    QStringLiteral("ConciseAnnouncements")),
            true);
    makeManager();
    setupGroup();
    createStatusControls();
    m_pDuration->set(90.0);
    m_pPlayPos->set(0.0);

    EXPECT_QSTRING_EQ("1 minute 30 seconds remaining.",
            m_pManager->formatTimeRemaining(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerTest, CrossfaderLock_Announced) {
    auto pLock = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[Master]"), QStringLiteral("crossfader_lock")));
    SpyTtsEngine* pSpy = makeManager(); // proxy attaches in init()

    pLock->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Crossfader locked", pSpy->lastText);

    pLock->set(0.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Crossfader unlocked", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, CrossfaderMove_WhileLocked_Silent) {
    // While the lock is on the engine ignores the crossfader control, so a
    // position readout would describe a fader that isn't doing anything.
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    auto pLock = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[Master]"), QStringLiteral("crossfader_lock")));
    auto pCrossfader = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[Master]"), QStringLiteral("crossfader")));
    SpyTtsEngine* pSpy = makeManager(); // proxies attach in init()

    // Unlocked: moving the crossfader names it, then speaks its position.
    pCrossfader->set(-1.0);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("left full", pSpy->lastText);

    pLock->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Crossfader locked", pSpy->lastText);
    const int callsAfterLock = pSpy->callCount;

    // Locked: position changes stay silent.
    pCrossfader->set(1.0);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_EQ(callsAfterLock, pSpy->callCount)
            << "crossfader position must not be announced while locked";

    // Unlocking restores the readout ("Crossfader unlocked" reset the
    // spoken context, so the move names the fader again before the value).
    pLock->set(0.0);
    QCoreApplication::processEvents();
    pCrossfader->set(0.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Crossfader", pSpy->lastText);
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("center", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, DisableTouchScratch_Announced) {
    auto pLock = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[Master]"), QStringLiteral("disable_touch_scratch")));
    SpyTtsEngine* pSpy = makeManager(); // proxy attaches in init()

    pLock->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Jog wheel touch locked", pSpy->lastText);

    pLock->set(0.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Jog wheel touch unlocked", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// Tier 2 enrichment: cue set, hotcue pressed, loop size, trim, effect knobs,
// announce-while-moving.
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerPerformanceTest, CueSet_Announced) {
    SpyTtsEngine* pSpy = makeManager();
    auto pCueSet = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("cue_set")));
    setupGroup();

    pCueSet->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] cue set", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, HotcuePressed_AnnouncedWhenSet) {
    SpyTtsEngine* pSpy = makeManager();
    auto pActivate = std::make_unique<ControlObject>(ConfigKey(
            QLatin1String(kGroup), QStringLiteral("hotcue_1_activate")));
    createPerformanceControls(); // includes hotcue_1_status
    setupGroup();

    m_pHotcue1Status->set(1.0); // set (announces "set")
    QCoreApplication::processEvents();

    pActivate->set(1.0); // press the pad
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] hotcue 1", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, HotcuePressed_EmptyPad_NoDoubleSpeak) {
    SpyTtsEngine* pSpy = makeManager();
    auto pActivate = std::make_unique<ControlObject>(ConfigKey(
            QLatin1String(kGroup), QStringLiteral("hotcue_1_activate")));
    createPerformanceControls();
    setupGroup();

    // Pressing an empty pad sets the cue: activate fires with status still 0.
    pActivate->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, pSpy->callCount)
            << "activate on an empty pad must stay quiet (status observer "
               "announces the set): "
            << pSpy->lastText.toStdString();
}

TEST_F(AnnouncementManagerPerformanceTest, LoopSizeChange_Debounced) {
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pBeatloopSize->set(8.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, pSpy->callCount) << "loop size must be debounced";
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("[TestChannel1] loop size 8", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, Trim_CenterSplit) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    SpyTtsEngine* pSpy = makeManager();
    auto pPregain = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("pregain")));
    pPregain->set(0.5); // neutral parameter: unity gain, center
    setupGroup();

    // pregain is a ControlAudioTaperPot with neutral parameter 0.5; the test
    // CO has no taper behavior, so getParameter() is identity and 0.375 is a
    // quarter below center on the -1..1 split scale ((0.375-0.5)*2 = -0.25).
    pPregain->set(0.375); // a quarter below unity
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("minus a quarter", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// Regression: volume/trim/gain readouts must speak the fader/knob POSITION,
// not the dB-tapered gain value. A physical half-way fader is roughly a
// quarter of the way through the linear gain range, so reading get() instead
// of getParameter() spoke "a quarter" for a fader the user had put at "half".
// These use a real ControlAudioTaperPot (not a plain ControlObject) so the
// taper behavior is actually exercised, unlike the other volume/trim tests.
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerPerformanceTest, VolumeChange_TaperedControl_HalfFaderIsAHalf) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    // Mirrors EngineMixer's real volume control: ControlAudioTaperPot(-20, 0, 1).
    // Must exist before makeManager() so connectGroupControls()'s ControlProxy
    // binds to it (a proxy constructed before the control exists never
    // retroactively attaches).
    auto pVolume = std::make_unique<ControlAudioTaperPot>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("volume")), -20, 0, 1);
    SpyTtsEngine* pSpy = makeManager();
    setupGroup(); // wires up connectGroupControls() for kGroup, incl. volume

    pVolume->setParameter(0.5);
    ASSERT_NE(0.5, pVolume->get())
            << "precondition: the dB taper must make value diverge from "
               "parameter at the fader's half-way position, or this test "
               "isn't exercising the bug";
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("a half", pSpy->lastText)
            << "announcement must read the fader position, not the tapered "
               "gain value: "
            << pSpy->lastText.toStdString();
}

TEST_F(AnnouncementManagerPerformanceTest, MainVolume_TaperedControl_ReadsKnobTravel) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    // Mirrors EngineMixer's real [Master],gain control.
    auto pMainGain = std::make_unique<ControlAudioTaperPot>(
            ConfigKey(QStringLiteral("[Master]"), QStringLiteral("gain")), -14, 14, 0.5);
    SpyTtsEngine* pSpy = makeManager();

    // The constructor already sets parameter to neutralParameter (0.5), so
    // setParameter(0.5) alone would be a no-op that never fires
    // valueChanged. Move away first to force a real transition back.
    pMainGain->setParameter(0.0);
    pMainGain->setParameter(0.5); // knob centered = unity gain = 0 dB
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    // Plain knob travel, read from the parameter (not the dB-tapered gain
    // value): center of the knob is "a half", not "center" or a taper echo.
    EXPECT_QSTRING_EQ("a half", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, EffectUnitMix_Announced) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    auto pMix = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[EffectRack1_EffectUnit1]"), QStringLiteral("mix")));
    SpyTtsEngine* pSpy = makeManager(); // proxy attaches in init()

    pMix->set(0.5);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Effect 1 mix", pSpy->lastText) << "name on touch";
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("a half", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, WhileMoving_SpeaksImmediately) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"),
                    QStringLiteral("AnnounceWhileMoving")),
            true);
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pVolume->set(0.75);
    QCoreApplication::processEvents();
    // No slotAnnouncePendingControl() call: while-moving mode speaks at once.
    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("[TestChannel1] volume three quarters", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, HeadSplitDecks_Announced) {
    auto pSplit = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[Master]"), QStringLiteral("headSplitDecks")));
    SpyTtsEngine* pSpy = makeManager(); // proxy attaches in init()

    pSplit->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Split cue on. Deck 1 left, deck 2 right", pSpy->lastText);

    pSplit->set(0.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Split cue off", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, Talkover_Announced) {
    auto pTalkover = std::make_unique<ControlObject>(ConfigKey(
            QStringLiteral("[Microphone]"), QStringLiteral("talkover")));
    SpyTtsEngine* pSpy = makeManager(); // proxy attaches in init()

    pTalkover->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Microphone on", pSpy->lastText);

    pTalkover->set(0.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Microphone off", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// Playlist and crate announcements (Tier 1)
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, Sidebar_PositionAnnounced) {
    SpyTtsEngine* pSpy = makeManager();

    m_pManager->slotSidebarItemActivated(QStringLiteral("House Bangers"), 2, 12, 0, false);

    EXPECT_QSTRING_EQ("House Bangers, 3 of 12", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, Sidebar_ContainerStateAnnounced) {
    SpyTtsEngine* pSpy = makeManager();

    m_pManager->slotSidebarItemActivated(QStringLiteral("Crates"), 3, 8, 5, false);
    EXPECT_QSTRING_EQ("Crates, 4 of 8, collapsed, 5 items", pSpy->lastText);

    // Expanding the same item re-announces with the new state (dedup is on
    // the full text, not the title).
    m_pManager->slotSidebarItemActivated(QStringLiteral("Crates"), 3, 8, 5, true);
    EXPECT_QSTRING_EQ("Crates, 4 of 8, expanded, 5 items", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, Sidebar_PlainTitleStillWorks) {
    SpyTtsEngine* pSpy = makeManager();

    // featureSelect path: no position info available.
    m_pManager->slotSidebarItemActivated(QStringLiteral("Tracks"));
    EXPECT_QSTRING_EQ("Tracks", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// Vinyl control (DVS) state announcements
// ---------------------------------------------------------------------------

class AnnouncementManagerVinylTest : public AnnouncementManagerTest {
  protected:
    static constexpr const char* kGroup = "[TestChannel1]";

    // Call after makeManager() to create the vinyl COs and wire up the
    // observers.
    void setupGroup() {
        m_pEnabled = std::make_unique<ControlObject>(ConfigKey(
                QLatin1String(kGroup), QStringLiteral("vinylcontrol_enabled")));
        m_pMode = std::make_unique<ControlObject>(ConfigKey(
                QLatin1String(kGroup), QStringLiteral("vinylcontrol_mode")));
        m_pCueing = std::make_unique<ControlObject>(ConfigKey(
                QLatin1String(kGroup), QStringLiteral("vinylcontrol_cueing")));
        m_pManager->connectGroupControls(QString::fromLatin1(kGroup));
    }

    void set(ControlObject* pControl, double v) {
        pControl->set(v);
        QCoreApplication::processEvents();
    }

    std::unique_ptr<ControlObject> m_pEnabled;
    std::unique_ptr<ControlObject> m_pMode;
    std::unique_ptr<ControlObject> m_pCueing;
};

TEST_F(AnnouncementManagerVinylTest, VinylEnabled_Announced) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    set(m_pEnabled.get(), 1.0);
    EXPECT_TRUE(pSpy->lastText.contains(QStringLiteral("vinyl control on")));

    set(m_pEnabled.get(), 0.0);
    EXPECT_TRUE(pSpy->lastText.contains(QStringLiteral("vinyl control off")));
}

TEST_F(AnnouncementManagerVinylTest, VinylMode_Announced) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    // Default is 0 (absolute); move away first so each set is a transition.
    set(m_pMode.get(), 1.0);
    EXPECT_TRUE(pSpy->lastText.contains(QStringLiteral("vinyl relative mode")));

    // Automatic flips (record end -> constant) speak through the same path.
    set(m_pMode.get(), 2.0);
    EXPECT_TRUE(pSpy->lastText.contains(QStringLiteral("vinyl constant mode")));

    set(m_pMode.get(), 0.0);
    EXPECT_TRUE(pSpy->lastText.contains(QStringLiteral("vinyl absolute mode")));
}

TEST_F(AnnouncementManagerVinylTest, VinylCueing_Announced) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    set(m_pCueing.get(), 1.0);
    EXPECT_TRUE(pSpy->lastText.contains(
            QStringLiteral("needle drop goes to cue point")));

    set(m_pCueing.get(), 2.0);
    EXPECT_TRUE(pSpy->lastText.contains(
            QStringLiteral("needle drop goes to nearest hotcue")));

    set(m_pCueing.get(), 0.0);
    EXPECT_TRUE(pSpy->lastText.contains(
            QStringLiteral("needle drop cueing off")));
}

TEST_F(AnnouncementManagerTest, PlaylistEdit_Announced) {
    SpyTtsEngine* pSpy = makeManager();

    m_pManager->slotPlaylistTracksEdited(QStringLiteral("Warmup"), 1, 0);
    EXPECT_QSTRING_EQ("Added to playlist Warmup", pSpy->lastText);

    m_pManager->slotPlaylistTracksEdited(QStringLiteral("Warmup"), 0, 1);
    EXPECT_QSTRING_EQ("Removed from playlist Warmup", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, CrateEdit_Announced) {
    SpyTtsEngine* pSpy = makeManager();

    m_pManager->slotCrateTracksEdited(QStringLiteral("House"), 1, 0);
    EXPECT_QSTRING_EQ("Added to crate House", pSpy->lastText);

    m_pManager->slotCrateTracksEdited(QStringLiteral("House"), 3, 0);
    EXPECT_QSTRING_EQ("Added 3 tracks to crate House", pSpy->lastText);

    m_pManager->slotCrateTracksEdited(QStringLiteral("House"), 0, 1);
    EXPECT_QSTRING_EQ("Removed from crate House", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, QuickPicker_ItemWithPositionAnnounced) {
    SpyTtsEngine* pSpy = makeManager();

    m_pManager->slotQuickPickerItemHighlighted(QStringLiteral("House"), 1, 3);

    EXPECT_QSTRING_EQ("House, 2 of 3", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, QuickPicker_PlainTextStillWorks) {
    SpyTtsEngine* pSpy = makeManager();

    // No position info: status messages like "No crates yet" have none.
    m_pManager->slotQuickPickerItemHighlighted(
            QStringLiteral("No crates yet. Press Control Shift N to create one."),
            -1,
            0);

    EXPECT_QSTRING_EQ("No crates yet. Press Control Shift N to create one.", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, QuickPicker_EmptyTextIsSilent) {
    SpyTtsEngine* pSpy = makeManager();

    m_pManager->slotQuickPickerItemHighlighted(QString(), -1, 0);

    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, QuickPicker_AnnouncedRegardlessOfPlaylistSetting) {
    // The picker is a deliberate, on-demand action, so it isn't gated by
    // AnnouncePlaylist the way ambient add/remove confirmations are.
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnouncePlaylist")),
            false);
    SpyTtsEngine* pSpy = makeManager();

    m_pManager->slotQuickPickerItemHighlighted(QStringLiteral("Warmup"), 0, 2);

    EXPECT_QSTRING_EQ("Warmup, 1 of 2", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, PlaylistEdit_SettingDisabled_Silent) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnouncePlaylist")),
            false);
    SpyTtsEngine* pSpy = makeManager();

    m_pManager->slotPlaylistTracksEdited(QStringLiteral("Warmup"), 1, 0);
    EXPECT_EQ(0, pSpy->callCount);
}

// ---------------------------------------------------------------------------
// Per-event feedback mode: each earcon-capable transport event honors its own
// FeedbackMode* setting. The earcon sink is null in tests, so we assert the
// speech side: mode 1 (sounds) suppresses speech, mode 0/2 keep it.
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerPerformanceTest, FeedbackSpeechMode_PlaySpeaks) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"),
                               QStringLiteral("FeedbackModePlay")),
            0); // speech
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPlay(1.0);
    EXPECT_QSTRING_EQ("Playing", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, FeedbackSoundsMode_PlaySilentSpeech) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"),
                               QStringLiteral("FeedbackModePlay")),
            1); // sounds only
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPlay(1.0);
    EXPECT_EQ(0, pSpy->callCount)
            << "sounds-only mode must not speak play; earcon carries it";
}

TEST_F(AnnouncementManagerPerformanceTest, FeedbackBothMode_PlaySpeaksToo) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"),
                               QStringLiteral("FeedbackModePlay")),
            2); // both
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPlay(1.0);
    EXPECT_QSTRING_EQ("Playing", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, FeedbackSoundsMode_CueSilentSpeech) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"),
                               QStringLiteral("FeedbackModeCue")),
            1);
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPfl(1.0);
    EXPECT_EQ(0, pSpy->callCount)
            << "sounds-only mode must not speak the headphone cue";
}

TEST_F(AnnouncementManagerPerformanceTest, FeedbackMode_IsPerEvent) {
    // Play set to sounds-only must not silence the headphone cue, which keeps
    // its own (default) mode.
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"),
                               QStringLiteral("FeedbackModePlay")),
            1);
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPfl(1.0);
    EXPECT_EQ(1, pSpy->callCount) << "cue feedback must be independent of play";
    EXPECT_TRUE(pSpy->lastText.contains(QStringLiteral("headphone cue")));
}

TEST_F(AnnouncementManagerPerformanceTest, FeedbackMode_DoesNotAffectNonEarconEvents) {
    // Loop announcements are not earcon-capable, so a sounds-only transport
    // mode must not silence them.
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"),
                               QStringLiteral("FeedbackModePlay")),
            1);
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pBeatloopSize->set(8.0);
    m_pLoopEnabled->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] loop 8 beats", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// MixerReadoutStyle preference: percentages instead of fractions.
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerPerformanceTest, MixerReadoutStyle_Percent_Volume) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("MixerReadoutStyle")),
            1); // percent
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pVolume->set(0.75);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("75 percent", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, MixerReadoutStyle_Percent_CenterSplit) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("MixerReadoutStyle")),
            1); // percent
    SpyTtsEngine* pSpy = makeManager();
    auto pPregain = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("pregain")));
    pPregain->set(0.5); // neutral parameter: unity gain, center
    setupGroup();

    pPregain->set(0.25); // half below center on the -1..1 split scale
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("minus 50 percent", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, MixerReadoutStyle_DefaultIsFractions) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pVolume->set(0.75);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("three quarters", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// Headphone mix (cue vs main) announcement.
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, HeadMix_LeaningCue_Announced) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    auto pHeadMix = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Master]"), QStringLiteral("headMix")));
    SpyTtsEngine* pSpy = makeManager(); // proxy attaches in init()

    pHeadMix->set(-0.75);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Headphone mix", pSpy->lastText) << "name on touch";
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("cue three quarters", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, HeadMix_LeaningMain_Announced) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    auto pHeadMix = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Master]"), QStringLiteral("headMix")));
    SpyTtsEngine* pSpy = makeManager();

    pHeadMix->set(0.5);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("main a half", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, HeadMix_Even_Announced) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    auto pHeadMix = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Master]"), QStringLiteral("headMix")));
    pHeadMix->set(-1.0);
    SpyTtsEngine* pSpy = makeManager();

    pHeadMix->set(0.0);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("even", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, HeadMix_MixerOnByDefault_Spoken) {
    auto pHeadMix = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Master]"), QStringLiteral("headMix")));
    SpyTtsEngine* pSpy = makeManager();

    pHeadMix->set(0.5);
    QCoreApplication::processEvents();
    // Name on touch: the knob names itself the moment it moves; the value
    // is debounced until it stops.
    EXPECT_QSTRING_EQ("Headphone mix", pSpy->lastText);
    m_pManager->slotAnnouncePendingControl();
    EXPECT_EQ(2, pSpy->callCount)
            << "mixer announcements are on by default (AnnounceMixer defaults on)";
    EXPECT_QSTRING_EQ("main a half", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, HeadMix_MixerDisabled_Silent) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            false);
    auto pHeadMix = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Master]"), QStringLiteral("headMix")));
    SpyTtsEngine* pSpy = makeManager();

    pHeadMix->set(0.5);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_EQ(0, pSpy->callCount)
            << "mixer announcements can be turned off (AnnounceMixer=false)";
}

// ---------------------------------------------------------------------------
// On-demand track re-announce (tts_track), for re-hearing a loaded track's
// name mid-set without waiting for the original load announcement.
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerStatusTest, FormatTrackName_NoTrackLoaded) {
    makeManager();
    EXPECT_QSTRING_EQ("Deck, Alpha. No track loaded.",
            m_pManager->formatTrackName(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, TrackButton_TriggersAnnouncement) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup(); // creates the tts_track trigger button for the group

    ControlProxy trackButton(QLatin1String(kGroup),
            QStringLiteral("tts_track"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
    trackButton.set(1.0);
    QCoreApplication::processEvents();

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_TRUE(pSpy->lastText.contains(QStringLiteral("No track loaded")))
            << pSpy->lastText.toStdString();
}

TEST_F(AnnouncementManagerStatusTest, FormatTrackName_Concise) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("ConciseAnnouncements")),
            true);
    makeManager();
    EXPECT_QSTRING_EQ("No track loaded.",
            m_pManager->formatTrackName(QString::fromLatin1(kGroup), 0));
}

// ---------------------------------------------------------------------------
// Restart and loop on/off also route through emitCue's per-event feedback
// mode, same as the original four transport events.
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerPerformanceTest, FeedbackSoundsMode_RestartSilentSpeech) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"),
                               QStringLiteral("FeedbackModeRestart")),
            1); // sounds only
    SpyTtsEngine* pSpy = makeManager();
    auto pStart = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("start")));
    setupGroup();

    pStart->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, pSpy->callCount)
            << "sounds-only mode must not speak back-to-start";
}

TEST_F(AnnouncementManagerPerformanceTest, FeedbackSoundsMode_LoopSilentSpeech) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"),
                               QStringLiteral("FeedbackModeLoop")),
            1); // sounds only
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pLoopEnabled->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, pSpy->callCount) << "sounds-only mode must not speak loop on";

    m_pLoopEnabled->set(0.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, pSpy->callCount) << "sounds-only mode must not speak loop off";
}

TEST_F(AnnouncementManagerPerformanceTest, FeedbackBothMode_LoopStillSpeaks) {
    // Default mode is "both", so loop announcements keep their existing text.
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pLoopEnabled->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] loop on", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// Master output clipping warning.
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, Clipping_Announced) {
    auto pPeak = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Main]"), QStringLiteral("peak_indicator")));
    SpyTtsEngine* pSpy = makeManager(); // proxy attaches in init()

    pPeak->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Clipping", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, Clipping_SettingDisabled_Silent) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceClipping")),
            false);
    auto pPeak = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Main]"), QStringLiteral("peak_indicator")));
    SpyTtsEngine* pSpy = makeManager();

    pPeak->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, Clipping_ThrottledOnRepeatedPeaks) {
    auto pPeak = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Main]"), QStringLiteral("peak_indicator")));
    SpyTtsEngine* pSpy = makeManager();

    pPeak->set(1.0);
    QCoreApplication::processEvents();
    ASSERT_EQ(1, pSpy->callCount);

    // A second peak shortly after must not re-announce (throttled).
    pPeak->set(0.0);
    pPeak->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(1, pSpy->callCount)
            << "clipping warning must be throttled during sustained clipping";
}

TEST_F(AnnouncementManagerPerformanceTest, FeedbackSoundsMode_ClippingSilentSpeech) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"),
                               QStringLiteral("FeedbackModeClipping")),
            1); // sounds only
    auto pPeak = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Main]"), QStringLiteral("peak_indicator")));
    SpyTtsEngine* pSpy = makeManager();

    pPeak->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, pSpy->callCount)
            << "sounds-only mode must not speak the clipping warning";
}

// ---------------------------------------------------------------------------
// Audio dropouts (xruns) — issue #66.
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, Xrun_Announced) {
    auto pXrun = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[App]"), QStringLiteral("audio_latency_overload")));
    SpyTtsEngine* pSpy = makeManager(); // proxy attaches in init()

    pXrun->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Audio dropout", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, Xrun_SettingDisabled_Silent) {
    // Xrun speech reuses the AnnounceClipping gate (both are audio-quality
    // safety signals, on by default).
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceClipping")),
            false);
    auto pXrun = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[App]"), QStringLiteral("audio_latency_overload")));
    SpyTtsEngine* pSpy = makeManager();

    pXrun->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, Xrun_ThrottledOnRepeatedOverloads) {
    auto pXrun = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[App]"), QStringLiteral("audio_latency_overload")));
    SpyTtsEngine* pSpy = makeManager();

    pXrun->set(1.0);
    QCoreApplication::processEvents();
    ASSERT_EQ(1, pSpy->callCount);

    pXrun->set(0.0);
    pXrun->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(1, pSpy->callCount)
            << "sustained xruns must be throttled, not repeated on every pulse";
}

TEST_F(AnnouncementManagerTest, Xrun_FeedbackSoundsMode_SilentSpeech) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"),
                               QStringLiteral("FeedbackModeClipping")),
            1); // sounds only
    auto pXrun = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[App]"), QStringLiteral("audio_latency_overload")));
    SpyTtsEngine* pSpy = makeManager();

    pXrun->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, pSpy->callCount)
            << "sounds-only mode must not speak the xrun warning";
}

// ---------------------------------------------------------------------------
// Knob/fader announcement context: name-once while the same control keeps
// moving, jitter suppression for a control resting on the same readout, and
// context reset when any other announcement interleaves.
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerPerformanceTest, ControlContext_JitterSuppressed) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pVolume->set(0.5);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] volume", pSpy->lastText) << "name on touch";
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("a half", pSpy->lastText);
    const int callsAfterFirst = pSpy->callCount;

    // A worn pot jitters by a hair: same spoken readout, so stay quiet —
    // neither the name nor the value — instead of chanting forever.
    m_pVolume->set(0.505);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_EQ(callsAfterFirst, pSpy->callCount)
            << "unchanged readout must be suppressed; spoke: "
            << pSpy->lastText.toStdString();

    // A real move still gets announced - value only, name already said.
    m_pVolume->set(0.75);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("three quarters", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, ControlContext_ResetByOtherAnnouncement) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    auto pKeylock = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("keylock")));
    setupGroup();

    m_pVolume->set(0.25);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("a quarter", pSpy->lastText);

    // An unrelated announcement (keylock) invalidates the context...
    pKeylock->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] key lock on", pSpy->lastText);

    // ...so the next volume move names the control again on touch.
    m_pVolume->set(0.75);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] volume", pSpy->lastText);
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("three quarters", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, NameOnTouch_SilentWhenReadoutUnchanged) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    auto pKeylock = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("keylock")));
    setupGroup();

    m_pVolume->set(0.5);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl(); // "…volume" + "a half" spoken

    // Another announcement expires the spoken context...
    pKeylock->set(1.0);
    QCoreApplication::processEvents();
    const int calls = pSpy->callCount;

    // ...but a touch that doesn't change the readout must still stay
    // completely silent — no name, no value. This is the worn-pot guard:
    // without it, a jittery pot would announce its name after every
    // unrelated announcement.
    m_pVolume->set(0.505);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_EQ(calls, pSpy->callCount)
            << "spoke: " << pSpy->lastText.toStdString();
}

TEST_F(AnnouncementManagerPerformanceTest, ControlContext_RepeatSpeaksFullText) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pVolume->set(0.25);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    m_pVolume->set(0.75);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("three quarters", pSpy->lastText);

    // The repeat key restores the full wording even though only the value
    // was spoken, so "what was that?" always has a complete answer.
    ControlProxy repeatButton(QStringLiteral("[Tts]"),
            QStringLiteral("repeat"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
    repeatButton.set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] volume three quarters", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// Cue preview earcon: the transport cue tap honors the Cue feedback mode
// instead of unconditionally speaking "Cue".
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerPerformanceTest, CuePreview_SoundsOnlyMode_NoSpeech) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"),
                               QStringLiteral("FeedbackModeCue")),
            1); // sounds only
    SpyTtsEngine* pSpy = makeManager();
    auto pCueDefault = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("cue_default")));
    setupGroup();

    pCueDefault->set(1.0);
    setPlay(1.0);
    EXPECT_EQ(0, pSpy->callCount)
            << "sounds-only Cue mode must not speak the cue preview; spoke: "
            << pSpy->lastText.toStdString();
}

// ---------------------------------------------------------------------------
// Controller feedback hooks: [Tts],shift and [Tts],pad_mode.
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, Shift_AnnouncedOnPressOnly) {
    SpyTtsEngine* pSpy = makeManager(); // creates the [Tts],shift control

    ControlProxy shift(QStringLiteral("[Tts]"),
            QStringLiteral("shift"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
    shift.set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Shift", pSpy->lastText);
    const int callsAfterPress = pSpy->callCount;

    shift.set(0.0); // release stays silent
    QCoreApplication::processEvents();
    EXPECT_EQ(callsAfterPress, pSpy->callCount);
}

TEST_F(AnnouncementManagerTest, PadMode_AnnouncedByVocabulary) {
    SpyTtsEngine* pSpy = makeManager(); // creates the [Tts],pad_mode control

    ControlProxy padMode(QStringLiteral("[Tts]"),
            QStringLiteral("pad_mode"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
    padMode.set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Pads, hot cues", pSpy->lastText);

    padMode.set(4.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Pads, sampler", pSpy->lastText);

    // Same mode again: no CO change, no announcement - this is what absorbs
    // the Numark Scratch firing one mode press for both decks.
    const int calls = pSpy->callCount;
    padMode.set(4.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(calls, pSpy->callCount);
}

// Issue #65: the DDJ-400's Keyboard, Pad FX1, Pad FX2, and Key Shift pad
// layers switch the hardware's MIDI notes but have no working pad behavior
// behind them (see the "Not implemented" note atop
// res/controllers/Pioneer-DDJ-400-script.js). Before this fix they were
// announced exactly like a working mode, so a blind DJ had no way to tell
// the pads underneath were dead until pressing one. The 4 unimplemented
// modes (5-8) must say so; the 5 working modes (1-4, 9) must not change.
TEST_F(AnnouncementManagerTest, PadMode_UnimplementedModesSayNotYetSupported) {
    SpyTtsEngine* pSpy = makeManager(); // creates the [Tts],pad_mode control

    ControlProxy padMode(QStringLiteral("[Tts]"),
            QStringLiteral("pad_mode"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);

    padMode.set(5.0); // keyboard
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Pads, keyboard (not yet supported)", pSpy->lastText);

    padMode.set(6.0); // pad effects 1
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Pads, pad effects 1 (not yet supported)", pSpy->lastText);

    padMode.set(7.0); // pad effects 2
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Pads, pad effects 2 (not yet supported)", pSpy->lastText);

    padMode.set(8.0); // key shift
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Pads, key shift (not yet supported)", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, PadMode_ImplementedModesUnaffectedByHonestyFix) {
    SpyTtsEngine* pSpy = makeManager(); // creates the [Tts],pad_mode control

    ControlProxy padMode(QStringLiteral("[Tts]"),
            QStringLiteral("pad_mode"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);

    padMode.set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Pads, hot cues", pSpy->lastText);

    padMode.set(2.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Pads, beat loop", pSpy->lastText);

    padMode.set(3.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Pads, beat jump", pSpy->lastText);

    padMode.set(4.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Pads, sampler", pSpy->lastText);

    padMode.set(9.0); // loop roll (Numark Scratch only, but still "working")
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Pads, loop roll", pSpy->lastText);
}

// Issue #65: [Tts],pad_mode deliberately keeps ignoring same-value writes
// (PadMode_AnnouncedByVocabulary above locks that in, since Numark Scratch's
// dedup of its mode button firing once per deck depends on it), so a
// controller mapping that wants a re-press of the same mode button to
// re-announce - letting a blind DJ query which of the eight layers they're
// currently on instead of cycling through all of them - has to force a
// change itself. The DDJ-400 mapping does this by bouncing the CO through 0
// (outside the spoken vocabulary) before re-setting the real value. This
// test exercises that exact primitive directly on the CO, without needing
// to run the DDJ-400 script.
TEST_F(AnnouncementManagerTest, PadMode_NeutralBounceForcesReannouncement) {
    SpyTtsEngine* pSpy = makeManager(); // creates the [Tts],pad_mode control

    ControlProxy padMode(QStringLiteral("[Tts]"),
            QStringLiteral("pad_mode"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);

    padMode.set(6.0); // pad effects 1
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Pads, pad effects 1 (not yet supported)", pSpy->lastText);
    const int callsAfterFirstPress = pSpy->callCount;

    // Bouncing through 0 must not itself speak: 0 isn't in the spoken
    // vocabulary, so the switch's default case returns silently.
    padMode.set(0.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(callsAfterFirstPress, pSpy->callCount)
            << "bouncing through the neutral value must stay silent";

    // Re-setting the same mode after the bounce must re-announce it, unlike
    // a bare same-value write (see PadMode_AnnouncedByVocabulary).
    padMode.set(6.0);
    QCoreApplication::processEvents();
    EXPECT_EQ(callsAfterFirstPress + 1, pSpy->callCount)
            << "re-press via the neutral-value bounce must re-announce the current mode";
    EXPECT_QSTRING_EQ("Pads, pad effects 1 (not yet supported)", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// Search result count folded into the spoken search announcement.
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, Search_IncludesResultCount) {
    SpyTtsEngine* pSpy = makeManager();

    m_pManager->slotSearchTextChanged(QStringLiteral("techno"));
    m_pManager->slotSearchResultCount(42);
    m_pManager->slotAnnounceSearch();
    EXPECT_QSTRING_EQ("Searching: techno. 42 tracks", pSpy->lastText);

    m_pManager->slotSearchTextChanged(QStringLiteral("technoz"));
    m_pManager->slotSearchResultCount(0);
    m_pManager->slotAnnounceSearch();
    EXPECT_QSTRING_EQ("Searching: technoz. No tracks", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, Search_NoCountReported_PlainAnnouncement) {
    SpyTtsEngine* pSpy = makeManager();

    // A view that never reports a count (not a track table) must not
    // inherit a stale count from an earlier query.
    m_pManager->slotSearchTextChanged(QStringLiteral("house"));
    m_pManager->slotSearchResultCount(7);
    m_pManager->slotAnnounceSearch();
    m_pManager->slotSearchTextChanged(QStringLiteral("garage"));
    m_pManager->slotAnnounceSearch();
    EXPECT_QSTRING_EQ("Searching: garage", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// BPM halve/double confirmation (fixing a half-tempo misanalysis by ear).
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerPerformanceTest, BeatsHalveDouble_Announced) {
    auto pHalve = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("beats_set_halve")));
    auto pDouble = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("beats_set_double")));
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    pHalve->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] B P M halved", pSpy->lastText);

    pDouble->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] B P M doubled", pSpy->lastText);
}

// ---------------------------------------------------------------------------
// Tempo range cycling (the DDJ-400's Shift+SYNC remaps to rateRange). The
// pitch fader keeps the same physical position across a range change, so the
// new range is spoken immediately: otherwise the next spoken pitch
// percentage is ambiguous about which BPM delta it actually means.
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerPerformanceTest, RateRangeChange_Announced) {
    auto pRateRange = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("rateRange")));
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    pRateRange->set(0.08);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] tempo range plus or minus 8 percent", pSpy->lastText);

    pRateRange->set(0.16);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] tempo range plus or minus 16 percent", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, RateRangeChange_TempoSettingDisabled_Silent) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceTempo")),
            false);
    auto pRateRange = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("rateRange")));
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    pRateRange->set(0.08);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, pSpy->callCount);
}

// ---------------------------------------------------------------------------
// Smart cue: loading into a stopped deck moves the headphone cue there.
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, SmartCue_MovesPflToLoadedIdleDeck) {
    auto pPlay1 = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("play")));
    auto pPlay2 = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Channel2]"), QStringLiteral("play")));
    auto pPfl1 = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("pfl")));
    auto pPfl2 = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Channel2]"), QStringLiteral("pfl")));
    m_pPlayerManager->m_deckCount = 2;
    makeManager();

    // Deck 1 is being previewed; a track lands in stopped deck 2.
    pPfl1->set(1.0);
    m_pManager->slotNewTrackLoaded(
            makeTrack(QStringLiteral("Artist"), QStringLiteral("Title")), 1);

    EXPECT_EQ(0.0, pPfl1->get()) << "cue must leave the old deck";
    EXPECT_EQ(1.0, pPfl2->get()) << "cue must follow the loaded track";
}

TEST_F(AnnouncementManagerTest, SmartCue_PlayingDeckKeepsItsCue) {
    auto pPlay2 = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Channel2]"), QStringLiteral("play")));
    auto pPfl1 = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("pfl")));
    auto pPfl2 = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Channel2]"), QStringLiteral("pfl")));
    m_pPlayerManager->m_deckCount = 2;
    makeManager();

    // Deck 2 is live (playing); loading into it must not touch any cue.
    pPlay2->set(1.0);
    pPfl1->set(1.0);
    m_pManager->slotNewTrackLoaded(
            makeTrack(QStringLiteral("Artist"), QStringLiteral("Title")), 1);

    EXPECT_EQ(1.0, pPfl1->get());
    EXPECT_EQ(0.0, pPfl2->get());
}

TEST_F(AnnouncementManagerTest, SmartCue_DisabledPref_NoChange) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Controls]"), QStringLiteral("SmartCue")),
            false);
    auto pPfl1 = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("pfl")));
    auto pPfl2 = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Channel2]"), QStringLiteral("pfl")));
    m_pPlayerManager->m_deckCount = 2;
    makeManager();

    pPfl1->set(1.0);
    m_pManager->slotNewTrackLoaded(
            makeTrack(QStringLiteral("Artist"), QStringLiteral("Title")), 1);

    EXPECT_EQ(1.0, pPfl1->get());
    EXPECT_EQ(0.0, pPfl2->get());
}

// ---------------------------------------------------------------------------
// Auto DJ (issue #61)
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, AutoDJEnabled_Announced) {
    auto pEnabled = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[AutoDJ]"), QStringLiteral("enabled")));
    SpyTtsEngine* pSpy = makeManager(); // proxy attaches in init()

    pEnabled->set(1.0);
    QCoreApplication::processEvents();
    // No Library in tests, so there is no queued track to fold in.
    EXPECT_QSTRING_EQ("Auto DJ on", pSpy->lastText);

    pEnabled->set(0.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Auto DJ off", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, AutoDJFadeNow_Announced) {
    auto pFadeNow = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[AutoDJ]"), QStringLiteral("fade_now")));
    SpyTtsEngine* pSpy = makeManager();

    pFadeNow->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Fading now", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, AutoDJSkipNext_Announced) {
    auto pSkipNext = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[AutoDJ]"), QStringLiteral("skip_next")));
    SpyTtsEngine* pSpy = makeManager();

    pSkipNext->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Skipped", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, FormatAutoDJNext_DisabledEmptyQueue_NoLibrary) {
    // Tests never wire up a real Library, so m_pAutoDJProcessor is null and
    // the queue is unreachable; the readout must still degrade gracefully
    // rather than crash or omit the on/off state.
    makeManager();
    EXPECT_QSTRING_EQ("Auto DJ is off. Queue is empty.", m_pManager->formatAutoDJNext());
}

TEST_F(AnnouncementManagerTest, FormatAutoDJNext_ReflectsEnabledState) {
    auto pEnabled = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[AutoDJ]"), QStringLiteral("enabled")));
    makeManager();

    pEnabled->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("Auto DJ is on. Queue is empty.", m_pManager->formatAutoDJNext());
}

TEST_F(AnnouncementManagerTest, AutoDJNextHotkey_TriggersReadout) {
    SpyTtsEngine* pSpy = makeManager();
    ControlProxy nextButton(QStringLiteral("[AutoDJ]"),
            QStringLiteral("tts_next"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid);
    nextButton.set(1.0);
    QCoreApplication::processEvents();

    EXPECT_QSTRING_EQ("Auto DJ is off. Queue is empty.", pSpy->lastText);
}

// Reproduces issue #48 (case 1): with Smart Cue, AnnounceTrackLoad, and
// AnnounceCue all on by default (as they are), loading a track into a
// stopped deck used to speak the load announcement and then have the Smart
// Cue pfl set -- fired synchronously within the same slotNewTrackLoaded call
// -- immediately clobber it with "headphone cue on" before the load
// announcement had any chance to render. TtsEngine's barge-in generation
// counter meant only the second utterance was ever actually audible.
TEST_F(AnnouncementManagerTest, SmartCue_LoadAnnouncementNotLostToCueBargeIn) {
    auto pPfl1 = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("pfl")));
    auto pPfl2 = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Channel2]"), QStringLiteral("pfl")));
    m_pPlayerManager->m_deckCount = 2;
    SpyTtsEngine* pSpy = makeManager();
    // Wire up the pfl -> "headphone cue on/off" observer the way a real
    // connectDeck() would, so the Smart Cue pfl set below actually speaks
    // through the normal AnnounceCue path instead of just moving a bare CO.
    m_pManager->connectGroupControls(QStringLiteral("[Channel1]"), 0);
    m_pManager->connectGroupControls(QStringLiteral("[Channel2]"), 1);

    // Deck B (index 1) is stopped; loading into it triggers Smart Cue, which
    // moves pfl from deck A to deck B and (with AnnounceCue on) speaks
    // "headphone cue on" for deck B.
    m_pManager->slotNewTrackLoaded(
            makeTrack(QStringLiteral("Artist"), QStringLiteral("Title")), 1);

    // Both pieces of information must reach the engine -- concatenated into
    // a single utterance -- instead of the cue announcement silently
    // overwriting the load announcement.
    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_TRUE(pSpy->lastText.contains(QStringLiteral("Loaded deck, Bravo")))
            << pSpy->lastText.toStdString();
    EXPECT_TRUE(pSpy->lastText.contains(QStringLiteral("headphone cue on")))
            << pSpy->lastText.toStdString();

    // The underlying cue behavior is unaffected by batching the speech.
    EXPECT_EQ(0.0, pPfl1->get());
    EXPECT_EQ(1.0, pPfl2->get());
}

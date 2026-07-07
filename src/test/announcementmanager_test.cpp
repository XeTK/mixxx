#include "util/announcementmanager.h"

#include <gtest/gtest.h>

#include <QCoreApplication>

#include "audio/types.h"
#include "control/controlaudiotaperpot.h"
#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "engine/enginetts.h"
#include "library/library_decl.h"
#include "mixer/playermanager.h"
#include "preferences/usersettings.h"
#include "test/mixxxtest.h"
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
            "Loaded deck, Ay. Aphex Twin. Windowlicker. 128 B P M. Key: A Minor.",
            AnnouncementManager::formatForLoad(pTrack, 0));
}

TEST_F(AnnouncementManagerTest, FormatForLoad_DeckLetter) {
    auto pTrack = makeTrack(QStringLiteral(""), QStringLiteral(""));
    EXPECT_TRUE(AnnouncementManager::formatForLoad(pTrack, 0).startsWith(
            QStringLiteral("Loaded deck, Ay")));
    EXPECT_TRUE(AnnouncementManager::formatForLoad(pTrack, 1).startsWith(
            QStringLiteral("Loaded deck, Bee")));
    EXPECT_TRUE(AnnouncementManager::formatForLoad(pTrack, 2).startsWith(
            QStringLiteral("Loaded deck, See")));
}

TEST_F(AnnouncementManagerTest, FormatForLoad_NoBpm) {
    auto pTrack = makeTrack(
            QStringLiteral("Aphex Twin"), QStringLiteral("Windowlicker"), 0.0, QStringLiteral("A minor"));
    const QString result = AnnouncementManager::formatForLoad(pTrack, 0);
    EXPECT_TRUE(result.contains(QStringLiteral("Loaded deck, Ay")));
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
    EXPECT_TRUE(result.startsWith(QStringLiteral("Loaded deck, Ay. Windowlicker.")));
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
    EXPECT_TRUE(pSpy->lastText.startsWith(QStringLiteral("Loaded deck, Ay.")));
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

TEST_F(AnnouncementManagerTest, AnnounceSelection_NullTrackIgnored) {
    SpyTtsEngine* pSpy = makeManager();
    focusTrackList(pSpy);
    m_pManager->slotTrackSelected(TrackPointer());
    m_pManager->slotAnnounceSelectedTrack();
    EXPECT_EQ(0, pSpy->callCount);
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

    std::unique_ptr<ControlObject> m_pPlay;
    std::unique_ptr<ControlObject> m_pEndOfTrack;
    std::unique_ptr<ControlObject> m_pPfl;
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
// Startup announcement (slotSkinLoaded)
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerTest, SkinLoaded_AnnouncesReady) {
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotSkinLoaded();
    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Mixxx ready", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, SkinLoaded_SettingDisabled_Silent) {
    config()->setValue(ConfigKey(QStringLiteral("[Accessibility]"),
                               QStringLiteral("AnnounceStartup")),
            false);
    SpyTtsEngine* pSpy = makeManager();
    m_pManager->slotSkinLoaded();
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

    m_pManager->slotSkinLoaded(); // triggers speak()

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_EQ(1.0, readRouteControl());
}

TEST_F(AnnouncementManagerRouteSyncTest, Speak_SyncsRouteToHeadphones) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("TtsRoute")), 0);
    SpyTtsEngine* pSpy = makeManagerWithSink();

    m_pManager->slotSkinLoaded();

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_EQ(0.0, readRouteControl());
}

TEST_F(AnnouncementManagerRouteSyncTest, Speak_SkipsRouteSyncWhenUnchanged) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("TtsRoute")), 1);
    SpyTtsEngine* pSpy = makeManagerWithSink();

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

    EXPECT_QSTRING_EQ("Deck, Ay. No track loaded.",
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
            "Deck, Ay. Playing. 1 minute 30 seconds remaining. 128 B P M. "
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
            "Deck, Bee. Stopped. 45 seconds remaining. Pitch down 5 percent.",
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

TEST_F(AnnouncementManagerPerformanceTest, SyncToggle_Announced) {
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pSync->set(1.0);
    QCoreApplication::processEvents();
    EXPECT_QSTRING_EQ("[TestChannel1] sync on", pSpy->lastText);

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
    EXPECT_EQ(0, pSpy->callCount) << "tempo announcement must be debounced";

    m_pManager->slotAnnouncePendingControl();
    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("[TestChannel1] Pitch up 5 percent", pSpy->lastText);
}

TEST_F(AnnouncementManagerPerformanceTest, VolumeChange_MixerOffByDefault_Silent) {
    SpyTtsEngine* pSpy = makeManager();
    createPerformanceControls();
    setupGroup();

    m_pVolume->set(0.5);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_EQ(0, pSpy->callCount)
            << "mixer announcements must be opt-in (AnnounceMixer defaults off)";
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
    m_pManager->slotAnnouncePendingControl();
    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("[TestChannel1] volume a half", pSpy->lastText);
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
    EXPECT_QSTRING_EQ("[TestChannel1] volume three quarters", pSpy->lastText);

    m_pVolume->set(0.3125); // 5/16
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("[TestChannel1] volume 5 sixteenths", pSpy->lastText);
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
    EXPECT_QSTRING_EQ("[TestChannel1] filter minus a half", pSpy->lastText);

    pSuper->set(0.5);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("[TestChannel1] filter center", pSpy->lastText);
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
    // and the raw value IS the fader parameter here. 0.25 is a quarter of the
    // way from minimum to center, i.e. half of the cut range below center.
    pMainGain->set(0.25); // half of the cut range = minus a half
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("Main volume minus a half", pSpy->lastText);
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
    EXPECT_QSTRING_EQ("Deck, Ay. No track loaded.",
            m_pManager->formatTimeRemaining(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, FormatTimeRemaining_WithTrack) {
    makeManager();
    setupGroup();
    createStatusControls();
    m_pDuration->set(200.0);
    m_pPlayPos->set(0.35); // 130 seconds remaining

    EXPECT_QSTRING_EQ("Deck, Ay. 2 minutes 10 seconds remaining.",
            m_pManager->formatTimeRemaining(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, FormatBpm_RoundsAndSpells) {
    makeManager();
    createStatusControls();
    m_pBpm->set(174.4);

    EXPECT_QSTRING_EQ("Deck, Ay. 174 B P M.",
            m_pManager->formatBpm(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, FormatBpm_NoneAvailable) {
    makeManager();
    EXPECT_QSTRING_EQ("Deck, Ay. No B P M.",
            m_pManager->formatBpm(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, FormatKey_SpokenName) {
    makeManager();
    auto pKey = std::make_unique<ControlObject>(
            ConfigKey(QLatin1String(kGroup), QStringLiteral("key")));
    pKey->set(22.0); // ChromaticKey A_MINOR

    EXPECT_QSTRING_EQ("Deck, Ay. Key: A Minor.",
            m_pManager->formatKey(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, FormatKey_Unknown) {
    makeManager();
    EXPECT_QSTRING_EQ("Deck, Ay. Key unknown.",
            m_pManager->formatKey(QString::fromLatin1(kGroup), 0));
}

TEST_F(AnnouncementManagerStatusTest, FormatBarPosition_NoDeck) {
    // The stub PlayerManager has no decks, so no track can be resolved.
    makeManager();
    EXPECT_QSTRING_EQ("Deck, Ay. No track loaded.",
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

    EXPECT_QSTRING_EQ("[TestChannel1] Pitch up 5 percent. 131 B P M", pSpy->lastText);
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
    EXPECT_QSTRING_EQ("[TestChannel1] trim minus a quarter", pSpy->lastText);
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
    EXPECT_QSTRING_EQ("[TestChannel1] volume a half", pSpy->lastText)
            << "announcement must read the fader position, not the tapered "
               "gain value: "
            << pSpy->lastText.toStdString();
}

TEST_F(AnnouncementManagerPerformanceTest, MainVolume_TaperedControl_CenterIsCenter) {
    config()->setValue(
            ConfigKey(QStringLiteral("[Accessibility]"), QStringLiteral("AnnounceMixer")),
            true);
    // Mirrors EngineMixer's real [Master],gain control.
    auto pMainGain = std::make_unique<ControlAudioTaperPot>(
            ConfigKey(QStringLiteral("[Master]"), QStringLiteral("gain")), -14, 14, 0.5);
    SpyTtsEngine* pSpy = makeManager();

    // The constructor already sets parameter to neutralParameter (0.5, i.e.
    // this same "center" value), so setParameter(0.5) alone would be a no-op
    // that never fires valueChanged. Move away first to force a real
    // transition back to center.
    pMainGain->setParameter(0.0);
    pMainGain->setParameter(0.5); // knob centered = unity gain = 0 dB
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("Main volume center", pSpy->lastText);
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
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("Effect 1 mix a half", pSpy->lastText);
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
    EXPECT_QSTRING_EQ("[TestChannel1] volume 75 percent", pSpy->lastText);
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
    EXPECT_QSTRING_EQ("[TestChannel1] trim minus 50 percent", pSpy->lastText);
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
    EXPECT_QSTRING_EQ("[TestChannel1] volume three quarters", pSpy->lastText);
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
    m_pManager->slotAnnouncePendingControl();
    EXPECT_QSTRING_EQ("Headphone mix cue three quarters", pSpy->lastText);
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
    EXPECT_QSTRING_EQ("Headphone mix main a half", pSpy->lastText);
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
    EXPECT_QSTRING_EQ("Headphone mix even", pSpy->lastText);
}

TEST_F(AnnouncementManagerTest, HeadMix_MixerOffByDefault_Silent) {
    auto pHeadMix = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Master]"), QStringLiteral("headMix")));
    SpyTtsEngine* pSpy = makeManager();

    pHeadMix->set(0.5);
    QCoreApplication::processEvents();
    m_pManager->slotAnnouncePendingControl();
    EXPECT_EQ(0, pSpy->callCount);
}

// ---------------------------------------------------------------------------
// On-demand track re-announce (tts_track), for re-hearing a loaded track's
// name mid-set without waiting for the original load announcement.
// ---------------------------------------------------------------------------

TEST_F(AnnouncementManagerStatusTest, FormatTrackName_NoTrackLoaded) {
    makeManager();
    EXPECT_QSTRING_EQ("Deck, Ay. No track loaded.",
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

#include "util/announcementmanager.h"

#include <gtest/gtest.h>

#include <QCoreApplication>

#include "audio/types.h"
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
                nullptr); // no EngineTts sink needed for logic tests
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
            "Loaded A. Aphex Twin. Windowlicker. 128 B P M. Key: A Minor.",
            AnnouncementManager::formatForLoad(pTrack, 0));
}

TEST_F(AnnouncementManagerTest, FormatForLoad_DeckLetter) {
    auto pTrack = makeTrack(QStringLiteral(""), QStringLiteral(""));
    EXPECT_TRUE(AnnouncementManager::formatForLoad(pTrack, 0).startsWith(
            QStringLiteral("Loaded A")));
    EXPECT_TRUE(AnnouncementManager::formatForLoad(pTrack, 1).startsWith(
            QStringLiteral("Loaded B")));
    EXPECT_TRUE(AnnouncementManager::formatForLoad(pTrack, 2).startsWith(
            QStringLiteral("Loaded C")));
}

TEST_F(AnnouncementManagerTest, FormatForLoad_NoBpm) {
    auto pTrack = makeTrack(
            QStringLiteral("Aphex Twin"), QStringLiteral("Windowlicker"), 0.0, QStringLiteral("A minor"));
    const QString result = AnnouncementManager::formatForLoad(pTrack, 0);
    EXPECT_TRUE(result.contains(QStringLiteral("Loaded A")));
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
    EXPECT_TRUE(result.startsWith(QStringLiteral("Loaded A. Windowlicker.")));
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
    EXPECT_TRUE(pSpy->lastText.startsWith(QStringLiteral("Loaded A.")));
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
    EXPECT_QSTRING_EQ("Headphone cue on", pSpy->lastText);
}

TEST_F(AnnouncementManagerPlaystateTest, PflOff_AnnouncesCueOff) {
    SpyTtsEngine* pSpy = makeManager();
    setupGroup();

    setPfl(1.0);
    pSpy->callCount = 0;
    pSpy->lastText.clear();
    setPfl(0.0);

    EXPECT_EQ(1, pSpy->callCount);
    EXPECT_QSTRING_EQ("Headphone cue off", pSpy->lastText);
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
    EXPECT_QSTRING_EQ("Headphone cue on", pSpy->lastText);
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
                m_pEngineTts.get());
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

    EXPECT_QSTRING_EQ("Deck A. No track loaded.",
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
            "Deck A. Playing. 1 minute 30 seconds remaining. 128 B P M. "
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
            "Deck B. Stopped. 45 seconds remaining. Pitch down 5 percent.",
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
    EXPECT_QSTRING_EQ("[TestChannel1] volume 50 percent", pSpy->lastText);
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

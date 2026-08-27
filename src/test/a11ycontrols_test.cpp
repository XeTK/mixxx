// Existence guard for every ControlObject the accessibility fork depends on.
//
// WHY THIS TEST EXISTS
// --------------------
// The fork's accessibility layer (AnnouncementManager, AccessMenuController,
// EngineBeatClick, EngineEarcon, LibraryControl's quick-add controls) works by
// *observing* around ninety distinct control names, most of them per-deck or
// per-effect-unit, which expand to 263 concrete (group, item) pairs once the
// decks and units this fixture builds are taken into account. Almost every one
// of those observers is created like this:
//
//     ControlProxy(group, item, this, ControlFlag::AllowMissingOrInvalid)
//
// Look at what that flag actually does (src/control/controlproxy.cpp):
//
//     m_pControl = ControlDoublePrivate::getControl(key, flags);
//     if (!m_pControl) {
//         DEBUG_ASSERT(flags & ControlFlag::AllowMissingOrInvalid);
//         m_pControl = ControlDoublePrivate::getDefaultControl();
//     }
//
// If the control does not exist, the proxy silently binds to a shared dummy
// whose value never changes. connectValueChanged() succeeds. Nothing asserts,
// nothing warns, nothing crashes. The observer simply never fires again.
//
// For a sighted user a missing announcement is invisible. For the blind user
// this fork exists for, it is the whole product: the deck stops saying "deck 1
// playing", the metronome stops following the beat, the accessibility menu
// reads back a value that is permanently zero. The app "works" and is unusable.
//
// AccessMenuController is worse still - it uses ControlFlag::NoWarnIfMissing,
// which additionally suppresses the console warning, so a broken binding leaves
// no trace anywhere at all.
//
// This is exactly the failure mode a large rebase onto a newer upstream
// produces: upstream renames a control, the fork still compiles, every existing
// test still passes, and the app goes quiet. src/test/announcementmanager_test.cpp
// cannot catch it because it fabricates its own ControlObjects under an invented
// "[TestChannel1]" group - it proves the formatting logic works, not that the
// controls it formats still exist.
//
// HOW IT WORKS
// ------------
// The fixture derives from BaseSignalPathTest, which builds a real EngineMixer
// with real Decks, real EngineBuffers and real CueControls. That matters: every
// ControlObject checked below is created by PRODUCTION code, never by this test.
// A test that creates the controls it then asserts on would prove nothing.
//
// HOW TO READ A FAILURE
// ---------------------
// Each failure names the group, the item, and what the user loses:
//
//     Missing ControlObject [Channel1],rate_ratio
//       observed by: AnnouncementManager (pitch/tempo announcements)
//       -> the accessibility layer binds to a dummy control and goes silent.
//
// A failure means upstream renamed, moved or deleted that control. Find the new
// name (`git log -p --all -S'"old_name"' -- src/engine src/mixer`) and update
// BOTH the production site that observes it AND the table in this file. Do not
// simply delete the entry - the entry is the point.

#include <gtest/gtest.h>

#include <QString>
#include <memory>
#include <vector>

#include "control/controlobject.h"
#include "effects/effectsmanager.h"
#include "engine/channels/enginedeck.h"
#include "library/library_prefs.h"
#include "library/librarycontrol.h"
#include "mixer/playermanager.h"
#include "recording/recordingmanager.h"
#include "test/signalpathtest.h"
#include "util/accessmenucontroller.h"
#include "util/announcementmanager.h"
#include "util/ttsengine.h"

namespace {

/// One control the accessibility layer depends on, plus the human consequence
/// of it disappearing. `why` is printed on failure, so keep it specific.
struct A11yKey {
    QString group;
    QString item;
    const char* why;
};

/// Assert that every key in `keys` exists, one clear failure line per key.
///
/// Deliberately a loop rather than TEST_P/INSTANTIATE_TEST_SUITE_P: gtest
/// reconstructs the fixture for every parameter, and this fixture builds an
/// entire EngineMixer (three decks, three EngineBuffers, effect racks). Over
/// ~200 keys that turns a millisecond hash-lookup sweep into ~20 seconds of
/// engine construction. A failing EXPECT_TRUE inside the loop still produces
/// one distinctly labelled failure per missing control, which is the property
/// that actually matters when diagnosing a rebase.
void expectAllExist(const std::vector<A11yKey>& keys) {
    for (const A11yKey& key : keys) {
        EXPECT_TRUE(ControlObject::exists(ConfigKey(key.group, key.item)))
                << "Missing ControlObject " << key.group.toStdString() << ","
                << key.item.toStdString() << "\n  observed by: " << key.why
                << "\n  -> the accessibility layer binds to a dummy control "
                   "and goes silent.";
    }
}

/// Minimal PlayerManagerInterface that hands AnnouncementManager the real Decks
/// the fixture built.
///
/// getDeckBase() must return a real player: AnnouncementManager::connectDeck()
/// returns early when it is null, and connectGroupControls() - which creates
/// every tts_* readout button - is the last thing it calls. A stub that returns
/// nullptr would make this test vacuously pass.
class StubPlayerManager : public PlayerManagerInterface {
  public:
    explicit StubPlayerManager(std::vector<BaseTrackPlayer*> decks)
            : m_decks(std::move(decks)) {
    }

    BaseTrackPlayer* getPlayer(const QString&) const override {
        return nullptr;
    }
    BaseTrackPlayer* getPlayer(const ChannelHandle&) const override {
        return nullptr;
    }
    BaseTrackPlayer* getDeckBase(int deckIndex) const override {
        if (deckIndex < 0 || deckIndex >= static_cast<int>(m_decks.size())) {
            return nullptr;
        }
        return m_decks[deckIndex];
    }
    PreviewDeck* getPreviewDeck(int) const override {
        return nullptr;
    }
    Sampler* getSampler(int) const override {
        return nullptr;
    }
    int numberOfDecks() const override {
        return static_cast<int>(m_decks.size());
    }
    int numberOfSamplers() const override {
        return 0;
    }
    int numberOfPreviewDecks() const override {
        return 0;
    }

  private:
    const std::vector<BaseTrackPlayer*> m_decks;
};

/// TtsEngine that does nothing. AnnouncementManager takes ownership.
class NullTtsEngine : public TtsEngine {
  public:
    void say(const QString&) override {
    }
    void setVoice(const QString&) override {
    }
    void setRate(int) override {
    }
};

} // namespace

/// Builds the real engine, then completes the wiring PlayerManager normally
/// does (standard effect chains plus the per-deck EQ/QuickEffect racks) so the
/// effect controls the fork announces actually exist.
class A11yControlExistenceTest : public BaseSignalPathTest {
  protected:
    void SetUp() override {
        BaseSignalPathTest::SetUp();
        // PlayerManager::addDeckInner() does this for every deck it creates;
        // BaseSignalPathTest constructs Decks directly and skips it.
        m_pEffectsManager->setup();
        for (const QString& group : deckGroups()) {
            m_pEffectsManager->addDeck(
                    m_pEngineMixer->registerChannelGroup(group));
        }
    }

    /// The decks BaseSignalPathTest actually creates. Per-deck expectations are
    /// parameterised over this rather than hard-coding a deck count.
    static const std::vector<QString>& deckGroups() {
        static const std::vector<QString> kGroups{
                m_sGroup1, m_sGroup2, m_sGroup3};
        return kGroups;
    }
};

// ---------------------------------------------------------------------------
// The main guard: controls owned by upstream, observed by the fork.
//
// These are the ones a rebase can silently break. Everything here is created by
// EngineMixer / Deck / EngineBuffer / CueControl / EffectsManager - i.e. by
// production code this test merely instantiates.
// ---------------------------------------------------------------------------

TEST_F(A11yControlExistenceTest, MixerControlsObservedByAccessibilityLayerExist) {
    const QString kMain = m_sMainGroup; // "[Master]"

    std::vector<A11yKey> keys{
            // AnnouncementManager + EngineTts/EngineBeatClick/EngineEarcon all
            // read the sample rate to schedule speech and clicks.
            {QStringLiteral("[App]"),
                    QStringLiteral("samplerate"),
                    "AnnouncementManager, EngineTts, EngineBeatClick, "
                    "EngineEarcon (speech and click scheduling)"},

            // Main output clipping alarm. Note the group: the fork observes
            // [Main],peak_indicator, which EngineVuMeter creates under [Main]
            // with a [Master] alias. If upstream ever drops the [Main] name
            // this binding dies silently.
            {QStringLiteral("[Main]"),
                    QStringLiteral("peak_indicator"),
                    "AnnouncementManager (clipping alarm - the DJ cannot see "
                    "the red light)"},

            {kMain,
                    QStringLiteral("crossfader"),
                    "AnnouncementManager (crossfader position readout)"},
            {kMain,
                    QStringLiteral("crossfader_lock"),
                    "AnnouncementManager (suppresses crossfader chatter while "
                    "locked)"},
            {kMain,
                    QStringLiteral("headMix"),
                    "AnnouncementManager (headphone mix readout)"},
            {kMain,
                    QStringLiteral("gain"),
                    "AnnouncementManager (main volume readout)"},
            {kMain,
                    QStringLiteral("headGain"),
                    "AnnouncementManager (headphone volume readout)"},
            {kMain,
                    QStringLiteral("headSplitDecks"),
                    "AnnouncementManager (split cue confirmation, Alt+H)"},
            {kMain,
                    QStringLiteral("disable_touch_scratch"),
                    "AnnouncementManager (touch-scratch toggle confirmation)"},

            // Fork-owned engine controls, created by EngineMixer's constructor
            // (EngineTts / EngineBeatClick / EngineEarcon). Included because a
            // rebase that drops those constructor lines from enginemixer.cpp
            // would otherwise go unnoticed - the fork's own code compiles fine
            // without them, it just never makes a sound.
            {QStringLiteral("[Tts]"),
                    QStringLiteral("enabled"),
                    "EngineTts; toggled by AccessMenuController and announced "
                    "by AnnouncementManager"},
            {QStringLiteral("[Tts]"),
                    QStringLiteral("speaking"),
                    "EngineTts (speech-in-progress flag used for ducking)"},
            {QStringLiteral("[Tts]"),
                    QStringLiteral("route_to_main"),
                    "EngineTts; read by EngineBeatClick and EngineEarcon to "
                    "follow the speech route"},
            {QStringLiteral("[Tts]"),
                    QStringLiteral("duckStrength"),
                    "EngineTts; editable from the accessibility menu"},
            {QStringLiteral("[BeatClick]"),
                    QStringLiteral("enabled"),
                    "EngineBeatClick; toggle announced by AnnouncementManager "
                    "(Alt+B)"},
            {QStringLiteral("[BeatClick]"),
                    QStringLiteral("volume"),
                    "EngineBeatClick; editable from the accessibility menu"},
            {QStringLiteral("[Earcon]"),
                    QStringLiteral("volume"),
                    "EngineEarcon (non-speech cue volume)"},
    };

    // Effect unit knobs and per-slot state, announced under AnnounceEffects.
    for (int unit = 1; unit <= 4; ++unit) {
        const QString unitGroup =
                QStringLiteral("[EffectRack1_EffectUnit%1]").arg(unit);
        keys.push_back({unitGroup,
                QStringLiteral("mix"),
                "AnnouncementManager (effect unit dry/wet readout)"});
        keys.push_back({unitGroup,
                QStringLiteral("super1"),
                "AnnouncementManager (effect unit super knob readout)"});
        for (int slot = 1; slot <= 4; ++slot) {
            const QString slotGroup =
                    QStringLiteral("[EffectRack1_EffectUnit%1_Effect%2]")
                            .arg(unit)
                            .arg(slot);
            keys.push_back({slotGroup,
                    QStringLiteral("enabled"),
                    "AnnouncementManager (per-effect on/off announcement)"});
            keys.push_back({slotGroup,
                    QStringLiteral("loaded_effect"),
                    "AnnouncementManager (announces which effect was loaded)"});
        }
    }

    expectAllExist(keys);
}

TEST_F(A11yControlExistenceTest, DeckControlsObservedByAccessibilityLayerExist) {
    std::vector<A11yKey> keys;

    for (const QString& group : deckGroups()) {
        const auto add = [&keys, &group](const QString& item, const char* why) {
            keys.push_back({group, item, why});
        };

        // Transport. Losing any of these means the deck stops narrating itself.
        add(QStringLiteral("play"),
                "AnnouncementManager (play/pause announcement)");
        add(QStringLiteral("end_of_track"),
                "AnnouncementManager (end-of-track warning - without it the "
                "DJ gets no notice that the track is running out)");
        add(QStringLiteral("duration"),
                "AnnouncementManager (time-remaining readout, tts_time)");
        add(QStringLiteral("playposition"),
                "AnnouncementManager (time-remaining and bar-position "
                "readouts)");
        add(QStringLiteral("start"),
                "AnnouncementManager (back-to-start announcement)");
        add(QStringLiteral("cue_gotoandstop"),
                "AnnouncementManager (back-to-start announcement)");
        add(QStringLiteral("cue_default"),
                "AnnouncementManager (smart-cue behaviour)");
        add(QStringLiteral("cue_set"),
                "AnnouncementManager (cue-point-set confirmation)");
        add(QStringLiteral("pfl"),
                "AnnouncementManager (headphone cue on/off announcement)");

        // Beat / tempo. Also what EngineBeatClick follows to place its clicks.
        add(QStringLiteral("bpm"),
                "AnnouncementManager (tts_bpm readout) and EngineBeatClick "
                "(click timing)");
        add(QStringLiteral("beat_distance"),
                "EngineBeatClick (click phase - a wrong or dummy value makes "
                "the metronome drift off the beat)");
        add(QStringLiteral("rate_ratio"),
                "AnnouncementManager (pitch/tempo readout)");
        add(QStringLiteral("beats_set_halve"),
                "AnnouncementManager (half-tempo fix confirmation)");
        add(QStringLiteral("beats_set_double"),
                "AnnouncementManager (double-tempo fix confirmation)");
        add(QStringLiteral("key"),
                "AnnouncementManager (tts_key readout)");
        add(QStringLiteral("keylock"),
                "AnnouncementManager (key lock on/off announcement)");
        add(QStringLiteral("quantize"),
                "AnnouncementManager (quantize on/off announcement)");
        add(QStringLiteral("sync_enabled"),
                "AnnouncementManager (beat-sync vs sync-lock announcement)");

        // Loops and beatjumps.
        add(QStringLiteral("loop_enabled"),
                "AnnouncementManager (loop on/off announcement)");
        add(QStringLiteral("beatloop_size"),
                "AnnouncementManager (loop length readout)");
        add(QStringLiteral("beatjump_size"),
                "AnnouncementManager (beatjump size readout)");
        add(QStringLiteral("beatjump_forward"),
                "AnnouncementManager (beatjump confirmation)");
        add(QStringLiteral("beatjump_backward"),
                "AnnouncementManager (beatjump confirmation)");

        // Channel strip.
        add(QStringLiteral("volume"),
                "AnnouncementManager (channel fader readout)");
        add(QStringLiteral("pregain"),
                "AnnouncementManager (gain trim readout)");

        // Vinyl control. These change mode on their own (a loop or a seek
        // drops absolute mode); with no feedback a blind DJ cannot tell why
        // the deck stopped following the turntable.
        add(QStringLiteral("vinylcontrol_enabled"),
                "AnnouncementManager (vinyl control on/off announcement)");
        add(QStringLiteral("vinylcontrol_mode"),
                "AnnouncementManager (absolute/relative/constant mode "
                "announcement)");
        add(QStringLiteral("vinylcontrol_cueing"),
                "AnnouncementManager (needle-drop cueing announcement)");

        // Fork-added pre-roll controls (EngineBuffer + CueControl). CueControl
        // binds disable_preroll with AllowMissingOrInvalid, so a rebase that
        // drops the EngineBuffer hunk silently restores upstream's pre-roll
        // behaviour - the deck starts playing silence before the first beat.
        add(QStringLiteral("disable_preroll"),
                "CueControl and EngineBuffer (fork-added: suppresses silent "
                "pre-roll before the first beat)");
        add(QStringLiteral("preroll_limit_beats"),
                "EngineBuffer (fork-added: clamps how much pre-roll is "
                "allowed)");

        // Hotcues. AnnouncementManager announces the first eight.
        for (int i = 1; i <= 8; ++i) {
            keys.push_back({group,
                    QStringLiteral("hotcue_%1_status").arg(i),
                    "AnnouncementManager (hotcue set/cleared announcement)"});
            keys.push_back({group,
                    QStringLiteral("hotcue_%1_activate").arg(i),
                    "AnnouncementManager (hotcue recall announcement)"});
        }

        // Per-deck effect racks, created by EffectsManager::addDeck().
        const QString eqGroup =
                QStringLiteral("[EqualizerRack1_%1_Effect1]").arg(group);
        for (int band = 1; band <= 3; ++band) {
            keys.push_back({eqGroup,
                    QStringLiteral("parameter%1").arg(band),
                    "AnnouncementManager (EQ low/mid/high knob readout)"});
        }
        const QString quickEffectGroup =
                QStringLiteral("[QuickEffectRack1_%1]").arg(group);
        keys.push_back({quickEffectGroup,
                QStringLiteral("super1"),
                "AnnouncementManager (filter knob readout)"});
        keys.push_back({quickEffectGroup,
                QStringLiteral("loaded_chain_preset"),
                "AnnouncementManager (announces the selected filter effect)"});

        // "Is this deck routed through effect unit N at all".
        for (int unit = 1; unit <= 4; ++unit) {
            keys.push_back({QStringLiteral("[EffectRack1_EffectUnit%1]")
                                    .arg(unit),
                    QStringLiteral("group_%1_enable").arg(group),
                    "AnnouncementManager (effect unit routing announcement)"});
        }
    }

    expectAllExist(keys);
}

// ---------------------------------------------------------------------------
// Controls owned by subsystems BaseSignalPathTest does not build.
//
// These are split out and each one constructs its real owner, so the controls
// are still created by production code. They are NOT dropped: the point of this
// file is that nothing the accessibility layer touches goes unchecked.
// ---------------------------------------------------------------------------

/// [Recording],status is created by RecordingManager, which BaseSignalPathTest
/// does not construct. It is cheap to build (config + EngineMixer), so build it.
///
/// AnnouncementManager announces both recording transitions: silence here means
/// the DJ believes they are recording a set that is not being recorded.
TEST_F(A11yControlExistenceTest, RecordingStatusControlExists) {
    RecordingManager recordingManager(m_pConfig, m_pEngineMixer);

    expectAllExist({
            {QStringLiteral("[Recording]"),
                    QStringLiteral("status"),
                    "AnnouncementManager (recording started/stopped "
                    "announcement)"},
    });
}

/// [Library] controls are created by LibraryControl, which normally needs a full
/// Library (database, TrackCollectionManager, PlayerManager). Its constructor
/// only stores the Library pointer as its QObject parent and never dereferences
/// it, so a null Library is enough to make it create its ControlObjects - which
/// is all this test needs, and keeps the controls coming from production code.
TEST_F(A11yControlExistenceTest, LibraryControlsObservedByAccessibilityLayerExist) {
    LibraryControl libraryControl(nullptr, m_pConfig);

    std::vector<A11yKey> keys{
            // Upstream-owned, observed by the fork.
            {QStringLiteral("[Library]"),
                    QStringLiteral("sort_column"),
                    "AnnouncementManager (speaks the new sort column)"},
            {QStringLiteral("[Library]"),
                    QStringLiteral("sort_order"),
                    "AnnouncementManager (speaks ascending/descending)"},
            {QStringLiteral("[Library]"),
                    QStringLiteral("focused_widget"),
                    "AnnouncementManager (suppresses track announcements when "
                    "focus leaves the track list)"},

            // Fork-added. Upstream cannot rename these, but a rebase can drop
            // the hunk that creates them, which is the same outcome for the
            // user: the keyboard shortcut stops doing anything.
            {QStringLiteral("[Library]"),
                    QStringLiteral("AddToCrate"),
                    "LibraryControl (fork-added: file the selected track into "
                    "a crate from the keyboard)"},
            {QStringLiteral("[Library]"),
                    QStringLiteral("AddToPlaylist"),
                    "LibraryControl (fork-added: file the selected track into "
                    "a playlist from the keyboard)"},
    };

    // Fork-added per-deck quick-add. LibraryControl fixes this at four decks,
    // independent of how many decks the engine created.
    for (int deck = 1; deck <= 4; ++deck) {
        const QString group = QStringLiteral("[Channel%1]").arg(deck);
        keys.push_back({group,
                QStringLiteral("quick_add_to_crate"),
                "LibraryControl (fork-added: file the playing track into a "
                "crate without leaving the deck)"});
        keys.push_back({group,
                QStringLiteral("quick_add_to_playlist"),
                "LibraryControl (fork-added: file the playing track into a "
                "playlist without leaving the deck)"});
    }

    expectAllExist(keys);
}

/// [Library],key_notation has no owner we can cheaply build - it is created by
/// Library itself, and even upstream's own LibraryTest fixture works around this
/// by constructing the ControlObject by hand.
///
/// Asserting existence would therefore prove nothing. What we can guard is the
/// thing that actually breaks: AnnouncementManager reads this control by hard
/// coded string literal while upstream refers to it through a named constant.
/// If upstream retargets the constant, the literal silently stops matching and
/// every spoken musical key falls back to the default notation.
TEST(A11yControlKeyLiterals, KeyNotationLiteralStillMatchesUpstreamConstant) {
    const ConfigKey& upstream = mixxx::library::prefs::kKeyNotationConfigKey;
    EXPECT_EQ(QStringLiteral("[Library]"), upstream.group)
            << "AnnouncementManager reads [Library],key_notation by literal; "
               "upstream moved the constant to group '"
            << upstream.group.toStdString()
            << "'. Spoken musical keys will silently use the wrong notation.";
    EXPECT_EQ(QStringLiteral("key_notation"), upstream.item)
            << "AnnouncementManager reads [Library],key_notation by literal; "
               "upstream renamed the item to '"
            << upstream.item.toStdString()
            << "'. Spoken musical keys will silently use the wrong notation.";
}

/// The accessibility menu's own controls, created by AccessMenuController.
///
/// Worth guarding despite being fork-owned: AccessMenuController is the blind
/// user's entire settings UI, and its value items are read through
/// ControlFlag::NoWarnIfMissing - the one flag that suppresses even the console
/// warning. If a rebase breaks these bindings there is no diagnostic anywhere.
TEST_F(A11yControlExistenceTest, AccessMenuControlsExist) {
    AccessMenuController menu([](const QString&) {}, m_pConfig);

    expectAllExist({
            {QStringLiteral("[AccessMenu]"),
                    QStringLiteral("open"),
                    "AccessMenuController (open the accessibility menu)"},
            {QStringLiteral("[AccessMenu]"),
                    QStringLiteral("close"),
                    "AccessMenuController (close the accessibility menu)"},
            {QStringLiteral("[AccessMenu]"),
                    QStringLiteral("navigate"),
                    "AccessMenuController (move between menu items)"},
            {QStringLiteral("[AccessMenu]"),
                    QStringLiteral("activate"),
                    "AccessMenuController (enter a submenu or value editor)"},
            {QStringLiteral("[AccessMenu]"),
                    QStringLiteral("back"),
                    "AccessMenuController (leave a submenu)"},
            {QStringLiteral("[AccessMenu]"),
                    QStringLiteral("confirm"),
                    "AccessMenuController (commit a value edit)"},
            {QStringLiteral("[AccessMenu]"),
                    QStringLiteral("active"),
                    "AccessMenuController (menu-open state, read by the skin "
                    "and by AnnouncementManager)"},
    });
}

/// The on-demand readout buttons (tts_status, tts_time, ...) and the controller
/// feedback hooks ([Tts],repeat / shift / pad_mode) are created by
/// AnnouncementManager's constructor and by connectGroupControls() for each deck
/// the PlayerManager reports. Constructing a real AnnouncementManager against
/// the real engine checks two things at once: that those buttons still get
/// created, and that AnnouncementManager still constructs at all against a
/// rebased engine.
TEST_F(A11yControlExistenceTest, AnnouncementManagerOwnedControlsExist) {
    StubPlayerManager playerManager({m_pMixerDeck1, m_pMixerDeck2, m_pMixerDeck3});
    AnnouncementManager announcementManager(nullptr, // no Library needed
            &playerManager,
            m_pConfig,
            std::make_unique<NullTtsEngine>(),
            nullptr,  // no EngineTts sink
            nullptr); // no EngineEarcon sink

    // Controller feedback hooks. Nothing drives these except a controller
    // mapping, so if a rebase drops them the only symptom is that mapped
    // hardware silently stops announcing - no error, no warning.
    std::vector<A11yKey> keys{
            {QStringLiteral("[Tts]"),
                    QStringLiteral("repeat"),
                    "AnnouncementManager (repeat the last announcement, "
                    "Alt+Shift+R)"},
            {QStringLiteral("[Tts]"),
                    QStringLiteral("shift"),
                    "AnnouncementManager (controller shift-button feedback)"},
            {QStringLiteral("[Tts]"),
                    QStringLiteral("pad_mode"),
                    "AnnouncementManager (controller performance-pad mode "
                    "feedback)"},
    };

    for (int deck = 0; deck < static_cast<int>(deckGroups().size()); ++deck) {
        // Use the same helper AnnouncementManager uses, so a change to the
        // deck group naming scheme shows up here too.
        const QString group = PlayerManager::groupForDeck(deck);
        keys.push_back({group,
                QStringLiteral("tts_status"),
                "AnnouncementManager (speak full deck status on demand)"});
        keys.push_back({group,
                QStringLiteral("tts_time"),
                "AnnouncementManager (speak time remaining on demand)"});
        keys.push_back({group,
                QStringLiteral("tts_bpm"),
                "AnnouncementManager (speak BPM on demand)"});
        keys.push_back({group,
                QStringLiteral("tts_key"),
                "AnnouncementManager (speak musical key on demand)"});
        keys.push_back({group,
                QStringLiteral("tts_bar"),
                "AnnouncementManager (speak bar position on demand)"});
        keys.push_back({group,
                QStringLiteral("tts_track"),
                "AnnouncementManager (speak track name on demand)"});
    }

    expectAllExist(keys);
}

// ---------------------------------------------------------------------------
// Deliberately not covered, recorded so the next person does not have to
// re-derive it:
//
// [Shoutcast],enabled - upstream is mid-migration away from this legacy group
// name (see src/broadcast/defs_broadcast.h), which makes it a natural rebase
// hazard. As of this commit, however, no accessibility code reads it: grepping
// src/ finds it only in upstream's broadcast preferences, controlpickermenu and
// the stock skins. There is nothing for this fork to guard. If the accessibility
// menu ever gains a broadcast toggle, add [Shoutcast],enabled here - and note
// that AccessMenuController would bind it with ControlFlag::NoWarnIfMissing, so
// a rename would fail completely silently.
// ---------------------------------------------------------------------------

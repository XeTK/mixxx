// Rebase guard for the fork's free-text speech channel,
// Library::announceText().
//
// WHY THIS EXISTS
// ---------------
// announceText() is how this accessibility fork speaks events that have no
// ControlObject behind them: the boot-time sound-device dialogs, the
// playlist/crate create/rename/duplicate/delete dialogs, "Playlists view" /
// "Crates view", the load-blocked message, "Deck N, no track loaded". Its call
// sites live almost entirely in files that this fork has modified heavily and
// that therefore conflict hardest with upstream (mixxxmainwindow.cpp,
// library.cpp, wtracktableview.cpp, baseplaylistfeature.cpp, cratefeature.cpp,
// cratefeaturehelper.cpp, librarycontrol.cpp).
//
// If a conflict resolution drops one of those calls, the failure is *silent*:
// nothing crashes, no other test goes red, the application simply stops
// speaking that one event. A blind user discovers it by noticing silence.
//
// This is a different failure mode from the one a11ycontrols_test.cpp guards
// (that a ControlObject still *exists*). Nothing else in the suite covers it.
//
// WHAT THIS FILE ASSERTS, AND HOW STRONG EACH PART IS
// ---------------------------------------------------
// 1. AnnounceTextBehaviourTest  --  a genuine behavioural assertion.
//    It builds a real Library and checks that announceText() still reaches the
//    TTS transport, and that Library::slotSwitchToView() still announces the
//    Playlists/Crates panes. This proves those code paths *execute*. It covers
//    the shared transport plus 2 of the call sites.
//
// 2. AnnounceTextCallSiteCensusTest  --  a SOURCE-LEVEL CENSUS, NOT A
//    BEHAVIOURAL ASSERTION. Read that twice before trusting it. It parses the
//    fork's own .cpp files off disk and checks that each function that is
//    supposed to announce still *contains* an announceText() call. It proves
//    the line is still in the source. It does NOT prove the line runs, that it
//    runs at the right moment, or that the text is correct or audible.
//    It exists because the remaining call sites sit behind modal dialogs
//    (QInputDialog/QMessageBox exec()) or behind full MixxxMainWindow/
//    CoreServices boot, and are not reachable from a unit test. A crude guard
//    over the real risk beats no guard at all -- but do not mistake it for
//    coverage.
//
// DESIGN NOTES (so this survives legitimate refactoring)
// ------------------------------------------------------
// * No line numbers anywhere. Anchors are function names -- code identifiers,
//   which a rebase preserves -- never tr() literals, which may legitimately be
//   reworded.
// * Counts are asserted as minimums (>=), so adding new announcements never
//   turns this red. Only losing one does.
// * When a per-function check fails, the message says explicitly that the
//   other possibility is a legitimate rename, and tells you to update the
//   table below rather than to "fix" the assert.

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <gsl/pointers>
#include <memory>

#include "control/controlindicatortimer.h"
#include "database/mixxxdb.h"
#include "effects/effectsmanager.h"
#include "engine/channelhandle.h"
#include "engine/enginemixer.h"
#include "library/coverartcache.h"
#include "library/library.h"
#include "library/trackcollectionmanager.h"
#include "mixer/playermanager.h"
#include "recording/recordingmanager.h"
#include "soundio/soundmanager.h"
#include "test/mixxxdbtest.h"
#include "test/soundsourceproviderregistration.h"
#include "track/track.h"
#include "util/db/dbconnectionpooled.h"

namespace {

// ---------------------------------------------------------------------------
// Part 1: behavioural
// ---------------------------------------------------------------------------

void deleteTrackDirectly(Track* pTrack) {
    // Unit tests have no main event loop, so deleteLater() would leak.
    delete pTrack;
}

// Builds a real Library. This mirrors the setup already used by
// PlayerManagerTest and MappingTestFixture -- Library's constructor connects to
// PlayerManager and hands RecordingManager to two of its features, so neither
// can be null. Decks/samplers are deliberately not created: nothing here needs
// a loaded track, and skipping them keeps the fixture cheap.
//
// LibraryTest is not usable as a base class here for the same reason
// PlayerManagerTest cannot use it: it creates a [Library],key_notation
// ControlObject that Library also creates, which trips a debug assert.
class AnnounceTextBehaviourTest : public MixxxDbTest,
                                  public SoundSourceProviderRegistration {
  public:
    AnnounceTextBehaviourTest()
            : MixxxDbTest(true) {
    }

    void SetUp() override {
        auto pChannelHandleFactory = std::make_shared<ChannelHandleFactory>();
        m_pEffectsManager = std::make_shared<EffectsManager>(
                m_pConfig, pChannelHandleFactory);
        m_pEngine = std::make_shared<EngineMixer>(
                m_pConfig,
                "[Master]",
                m_pEffectsManager.get(),
                pChannelHandleFactory,
                /*bEnableSidechain*/ true);
        m_pSoundManager = std::make_shared<SoundManager>(m_pConfig, m_pEngine.get());
        m_pControlIndicatorTimer =
                std::make_shared<mixxx::ControlIndicatorTimer>(nullptr);
        m_pEngine->registerNonEngineChannelSoundIO(
                gsl::make_not_null(m_pSoundManager.get()));
        m_pPlayerManager = std::make_shared<PlayerManager>(m_pConfig,
                m_pSoundManager.get(),
                m_pEffectsManager.get(),
                m_pEngine.get());
        m_pEffectsManager->setup();

        const auto dbConnection = mixxx::DbConnectionPooled(dbConnectionPooler());
        ASSERT_TRUE(MixxxDb::initDatabaseSchema(dbConnection));

        m_pTrackCollectionManager = std::make_unique<TrackCollectionManager>(
                nullptr,
                m_pConfig,
                dbConnectionPooler(),
                deleteTrackDirectly);
        m_pRecordingManager = std::make_shared<RecordingManager>(
                m_pConfig, m_pEngine.get());

        CoverArtCache::createInstance();

        m_pLibrary = std::make_shared<Library>(
                nullptr,
                m_pConfig,
                dbConnectionPooler(),
                m_pTrackCollectionManager.get(),
                m_pPlayerManager.get(),
                m_pRecordingManager.get());
    }

    void TearDown() override {
        m_pLibrary->stopPendingTasks();
        m_pLibrary.reset();
        CoverArtCache::destroy();
    }

  protected:
    std::shared_ptr<EffectsManager> m_pEffectsManager;
    std::shared_ptr<EngineMixer> m_pEngine;
    std::shared_ptr<SoundManager> m_pSoundManager;
    std::shared_ptr<mixxx::ControlIndicatorTimer> m_pControlIndicatorTimer;
    std::shared_ptr<PlayerManager> m_pPlayerManager;
    std::unique_ptr<TrackCollectionManager> m_pTrackCollectionManager;
    std::shared_ptr<RecordingManager> m_pRecordingManager;
    std::shared_ptr<Library> m_pLibrary;
};

// The shared transport. announceText() has no signal of its own -- it reuses
// quickPickerItemHighlighted() with a sentinel row of -1 and a sibling count of
// 0, which is what tells the announcement layer "this is free text, not a list
// item, so do not append a position". If a rebase drops the emit, changes the
// signal, or changes the sentinel, all of the fork's free-text speech goes
// silent (or starts saying "of 0") at once. This is the single highest-value
// assertion in the file.
TEST_F(AnnounceTextBehaviourTest, AnnounceText_EmitsFreeTextOnQuickPickerSignal) {
    QSignalSpy spy(m_pLibrary.get(), &Library::quickPickerItemHighlighted);
    ASSERT_TRUE(spy.isValid());

    const QString text = QStringLiteral("guard probe text");
    m_pLibrary->announceText(text);

    ASSERT_EQ(1, spy.count());
    const QList<QVariant> args = spy.at(0);
    ASSERT_EQ(3, args.size());
    EXPECT_QSTRING_EQ(text, args.at(0).toString());
    EXPECT_EQ(-1, args.at(1).toInt()) << "announceText() must pass the -1 "
                                         "sentinel row so the announcement is "
                                         "spoken as free text, not as a list "
                                         "item with a position.";
    EXPECT_EQ(0, args.at(2).toInt()) << "announceText() must pass a sibling "
                                        "count of 0.";
}

// Real call site, exercised for real: entering the Playlists or Crates pane is
// otherwise silent, because the sidebar only announces selection movement, not
// the pane actually switching. Covers library.cpp.
TEST_F(AnnounceTextBehaviourTest, SwitchToPlaylistHome_IsAnnounced) {
    QSignalSpy spy(m_pLibrary.get(), &Library::quickPickerItemHighlighted);
    ASSERT_TRUE(spy.isValid());

    m_pLibrary->slotSwitchToView(QStringLiteral("PLAYLISTHOME"));

    ASSERT_EQ(1, spy.count()) << "Entering the Playlists pane must speak "
                                 "something.";
    EXPECT_FALSE(spy.at(0).at(0).toString().isEmpty());
    EXPECT_EQ(-1, spy.at(0).at(1).toInt());
}

TEST_F(AnnounceTextBehaviourTest, SwitchToCrateHome_IsAnnounced) {
    QSignalSpy spy(m_pLibrary.get(), &Library::quickPickerItemHighlighted);
    ASSERT_TRUE(spy.isValid());

    m_pLibrary->slotSwitchToView(QStringLiteral("CRATEHOME"));

    ASSERT_EQ(1, spy.count()) << "Entering the Crates pane must speak "
                                 "something.";
    EXPECT_FALSE(spy.at(0).at(0).toString().isEmpty());
    EXPECT_EQ(-1, spy.at(0).at(1).toInt());
}

// Negative control. Without this, the two tests above would still pass if
// slotSwitchToView() were changed to announce unconditionally -- which would be
// a regression in its own right (every view switch would babble).
TEST_F(AnnounceTextBehaviourTest, SwitchToOtherView_IsNotAnnounced) {
    QSignalSpy spy(m_pLibrary.get(), &Library::quickPickerItemHighlighted);
    ASSERT_TRUE(spy.isValid());

    m_pLibrary->slotSwitchToView(QStringLiteral("TRACKS"));

    EXPECT_EQ(0, spy.count()) << "Only the Playlists and Crates home views are "
                                 "supposed to be announced by "
                                 "slotSwitchToView().";
}

// ---------------------------------------------------------------------------
// Part 2: source-level census
//
// Everything below reads the fork's source off disk. It is a census, not a
// behavioural assertion -- see the header comment.
// ---------------------------------------------------------------------------

// RESOURCE_FOLDER is baked in by CMake as "${CMAKE_CURRENT_SOURCE_DIR}/res",
// so its parent is the source tree root. This is more robust than relying on
// the working directory.
QDir sourceRoot() {
    return QDir(QFileInfo(QStringLiteral(RESOURCE_FOLDER)).absolutePath());
}

// Removes // line comments so that commented-out or merely *mentioned*
// announceText() calls are not counted as live call sites. "://" is left alone
// so URLs inside string literals survive. Block comments are not stripped; none
// of the census targets use them around a call site, and adding a naive /* */
// stripper would risk mangling string literals.
QString stripLineComments(const QString& source) {
    QStringList out;
    const QStringList lines = source.split(QLatin1Char('\n'));
    out.reserve(lines.size());
    for (const QString& line : lines) {
        int searchFrom = 0;
        int cut = -1;
        while (true) {
            const int idx = line.indexOf(QStringLiteral("//"), searchFrom);
            if (idx < 0) {
                break;
            }
            if (idx > 0 && line.at(idx - 1) == QLatin1Char(':')) {
                searchFrom = idx + 2;
                continue;
            }
            cut = idx;
            break;
        }
        out.append(cut < 0 ? line : line.left(cut));
    }
    return out.join(QLatin1Char('\n'));
}

// Reads a source file relative to the repository root, with comments stripped.
// Fails the calling test (rather than skipping) if the file is missing: a guard
// that quietly opts out of guarding is worse than no guard, because it looks
// green.
bool readForkSource(const QString& relativePath, QString* pOut) {
    const QString absolutePath = sourceRoot().filePath(relativePath);
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ADD_FAILURE() << "Could not read " << qPrintable(absolutePath)
                      << ". This census reads the fork's own source tree; if "
                         "the file moved, update the table in "
                         "announcetext_guard_test.cpp.";
        return false;
    }
    QTextStream in(&file);
    *pOut = stripLineComments(in.readAll());
    return true;
}

// Returns the body of a top-level function definition, located by the start of
// its signature (e.g. "void CrateFeature::slotRenameCrate("). The body is taken
// up to the next closing brace in column 0, which is how clang-format ends every
// top-level definition in this codebase. Returns a null QString if the signature
// is not found.
QString functionBody(const QString& source, const QString& signaturePrefix) {
    const int start = source.indexOf(signaturePrefix);
    if (start < 0) {
        return QString();
    }
    const int end = source.indexOf(QStringLiteral("\n}\n"), start);
    return end < 0 ? source.mid(start) : source.mid(start, end - start);
}

int countAnnounceCalls(const QString& text) {
    // The definition itself ("void Library::announceText(") is not a call site.
    int count = 0;
    int from = 0;
    while (true) {
        const int idx = text.indexOf(QStringLiteral("announceText("), from);
        if (idx < 0) {
            break;
        }
        from = idx + 1;
        if (text.mid(0, idx).endsWith(QStringLiteral("void Library::"))) {
            continue;
        }
        ++count;
    }
    return count;
}

// The census table.
//
// One row per function that is supposed to speak, with the number of
// announceText() calls it had when this guard was written. Asserted as a
// minimum, so adding announcements is always fine.
//
// If you are here because a row failed: either the announcement really was
// lost (fix the source), or the function was legitimately renamed or split
// upstream (update this row). Do not simply lower the number.
struct CensusRow {
    const char* relativePath;
    const char* signaturePrefix;
    int minimumCalls;
    const char* what;
};

// clang-format off
constexpr CensusRow kCensus[] = {
        // Boot-time dialogs. These are unreachable from a unit test: they block
        // on QMessageBox::exec() inside a fully constructed MixxxMainWindow.
        {"src/mixxxmainwindow.cpp", "void MixxxMainWindow::initialize(", 1,
                "spoken [AccessMenu] popup menu controller callback"},
        {"src/mixxxmainwindow.cpp", "void MixxxMainWindow::alwaysHideMenuBarDlg(", 1,
                "menu-bar-hide prompt"},
        {"src/mixxxmainwindow.cpp", "MixxxMainWindow::soundDeviceBusyDlg(", 1,
                "sound device busy dialog"},
        {"src/mixxxmainwindow.cpp", "MixxxMainWindow::soundDeviceErrorMsgDlg(", 1,
                "sound device error dialog"},
        {"src/mixxxmainwindow.cpp", "MixxxMainWindow::noOutputDlg(", 1,
                "no configured output dialog"},
        {"src/mixxxmainwindow.cpp", "void MixxxMainWindow::slotNoVinylControlInputConfigured(", 1,
                "missing vinyl control input warning"},
        {"src/mixxxmainwindow.cpp", "void MixxxMainWindow::slotNoDeckPassthroughInputConfigured(", 1,
                "missing deck passthrough input warning"},
        {"src/mixxxmainwindow.cpp", "void MixxxMainWindow::slotNoMicrophoneInputConfigured(", 1,
                "missing microphone input warning"},
        {"src/mixxxmainwindow.cpp", "void MixxxMainWindow::slotNoAuxiliaryInputConfigured(", 1,
                "missing auxiliary input warning"},
        {"src/mixxxmainwindow.cpp", "void MixxxMainWindow::slotLibraryScanSummaryDlg(", 1,
                "library scan summary"},
        {"src/mixxxmainwindow.cpp", "void MixxxMainWindow::checkDirectRendering(", 1,
                "direct rendering warning"},

        // Playlist dialogs. Each of these announces the dialog opening, echoes
        // back what was typed, speaks each validation failure, and confirms the
        // result -- hence the counts above 1. They block on
        // QInputDialog::getText().
        {"src/library/trackset/baseplaylistfeature.cpp",
                "void BasePlaylistFeature::slotCreatePlaylist(", 4,
                "narrated create-playlist dialog"},
        {"src/library/trackset/baseplaylistfeature.cpp",
                "void BasePlaylistFeature::slotRenamePlaylist(", 5,
                "narrated rename-playlist dialog"},
        {"src/library/trackset/baseplaylistfeature.cpp",
                "void BasePlaylistFeature::slotDuplicatePlaylist(", 5,
                "narrated duplicate-playlist dialog"},
        {"src/library/trackset/baseplaylistfeature.cpp",
                "void BasePlaylistFeature::slotDeletePlaylist(", 2,
                "narrated delete-playlist confirmation"},

        // Crate dialogs, same pattern.
        {"src/library/trackset/crate/cratefeature.cpp",
                "void CrateFeature::slotDeleteCrate(", 2,
                "narrated delete-crate confirmation"},
        {"src/library/trackset/crate/cratefeature.cpp",
                "void CrateFeature::slotRenameCrate(", 5,
                "narrated rename-crate dialog"},
        {"src/library/trackset/crate/cratefeaturehelper.cpp",
                "CrateId CrateFeatureHelper::createEmptyCrate(", 4,
                "narrated create-crate dialog"},
        {"src/library/trackset/crate/cratefeaturehelper.cpp",
                "CrateId CrateFeatureHelper::duplicateCrate(", 5,
                "narrated duplicate-crate dialog"},

        // Track table. moveRows() speaks the new position after a reorder;
        // loadSelectedTrackToGroup() speaks the load-blocked message
        // ("Deck 1 is playing, load blocked"). Both need a live WTrackTableView
        // with a model and a selection.
        {"src/widget/wtracktableview.cpp",
                "void WTrackTableView::moveRows(", 2,
                "track reorder position feedback"},
        {"src/widget/wtracktableview.cpp",
                "void WTrackTableView::loadSelectedTrackToGroup(", 1,
                "load-blocked message"},

        // Library control. Needs a bound WLibrary widget to reach.
        {"src/library/librarycontrol.cpp",
                "void LibraryControl::deckQuickAdd(", 1,
                "\"Deck N, no track loaded\""},

        // Also censused even though it is covered behaviourally above, so that
        // the per-file totals below add up and so a rename is reported here
        // too.
        {"src/library/library.cpp",
                "void Library::slotSwitchToView(", 2,
                "\"Playlists view\" / \"Crates view\""},
};
// clang-format on

// Per-file totals, as a backstop. If a per-function row fails but the file
// total still passes, the announcement almost certainly moved (a rename or a
// helper being extracted) rather than being deleted -- which is the signal you
// want when triaging a rebase.
struct FileTotal {
    const char* relativePath;
    int minimumCalls;
};

constexpr FileTotal kFileTotals[] = {
        {"src/library/trackset/baseplaylistfeature.cpp", 16},
        {"src/mixxxmainwindow.cpp", 11},
        {"src/library/trackset/crate/cratefeaturehelper.cpp", 9},
        {"src/library/trackset/crate/cratefeature.cpp", 7},
        {"src/widget/wtracktableview.cpp", 3},
        {"src/library/library.cpp", 2},
        {"src/library/librarycontrol.cpp", 1},
};

// Total number of announceText() call sites in the fork at the time of writing.
constexpr int kTotalCallSites = 49;

class AnnounceTextCallSiteCensusTest : public testing::Test {};

TEST_F(AnnounceTextCallSiteCensusTest, EveryAnnouncingFunctionStillAnnounces) {
    for (const CensusRow& row : kCensus) {
        QString source;
        if (!readForkSource(QString::fromLatin1(row.relativePath), &source)) {
            continue;
        }
        const QString body = functionBody(
                source, QString::fromLatin1(row.signaturePrefix));
        if (body.isNull()) {
            ADD_FAILURE()
                    << row.relativePath << ": could not find '"
                    << row.signaturePrefix << "'. Either it was renamed or "
                       "removed upstream. If the rename is legitimate, update "
                       "the census table; the announcement it used to make was: "
                    << row.what;
            continue;
        }
        const int found = countAnnounceCalls(body);
        EXPECT_GE(found, row.minimumCalls)
                << row.relativePath << " :: " << row.signaturePrefix
                << " has " << found << " announceText() call(s), expected at "
                << "least " << row.minimumCalls
                << ". A fork accessibility announcement appears to have been "
                   "lost -- most likely to a merge conflict resolution. "
                   "The affected announcement is: "
                << row.what
                << ". This is the silent kind of regression: nothing crashes, "
                   "the app just stops speaking this event.";
    }
}

TEST_F(AnnounceTextCallSiteCensusTest, PerFileCallSiteCountsHaveNotDropped) {
    int grandTotal = 0;
    for (const FileTotal& entry : kFileTotals) {
        QString source;
        if (!readForkSource(QString::fromLatin1(entry.relativePath), &source)) {
            continue;
        }
        const int found = countAnnounceCalls(source);
        grandTotal += found;
        EXPECT_GE(found, entry.minimumCalls)
                << entry.relativePath << " has " << found
                << " announceText() call site(s), expected at least "
                << entry.minimumCalls
                << ". If the per-function census above is green, an "
                   "announcement was moved out of this file rather than "
                   "deleted; if it is red too, one was deleted.";
    }
    EXPECT_GE(grandTotal, kTotalCallSites)
            << "The fork had " << kTotalCallSites
            << " announceText() call sites; only " << grandTotal
            << " remain across the files this census knows about.";
}

// Every boot dialog builds its spoken text with a dedicated static helper
// (MixxxMainWindow::noOutputSpeech() and friends), and those helpers are unit
// tested in bootdialog_speech_test.cpp. But that test only proves the helper
// returns the right string -- it says nothing about whether anything still
// *calls* it. A conflict resolution that keeps the helpers and drops the
// announceText() lines would leave bootdialog_speech_test.cpp fully green while
// every boot dialog fell silent. This closes that gap.
//
// Matching on helper names rather than on tr() literals is deliberate: the
// spoken wording may legitimately be reworded or retranslated, but the helper
// name is a code identifier that a rebase preserves.
TEST_F(AnnounceTextCallSiteCensusTest, EveryBootDialogSpeechHelperIsStillCalled) {
    QString source;
    ASSERT_TRUE(readForkSource(QStringLiteral("src/mixxxmainwindow.cpp"), &source));

    // Collapse whitespace: these calls are wrapped across two or three lines.
    const QString flattened = source.simplified();

    const QStringList helpers = {
            QStringLiteral("menuBarHideSpeech"),
            QStringLiteral("soundDeviceBusySpeech"),
            QStringLiteral("soundDeviceErrorSpeech"),
            QStringLiteral("noOutputSpeech"),
            QStringLiteral("noVinylControlInputSpeech"),
            QStringLiteral("noPassthroughInputSpeech"),
            QStringLiteral("noMicrophoneInputSpeech"),
            QStringLiteral("noAuxiliaryInputSpeech"),
            QStringLiteral("libraryScanSummarySpeech"),
            QStringLiteral("directRenderingSpeech"),
    };

    for (const QString& helper : helpers) {
        const QRegularExpression re(
                QStringLiteral("announceText\\(\\s*%1\\s*\\(").arg(helper));
        EXPECT_TRUE(re.match(flattened).hasMatch())
                << "mixxxmainwindow.cpp no longer passes "
                << qPrintable(helper)
                << "() to announceText(). bootdialog_speech_test.cpp will "
                   "still pass, because the helper itself is intact -- but "
                   "nothing speaks it any more, so this boot dialog is now "
                   "silent for a screen reader user.";
    }
}

} // namespace

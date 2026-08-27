// Regression guard for the fork's accessibility keyboard bindings.
//
// WHY THIS EXISTS
// ---------------
// res/keyboard/*.kbd.cfg carries ~30 fork-added accessibility bindings per
// locale (TTS readouts, beat click, crossfader lock, split cue, quick filing,
// ...). Upstream regenerates and reshuffles those files periodically, so every
// rebase is an opportunity to lose them: a conflict resolved as "theirs", a
// reformat, or a single dropped line and the shortcuts are gone.
//
// Nothing else in the tree catches that. KeyboardEventFilter only complains
// ("Keyboard key is configured for nonexistent control") when the key is
// actually *pressed*, via qDebug() -- which a blind user will never see.
//
// WHAT IT ASSERTS
// ---------------
// Structural properties, deliberately NOT specific chord strings. Chords are in
// active churn (moving off Ctrl+Alt so they stop fighting macOS VoiceOver,
// new mixer/FX chords, keylock). Hard-coding "keylock is Ctrl+Alt+K" would make
// this test a merge obstacle instead of a safety net. So we assert:
//
//   * every accessibility *control* named in en_US is bound in all 12 locales;
//   * the accessibility chords are identical across all 12 (they are
//     deliberately not localized, so a partial edit is always a bug);
//   * every control bound in en_US is bound in every other locale;
//   * no chord collides with another, except a short, explicit allowlist of
//     collisions we inherited from upstream;
//   * a spot-check that engine-backed accessibility bindings resolve to real
//     ControlObjects.
//
// The exhaustive "does this ControlObject exist" table lives in
// a11ycontrols_test.cpp. This file is about the *bindings*.

#include <gtest/gtest.h>

#include <QFile>
#include <QHash>
#include <QKeySequence>
#include <QList>
#include <QMultiHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <memory>

#include "control/controlobject.h"
#include "preferences/configobject.h"
#include "test/mixxxtest.h"
#include "test/signalpathtest.h"

namespace {

/// Every keyboard mapping shipped in res/keyboard/. A blind user running a
/// non-English system locale gets one of these, so all of them have to carry
/// the accessibility bindings -- not just en_US.
const QStringList kShippedLocales = {
        QStringLiteral("cs_CZ"),
        QStringLiteral("da_DK"),
        QStringLiteral("de_CH"),
        QStringLiteral("de_DE"),
        QStringLiteral("el_GR"),
        QStringLiteral("en_US"),
        QStringLiteral("es_ES"),
        QStringLiteral("fi_FI"),
        QStringLiteral("fr_CH"),
        QStringLiteral("fr_FR"),
        QStringLiteral("it_IT"),
        QStringLiteral("ru_RU"),
};

/// en_US is the mapping CoreServices::initializeKeyboard() falls back to when
/// the user's locale has no mapping, so it is the reference for parity checks.
const QString kReferenceLocale = QStringLiteral("en_US");

/// Upstream's main-menu accelerators. KeyboardEventFilter::eventFilter()
/// explicitly skips this group -- they are dispatched by QAction, not by a
/// ControlObject -- so they are excluded from control-resolution checks.
const QString kMenuShortcutGroup = QStringLiteral("[KeyboardShortcuts]");

/// ConfigObject::parse() has no comment syntax. Upstream's "// DEVELOPER MENU"
/// line in each .kbd.cfg is therefore parsed as a binding whose control is
/// named "//" with the value "DEVELOPER MENU". It is a parser artifact, not a
/// binding; ignore it everywhere.
const QString kCommentArtifactItem = QStringLiteral("//");

/// The fork's accessibility bindings, transcribed from
/// res/keyboard/en_US.kbd.cfg on this branch (30 entries, replicated verbatim
/// into all 12 locale files -- see `git diff 5bcf981583^ HEAD -- res/keyboard`).
///
/// This lists *controls*, not chords, on purpose: which key a feature sits on
/// is expected to move, but the feature disappearing entirely is the regression
/// we are guarding. This is a minimum set, not an exact set, so PRs that add
/// new accessibility bindings do not have to touch this list.
QList<ConfigKey> forkAccessibilityBindings() {
    QList<ConfigKey> keys = {
            // Main/mixer-level accessibility toggles.
            ConfigKey(QStringLiteral("[Master]"), QStringLiteral("crossfader_lock")),
            ConfigKey(QStringLiteral("[Master]"), QStringLiteral("headSplitDecks")),
            ConfigKey(QStringLiteral("[Master]"), QStringLiteral("disable_touch_scratch")),
            // Audible beat-click metronome.
            ConfigKey(QStringLiteral("[BeatClick]"), QStringLiteral("enabled")),
            // Screen-reader / speech output.
            ConfigKey(QStringLiteral("[Tts]"), QStringLiteral("enabled")),
            ConfigKey(QStringLiteral("[Tts]"), QStringLiteral("repeat")),
            // Quick filing from the library.
            ConfigKey(QStringLiteral("[Library]"), QStringLiteral("AddToCrate")),
            ConfigKey(QStringLiteral("[Library]"), QStringLiteral("AddToPlaylist")),
    };

    // Per-deck readouts and eyes-free deck operations. Bound identically on
    // both decks (with a Shift for deck 2).
    const QStringList deckGroups = {
            QStringLiteral("[Channel1]"),
            QStringLiteral("[Channel2]"),
    };
    const QStringList deckItems = {
            // On-demand spoken readouts.
            QStringLiteral("tts_status"),
            QStringLiteral("tts_time"),
            QStringLiteral("tts_bpm"),
            QStringLiteral("tts_key"),
            QStringLiteral("tts_bar"),
            QStringLiteral("tts_track"),
            // Eyes-free beatgrid correction (no waveform to drag).
            QStringLiteral("beats_set_halve"),
            QStringLiteral("beats_set_double"),
            // Quick filing of the loaded track.
            QStringLiteral("quick_add_to_playlist"),
            QStringLiteral("quick_add_to_crate"),
            // Quantize, needed to mix without visual beat alignment.
            QStringLiteral("quantize"),
    };
    for (const QString& group : deckGroups) {
        for (const QString& item : deckItems) {
            keys.append(ConfigKey(group, item));
        }
    }
    return keys;
}

/// Accessibility bindings whose ControlObject is created by the audio engine,
/// and which are therefore resolvable on the BaseSignalPathTest fixture. This
/// is a deliberate spot-check: the remaining accessibility controls
/// (tts_*, [BeatClick],enabled, [Library],*, quick_add_*) are created by
/// AnnouncementManager / LibraryControl / PlayerManager and are covered
/// exhaustively by a11ycontrols_test.cpp. Duplicating that table here would
/// only mean two places to update.
QList<ConfigKey> engineBackedAccessibilityBindings() {
    return {
            ConfigKey(QStringLiteral("[Master]"), QStringLiteral("crossfader_lock")),
            ConfigKey(QStringLiteral("[Master]"), QStringLiteral("headSplitDecks")),
            ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("quantize")),
            ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("beats_set_halve")),
            ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("beats_set_double")),
            ConfigKey(QStringLiteral("[Channel2]"), QStringLiteral("quantize")),
            ConfigKey(QStringLiteral("[Channel2]"), QStringLiteral("beats_set_halve")),
            ConfigKey(QStringLiteral("[Channel2]"), QStringLiteral("beats_set_double")),
    };
}

// ---------------------------------------------------------------------------
// Known, pre-existing chord collisions inherited from upstream.
//
// These are NOT fork bugs and NOT accessibility bindings. They are upstream's
// own mappings colliding with each other, and they predate all of the
// accessibility work. Fixing them is out of scope for a regression guard, but
// silently ignoring *all* collisions would defeat the point -- so they are
// listed here individually, and anything new fails the test.
//
// Revisit this list after each upstream rebase: entries that upstream has fixed
// should be deleted, and it should never grow without a comment explaining why.
//
// Signature format: "<locale>|<control>+<control>+..." with controls sorted.
// ---------------------------------------------------------------------------
const QSet<QString> kKnownUpstreamCollisions = {
        // fr_FR: AZERTY swaps q and a, which moved beatloop_activate onto the
        // 'a' that beatjump_backward already occupies. Upstream's mapping bug;
        // both controls fire on the same key.
        QStringLiteral("fr_FR|[Channel1],beatjump_backward+[Channel1],beatloop_activate"),
};

/// Bindings that upstream ships broken: the chord does not parse, so the
/// binding is dead at runtime. Same policy as the collision allowlist --
/// explicit, commented, and revisited on each upstream rebase.
const QSet<QString> kKnownUnparseableChords = {};

/// Load a shipped keyboard mapping exactly the way CoreServices does
/// (src/coreservices.cpp, CoreServices::initializeKeyboard): resolve the
/// resource dir, append "keyboard/<locale>.kbd.cfg", and hand it to a
/// ConfigObject<ConfigValueKbd>. Using the production loader means this test
/// sees the same parse quirks and the same QKeySequence normalization that
/// Mixxx sees at runtime.
std::unique_ptr<ConfigObject<ConfigValueKbd>> loadKeyboardMapping(const QString& locale) {
    const QString resourcePath = ConfigObject<ConfigValueKbd>::computeResourcePath();
    const QString path = QString(resourcePath)
                                 .append(QStringLiteral("keyboard/"))
                                 .append(locale)
                                 .append(QStringLiteral(".kbd.cfg"));
    EXPECT_TRUE(QFile::exists(path))
            << "Missing keyboard mapping: " << qPrintable(path);
    return std::make_unique<ConfigObject<ConfigValueKbd>>(path);
}

QString describe(const ConfigKey& key) {
    return QStringLiteral("%1,%2").arg(key.group, key.item);
}

bool isRealBinding(const ConfigKey& key) {
    return key.isValid() && key.item != kCommentArtifactItem;
}

/// Every ConfigKey the mapping actually binds, comment artifact excluded.
QSet<ConfigKey> boundControls(const ConfigObject<ConfigValueKbd>& config) {
    QSet<ConfigKey> keys;
    const QMultiHash<ConfigValueKbd, ConfigKey> byChord = config.transpose();
    for (const ConfigKey& key : byChord) {
        if (isRealBinding(key)) {
            keys.insert(key);
        }
    }
    return keys;
}

struct Collision {
    QString chord;
    QStringList controls;

    QString signature(const QString& locale) const {
        return QStringLiteral("%1|%2").arg(locale, controls.join(QLatin1Char('+')));
    }
};

/// Chords bound to more than one control.
///
/// Grouping is by ConfigValueKbd::value, i.e. QKeySequence::toString() of the
/// parsed chord, which is what KeyboardEventFilter effectively matches on. That
/// matters: several real collisions in these files are invisible in the raw
/// text and only appear after Qt normalizes the chord. el_GR binds sigma twice
/// as the two different lowercase forms (sigma and final sigma), and both
/// uppercase to the same key.
///
/// Note this walks every (chord, control) pair explicitly rather than going via
/// uniqueKeys()/values(). ConfigValueKbd's operator== compares QKeySequence
/// while its qHash() hashes the string, and leaning on that pairing to do
/// lookups silently drops collisions.
QList<Collision> findCollisions(const ConfigObject<ConfigValueKbd>& config) {
    const QMultiHash<ConfigValueKbd, ConfigKey> byChord = config.transpose();

    QHash<QString, QStringList> controlsByChord;
    for (auto it = byChord.cbegin(); it != byChord.cend(); ++it) {
        // Unparseable chords all normalize to the empty string. Grouping them
        // together would report a bogus collision; they are already reported
        // individually by EveryBindingIsWellFormed.
        if (isRealBinding(it.value()) && !it.key().value.isEmpty()) {
            controlsByChord[it.key().value].append(describe(it.value()));
        }
    }

    QList<Collision> collisions;
    for (auto it = controlsByChord.begin(); it != controlsByChord.end(); ++it) {
        if (it.value().size() > 1) {
            it.value().sort();
            collisions.append(Collision{it.key(), it.value()});
        }
    }
    std::sort(collisions.begin(),
            collisions.end(),
            [](const Collision& lhs, const Collision& rhs) {
                return lhs.controls.join(QLatin1Char('+')) <
                        rhs.controls.join(QLatin1Char('+'));
            });
    return collisions;
}

} // namespace

class KeyboardBindingsTest : public MixxxTest {};

/// Sanity: all 12 mappings are present and the production loader gets something
/// out of each. If this fails, every other assertion below is meaningless.
TEST_F(KeyboardBindingsTest, EveryShippedLocaleFileLoads) {
    ASSERT_EQ(12, kShippedLocales.size());
    for (const QString& locale : kShippedLocales) {
        const auto pConfig = loadKeyboardMapping(locale);
        EXPECT_FALSE(boundControls(*pConfig).isEmpty())
                << locale.toStdString() << ".kbd.cfg parsed to zero bindings";
    }
}

/// Every binding names a syntactically valid ConfigKey and resolves to a
/// non-empty key sequence. Catches truncated lines and group headers that lost
/// their brackets during a merge.
TEST_F(KeyboardBindingsTest, EveryBindingIsWellFormed) {
    for (const QString& locale : kShippedLocales) {
        const auto pConfig = loadKeyboardMapping(locale);
        const QSet<ConfigKey> keys = boundControls(*pConfig);
        for (const ConfigKey& key : keys) {
            EXPECT_TRUE(key.isValid())
                    << locale.toStdString() << ": invalid ConfigKey "
                    << qPrintable(describe(key));
            EXPECT_TRUE(key.group.startsWith(QLatin1Char('[')) &&
                    key.group.endsWith(QLatin1Char(']')))
                    << locale.toStdString() << ": malformed group in "
                    << qPrintable(describe(key));
            // ConfigValueKbd stores QKeySequence::toString() of the parsed
            // chord, so an empty string means Qt could not parse the chord and
            // the binding is dead at runtime.
            const QString signature =
                    QStringLiteral("%1|%2").arg(locale, describe(key));
            if (kKnownUnparseableChords.contains(signature)) {
                continue;
            }
            EXPECT_FALSE(pConfig->getValueString(key).isEmpty())
                    << locale.toStdString() << ": " << qPrintable(describe(key))
                    << " does not parse to a usable key sequence. If this is a "
                       "pre-existing upstream breakage, add \""
                    << qPrintable(signature)
                    << "\" to kKnownUnparseableChords with a comment.";
        }
    }
}

/// THE ONE THAT MATTERS. Every accessibility control bound in en_US must be
/// bound in all 12 locale files. Catches the partial-locale regression where a
/// rebase keeps the en_US accessibility block but drops it from, say, ru_RU.
TEST_F(KeyboardBindingsTest, AccessibilityBindingsPresentInEveryLocale) {
    const QList<ConfigKey> expected = forkAccessibilityBindings();
    ASSERT_EQ(30, expected.size()) << "Accessibility binding list changed size; "
                                     "update it deliberately, do not shrink it "
                                     "to make a red test go green.";

    for (const QString& locale : kShippedLocales) {
        const auto pConfig = loadKeyboardMapping(locale);
        for (const ConfigKey& key : expected) {
            EXPECT_TRUE(pConfig->exists(key))
                    << "res/keyboard/" << locale.toStdString()
                    << ".kbd.cfg has lost the accessibility binding for "
                    << qPrintable(describe(key))
                    << ". A blind user on this locale can no longer reach it.";
        }
    }
}

/// The accessibility bindings are deliberately NOT localized -- the block is
/// copied verbatim into every locale file. So if en_US says one thing and
/// de_DE says another, somebody edited one file and forgot the other eleven.
///
/// Note this compares locales against each other rather than against a
/// hard-coded chord, so a PR that intentionally moves a chord (e.g. off
/// Ctrl+Alt for macOS VoiceOver) passes as long as it moves it everywhere --
/// which is exactly the property worth enforcing.
TEST_F(KeyboardBindingsTest, AccessibilityChordsAreIdenticalAcrossLocales) {
    const auto pReference = loadKeyboardMapping(kReferenceLocale);
    const QList<ConfigKey> expected = forkAccessibilityBindings();

    for (const QString& locale : kShippedLocales) {
        if (locale == kReferenceLocale) {
            continue;
        }
        const auto pConfig = loadKeyboardMapping(locale);
        for (const ConfigKey& key : expected) {
            EXPECT_QSTRING_EQ(pReference->getValueString(key), pConfig->getValueString(key))
                    << " for " << qPrintable(describe(key)) << " in "
                    << locale.toStdString()
                    << ".kbd.cfg (accessibility chords are not localized; "
                       "change them in all 12 files or none)";
        }
    }
}

/// Broader parity: every control bound in en_US -- accessibility or upstream --
/// is bound in every other locale. Locales may bind *extra* controls (el_GR
/// binds waveform_zoom_*), so this is a subset check, not equality.
///
/// This is a superset of AccessibilityBindingsPresentInEveryLocale. It is kept
/// separate because it can go red on a purely upstream inconsistency, and in
/// that case we want to be able to see at a glance whether the accessibility
/// bindings are still fine.
TEST_F(KeyboardBindingsTest, EveryReferenceControlIsBoundInEveryLocale) {
    const auto pReference = loadKeyboardMapping(kReferenceLocale);
    const QSet<ConfigKey> referenceKeys = boundControls(*pReference);

    for (const QString& locale : kShippedLocales) {
        if (locale == kReferenceLocale) {
            continue;
        }
        const auto pConfig = loadKeyboardMapping(locale);
        const QSet<ConfigKey> keys = boundControls(*pConfig);
        for (const ConfigKey& key : referenceKeys) {
            EXPECT_TRUE(keys.contains(key))
                    << "res/keyboard/" << locale.toStdString() << ".kbd.cfg is missing "
                    << qPrintable(describe(key)) << ", which en_US.kbd.cfg binds";
        }
    }
}

/// No two controls share a chord, apart from the explicitly allowlisted
/// collisions we inherited from upstream.
TEST_F(KeyboardBindingsTest, NoUnexpectedChordCollisions) {
    for (const QString& locale : kShippedLocales) {
        const auto pConfig = loadKeyboardMapping(locale);
        const QList<Collision> collisions = findCollisions(*pConfig);
        for (const Collision& collision : collisions) {
            EXPECT_TRUE(kKnownUpstreamCollisions.contains(collision.signature(locale)))
                    << "res/keyboard/" << locale.toStdString() << ".kbd.cfg binds "
                    << qPrintable(collision.chord) << " to "
                    << qPrintable(collision.controls.join(QStringLiteral(" and ")))
                    << ". If this is a pre-existing upstream collision, add \""
                    << qPrintable(collision.signature(locale))
                    << "\" to kKnownUpstreamCollisions with a comment.";
        }
    }
}

/// Stricter, unconditional version for the bindings we own: no accessibility
/// chord may collide with anything, ever. There is no allowlist here -- a
/// shadowed accessibility shortcut is a silent, unreportable failure for a
/// blind user, so it must always be a hard failure.
TEST_F(KeyboardBindingsTest, NoAccessibilityChordCollides) {
    const QList<ConfigKey> accessibilityKeys = forkAccessibilityBindings();
    const QSet<ConfigKey> accessibilitySet(accessibilityKeys.cbegin(), accessibilityKeys.cend());

    for (const QString& locale : kShippedLocales) {
        const auto pConfig = loadKeyboardMapping(locale);
        const QList<Collision> collisions = findCollisions(*pConfig);
        for (const Collision& collision : collisions) {
            bool touchesAccessibility = false;
            for (const ConfigKey& key : accessibilitySet) {
                if (collision.controls.contains(describe(key))) {
                    touchesAccessibility = true;
                    break;
                }
            }
            EXPECT_FALSE(touchesAccessibility)
                    << "res/keyboard/" << locale.toStdString()
                    << ".kbd.cfg shadows an accessibility binding: "
                    << qPrintable(collision.chord) << " is bound to "
                    << qPrintable(collision.controls.join(QStringLiteral(" and ")));
        }
    }
}

/// Engine-backed spot-check that bindings point at controls that really exist.
/// BaseSignalPathTest stands up a real EngineMixer with real decks, so the
/// [Master] and [ChannelN] controls below are genuinely registered here.
class KeyboardBindingsEngineTest : public BaseSignalPathTest {};

TEST_F(KeyboardBindingsEngineTest, EngineBackedAccessibilityBindingsResolveToControls) {
    const auto pConfig = loadKeyboardMapping(kReferenceLocale);
    for (const ConfigKey& key : engineBackedAccessibilityBindings()) {
        EXPECT_TRUE(pConfig->exists(key))
                << "en_US.kbd.cfg no longer binds " << qPrintable(describe(key));
        EXPECT_TRUE(ControlObject::exists(key))
                << qPrintable(describe(key))
                << " is bound in en_US.kbd.cfg but no such ControlObject exists. "
                   "KeyboardEventFilter would only report this via qDebug(), and "
                   "only if the key were pressed.";
    }
}

/// The comment-artifact and menu-shortcut exclusions above are load-bearing;
/// if upstream ever gains real comment support or renames the menu group, the
/// exclusions become silent holes in the checks. Pin the assumptions.
TEST_F(KeyboardBindingsTest, ParserAssumptionsStillHold) {
    const auto pConfig = loadKeyboardMapping(kReferenceLocale);
    // ConfigObject::parse() keeps the brackets when it records a group.
    EXPECT_TRUE(pConfig->exists(ConfigKey(kMenuShortcutGroup, QStringLiteral("FileMenu_Quit"))))
            << "Menu shortcut group is no longer " << qPrintable(kMenuShortcutGroup)
            << "; the ControlObject-resolution exclusions need revisiting.";
    // "// DEVELOPER MENU" is still parsed as a binding rather than a comment.
    EXPECT_TRUE(pConfig->exists(ConfigKey(kMenuShortcutGroup, kCommentArtifactItem)))
            << "ConfigObject appears to have gained comment support; the "
               "kCommentArtifactItem workaround can be removed.";
}

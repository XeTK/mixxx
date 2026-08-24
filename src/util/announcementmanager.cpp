#include "util/announcementmanager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "control/controlpushbutton.h"
#include "engine/engineearcon.h"
#include "engine/enginetts.h"
#include "library/library.h"
#include "library/library_decl.h"
#include "library/trackmodel.h"
#include "mixer/basetrackplayer.h"
#include "mixer/playermanager.h"
#include "moc_announcementmanager.cpp"
// For kConfigKeySmartCue/kDefaultSmartCue: smart cue is a general
// deck-loading behavior configured in Deck preferences, not an
// accessibility-specific setting, so it isn't in AccessibilitySettings.
#include "preferences/dialog/dlgprefdeck.h"
#include "proto/keys.pb.h"
#include "track/beats.h"
#include "track/keyutils.h"
#include "track/track.h"
#include "util/cmdlineargs.h"
#include "util/parented_ptr.h"
#include "util/ttsengine.h"
#include "vinylcontrol/defs_vinylcontrol.h"

namespace {
constexpr int kSelectionDebounceMs = 400;
constexpr int kSearchDebounceMs = 600;
constexpr int kControlDebounceMs = 400;
// Sort column/order changes are debounced so a single toggle that updates
// both controls (a new column resets the order to ascending) collapses into
// one announcement.
constexpr int kSortDebounceMs = 400;
// Minimum gap between spoken updates in announce-while-moving mode.
constexpr qint64 kMovingThrottleMs = 300;
// How long a knob/fader keeps its spoken "context": while the same control
// keeps moving within this window, only the new value is spoken ("a half"
// instead of "A volume a half"), and an unchanged value is suppressed
// entirely — a worn pot jittering around its resting point would otherwise
// repeat the same readout forever.
constexpr qint64 kControlContextMs = 8000;
// Track load/unload rewrites every hotcue status CO; suppress hotcue
// announcements for this long afterwards so a load doesn't fire a burst
// of "hotcue set" messages.
constexpr qint64 kHotcueSuppressMs = 1000;
// Hotcues 1..8 cover the pads on entry-level controllers.
constexpr int kNumAnnouncedHotcues = 8;
// Minimum gap between clipping warnings so sustained clipping doesn't repeat
// the announcement on every ~500 ms peak-indicator cycle.
constexpr qint64 kClippingThrottleMs = 5000;
// sync_enabled is LongPressLatching: it flips to 1 on press and reverts on a
// release within ControlPushButtonBehavior::kLongPressLatchingTimeMillis
// (300 ms). Probe slightly after that window to tell a latched hold from a
// one-shot beat-sync press.
constexpr int kSyncLatchProbeMs = 450;

double readGroupControl(const QString& group, const QString& name) {
    return ControlProxy(group, name, nullptr, ControlFlag::AllowMissingOrInvalid).get();
}

// "2 minutes 10 seconds remaining", with correct singulars.
QString remainingText(int totalSeconds) {
    const int seconds = std::max(0, totalSeconds);
    const int minutes = seconds / 60;
    const int secondsPart = seconds % 60;
    const QString minuteText = minutes == 1
            ? AnnouncementManager::tr("1 minute")
            : AnnouncementManager::tr("%1 minutes").arg(minutes);
    const QString secondText = secondsPart == 1
            ? AnnouncementManager::tr("1 second")
            : AnnouncementManager::tr("%1 seconds").arg(secondsPart);
    if (minutes > 0) {
        return AnnouncementManager::tr("%1 %2 remaining").arg(minuteText, secondText);
    }
    return AnnouncementManager::tr("%1 remaining").arg(secondText);
}

// Fader/knob travel spoken as a fraction ("three quarters") or a percentage
// ("75 percent") depending on the user's MixerReadoutStyle preference.
// Fractions match how DJs think of physical controls and read faster;
// percentages give exact values for users who want them. Fractions are
// snapped to the user's MixerFractionDetail denominator (4, 8, or 16 —
// sixteenths proved too fine by ear) and simplified.
QString fractionText(double zeroToOne, bool asPercent, int denominator) {
    if (asPercent) {
        const int percent = static_cast<int>(
                std::lround(std::clamp(zeroToOne, 0.0, 1.0) * 100));
        return AnnouncementManager::tr("%1 percent").arg(percent);
    }
    // Snap at the requested detail, then express in sixteenths so the naming
    // below simplifies the result ("a quarter", "3 eighths") the same way at
    // every detail level.
    const int snapped = std::clamp(
            static_cast<int>(std::lround(zeroToOne * denominator)),
            0,
            denominator);
    const int sixteenths = snapped * (16 / denominator);
    switch (sixteenths) {
    case 0:
        return AnnouncementManager::tr("zero");
    case 4:
        return AnnouncementManager::tr("a quarter");
    case 8:
        return AnnouncementManager::tr("a half");
    case 12:
        return AnnouncementManager::tr("three quarters");
    case 16:
        return AnnouncementManager::tr("full");
    default:
        break;
    }
    if (sixteenths % 2 == 0) {
        const int eighths = sixteenths / 2;
        return eighths == 1
                ? AnnouncementManager::tr("an eighth")
                : AnnouncementManager::tr("%1 eighths").arg(eighths);
    }
    return sixteenths == 1
            ? AnnouncementManager::tr("a sixteenth")
            : AnnouncementManager::tr("%1 sixteenths").arg(sixteenths);
}

// Center-detented controls (EQ, filter, gain): deviation from center as a
// signed fraction or percentage — "center", "plus a quarter", "minus 25
// percent", per MixerReadoutStyle.
QString centerSplitText(double normalized /* -1 .. +1, 0 = center */,
        bool asPercent,
        int denominator) {
    const double magnitude = std::clamp(std::abs(normalized), 0.0, 1.0);
    const bool isCenter = asPercent
            ? std::lround(magnitude * 100) == 0
            : std::lround(magnitude * denominator) == 0;
    if (isCenter) {
        return AnnouncementManager::tr("center");
    }
    return normalized > 0
            ? AnnouncementManager::tr("plus %1").arg(
                      fractionText(magnitude, asPercent, denominator))
            : AnnouncementManager::tr("minus %1")
                      .arg(fractionText(magnitude, asPercent, denominator));
}

// Normalize a unity-1 gain knob (range 0..4, center 1 — EQ knobs and the
// main/headphone gain tapers) to -1..+1 for centerSplitText.
double normalizeUnityGain(double value) {
    return value <= 1.0 ? value - 1.0 : (value - 1.0) / 3.0;
}

// Spoken pitch-fader deviation, e.g. "Pitch up 2 percent". Empty at exactly
// normal speed. Words instead of a sign because TTS engines don't read "+"
// reliably.
QString pitchText(double rateRatio) {
    if (rateRatio <= 0.0 || std::abs(rateRatio - 1.0) < 0.0005) {
        return {};
    }
    const double percent = std::abs(rateRatio - 1.0) * 100.0;
    QString percentText = QString::number(percent, 'f', 1);
    if (percentText.endsWith(QStringLiteral(".0"))) {
        percentText.chop(2);
    }
    return rateRatio > 1.0
            ? AnnouncementManager::tr("Pitch up %1 percent").arg(percentText)
            : AnnouncementManager::tr("Pitch down %1 percent").arg(percentText);
}

// The same deviation without the leading "Pitch" word, for the fader
// readout where the control is already named ("Deck 1 pitch" on touch,
// "up 2 percent" at rest). Empty at exactly normal speed.
QString pitchDeviationText(double rateRatio) {
    if (rateRatio <= 0.0 || std::abs(rateRatio - 1.0) < 0.0005) {
        return {};
    }
    const double percent = std::abs(rateRatio - 1.0) * 100.0;
    QString percentText = QString::number(percent, 'f', 1);
    if (percentText.endsWith(QStringLiteral(".0"))) {
        percentText.chop(2);
    }
    return rateRatio > 1.0
            ? AnnouncementManager::tr("up %1 percent").arg(percentText)
            : AnnouncementManager::tr("down %1 percent").arg(percentText);
}

// Returns a fully-spelled pronounceable key name for the given ChromaticKey,
// e.g. A_MINOR → "A Minor", F#_MAJOR → "F Sharp Major".
// Using a lookup table keyed by the enum integer (INVALID=0, C_MAJOR=1 … B_MINOR=24).
// Each name is wrapped in tr() so the spoken key respects the app locale.
QString keyForSpeech(mixxx::track::io::key::ChromaticKey key) {
    using namespace mixxx::track::io::key;
    static const QString kNames[] = {
            QString(),                       // 0  INVALID
            AnnouncementManager::tr("C Major"),                  // 1
            AnnouncementManager::tr("D Flat Major"),             // 2
            AnnouncementManager::tr("D Major"),                  // 3
            AnnouncementManager::tr("E Flat Major"),             // 4
            AnnouncementManager::tr("E Major"),                  // 5
            AnnouncementManager::tr("F Major"),                  // 6
            AnnouncementManager::tr("F Sharp Major"),            // 7
            AnnouncementManager::tr("G Major"),                  // 8
            AnnouncementManager::tr("A Flat Major"),             // 9
            AnnouncementManager::tr("A Major"),                  // 10
            AnnouncementManager::tr("B Flat Major"),             // 11
            AnnouncementManager::tr("B Major"),                  // 12
            AnnouncementManager::tr("C Minor"),                  // 13
            AnnouncementManager::tr("C Sharp Minor"),            // 14
            AnnouncementManager::tr("D Minor"),                  // 15
            AnnouncementManager::tr("E Flat Minor"),             // 16
            AnnouncementManager::tr("E Minor"),                  // 17
            AnnouncementManager::tr("F Minor"),                  // 18
            AnnouncementManager::tr("F Sharp Minor"),            // 19
            AnnouncementManager::tr("G Minor"),                  // 20
            AnnouncementManager::tr("A Flat Minor"),             // 21
            AnnouncementManager::tr("A Minor"),                  // 22
            AnnouncementManager::tr("B Flat Minor"),             // 23
            AnnouncementManager::tr("B Minor"),                  // 24
    };
    const int idx = static_cast<int>(key);
    if (idx < 0 || idx >= static_cast<int>(std::size(kNames))) {
        return {};
    }
    return kNames[idx];
}

// Spells out a deck letter so it is pronounced as a letter name rather than
// misread as a word — a bare "A" in particular is also the indefinite
// article, so TTS engines often read it with its unstressed "uh"
// pronunciation instead of the letter name.
//
// Uses the NATO/ICAO phonetic alphabet rather than short single-syllable
// English words (an earlier version used "Ay", "Bee", "See", …). "Ay" for A
// turned out to be a real homograph: it's also the vote/nautical interjection
// ("the ayes have it"), and macOS's speech synthesis resolves it to the same
// pronunciation as "eye"/"I" — confirmed by rendering "Ay", "Aye", and "eye"
// and diffing the audio, which was byte-for-byte identical for all three.
// Rather than track down whether any of the other 25 short words have a
// similar hidden ambiguity on some engine/voice, every letter now uses the
// NATO word, which exists specifically to be unambiguous over a voice
// channel. Confirmed no two NATO words collide by rendering and diffing all
// 26. Costs a syllable or two of brevity per letter; correctness matters
// more for an accessibility feature meant to be trusted at face value.
QString phoneticLetter(QChar letter) {
    static const QString kNames[] = {
            QStringLiteral("Alpha"),    // A
            QStringLiteral("Bravo"),    // B
            QStringLiteral("Charlie"),  // C
            QStringLiteral("Delta"),    // D
            QStringLiteral("Echo"),     // E
            QStringLiteral("Foxtrot"),  // F
            QStringLiteral("Golf"),     // G
            QStringLiteral("Hotel"),    // H
            QStringLiteral("India"),    // I
            QStringLiteral("Juliet"),   // J
            QStringLiteral("Kilo"),     // K
            QStringLiteral("Lima"),     // L
            QStringLiteral("Mike"),     // M
            QStringLiteral("November"), // N
            QStringLiteral("Oscar"),    // O
            QStringLiteral("Papa"),     // P
            QStringLiteral("Quebec"),   // Q
            QStringLiteral("Romeo"),    // R
            QStringLiteral("Sierra"),   // S
            QStringLiteral("Tango"),    // T
            QStringLiteral("Uniform"),  // U
            QStringLiteral("Victor"),   // V
            QStringLiteral("Whiskey"),  // W
            QStringLiteral("X-ray"),    // X
            QStringLiteral("Yankee"),   // Y
            QStringLiteral("Zulu"),     // Z
    };
    const int idx = letter.toUpper().unicode() - u'A';
    if (idx < 0 || idx >= static_cast<int>(std::size(kNames))) {
        return QString(letter);
    }
    return kNames[idx];
}

// Speaks the key in whichever notation the user has chosen under
// Preferences, Interface, Key Notation ([Library],key_notation), so a DJ who
// reads Camelot ("8A") or Open Key ("5d") codes elsewhere in Mixxx hears the
// same code instead of always the full traditional name. Falls back to the
// full name for Traditional/Custom/ID3v2/unset, where a short code isn't
// what's shown on screen anyway.
QString keyForSpeechInNotation(mixxx::track::io::key::ChromaticKey key) {
    if (key == mixxx::track::io::key::INVALID) {
        return {};
    }
    const KeyUtils::KeyNotation notation = KeyUtils::keyNotationFromNumericValue(
            readGroupControl(QStringLiteral("[Library]"), QStringLiteral("key_notation")));
    const bool isOpenKey = notation == KeyUtils::KeyNotation::OpenKey ||
            notation == KeyUtils::KeyNotation::OpenKeyAndTraditional;
    const bool isLancelot = notation == KeyUtils::KeyNotation::Lancelot ||
            notation == KeyUtils::KeyNotation::LancelotAndTraditional;
    if (!isOpenKey && !isLancelot) {
        return keyForSpeech(key);
    }

    // Open Key and Lancelot (Camelot) codes are digits followed by exactly
    // one letter ("5d", "8A"); the letter is spelled out phonetically for
    // the same reason as phoneticLetter() above.
    const QString code = KeyUtils::keyToString(key,
            isOpenKey ? KeyUtils::KeyNotation::OpenKey : KeyUtils::KeyNotation::Lancelot);
    if (code.isEmpty()) {
        return keyForSpeech(key);
    }
    QString spoken = AnnouncementManager::tr("%1, %2")
                             .arg(code.left(code.length() - 1), phoneticLetter(code.back()));
    if (notation == KeyUtils::KeyNotation::OpenKeyAndTraditional ||
            notation == KeyUtils::KeyNotation::LancelotAndTraditional) {
        spoken += QStringLiteral(". ") + keyForSpeech(key);
    }
    return spoken;
}
} // namespace

// static
std::unique_ptr<AnnouncementManager> AnnouncementManager::create(
        Library* pLibrary,
        PlayerManagerInterface* pPlayerManager,
        UserSettingsPointer pConfig,
        EngineTts* pTtsSink,
        EngineEarcon* pEarcon,
        QObject* parent) {
    return std::make_unique<AnnouncementManager>(pLibrary,
            pPlayerManager,
            std::move(pConfig),
            TtsEngine::create(),
            pTtsSink,
            pEarcon,
            parent);
}

AnnouncementManager::AnnouncementManager(
        Library* pLibrary,
        PlayerManagerInterface* pPlayerManager,
        UserSettingsPointer pConfig,
        std::unique_ptr<TtsEngine> pTts,
        EngineTts* pTtsSink,
        EngineEarcon* pEarcon,
        QObject* parent)
        : QObject(parent),
          m_pTts(std::move(pTts)),
          m_pTtsSink(pTtsSink),
          m_pEarcon(pEarcon),
          m_settings(pConfig),
          m_pConfig(pConfig),
          m_pPlayerManager(pPlayerManager) {
    if (m_pTts && m_pTtsSink) {
        m_pTts->setSink(m_pTtsSink);
    }
    if (m_pTtsSink) {
        m_pSampleRate = std::make_unique<ControlProxy>(
                QStringLiteral("[App]"),
                QStringLiteral("samplerate"),
                this,
                ControlFlag::AllowMissingOrInvalid);
        // The engine sink outlives the manager in normal shutdown, but a
        // ControlProxy observing its [Tts],enabled control can still fire
        // speak() after the sink is destroyed (see issue #30). Drop the raw
        // pointer when the sink is torn down so speak() never dereferences it.
        connect(m_pTtsSink,
                &EngineTts::sinkDestroyed,
                this,
                &AnnouncementManager::onTtsSinkDestroyed);
    }
    init(pLibrary, pPlayerManager);
}

void AnnouncementManager::init(Library* pLibrary, PlayerManagerInterface* pPlayerManager) {
    m_selectionDebounce.setSingleShot(true);
    m_selectionDebounce.setInterval(kSelectionDebounceMs);
    connect(&m_selectionDebounce,
            &QTimer::timeout,
            this,
            &AnnouncementManager::slotAnnounceSelectedTrack);

    m_searchDebounce.setSingleShot(true);
    m_searchDebounce.setInterval(kSearchDebounceMs);
    connect(&m_searchDebounce,
            &QTimer::timeout,
            this,
            &AnnouncementManager::slotAnnounceSearch);

    m_controlDebounce.setSingleShot(true);
    m_controlDebounce.setInterval(kControlDebounceMs);
    connect(&m_controlDebounce,
            &QTimer::timeout,
            this,
            &AnnouncementManager::slotAnnouncePendingControl);

    m_sortDebounce.setSingleShot(true);
    m_sortDebounce.setInterval(kSortDebounceMs);
    connect(&m_sortDebounce,
            &QTimer::timeout,
            this,
            &AnnouncementManager::slotAnnounceSort);

    if (pLibrary) {
        connect(pLibrary,
                &Library::trackSelected,
                this,
                &AnnouncementManager::slotTrackSelected);
        connect(pLibrary,
                &Library::sidebarItemActivated,
                this,
                &AnnouncementManager::slotSidebarItemActivated);
        connect(pLibrary,
                &Library::playlistTracksEdited,
                this,
                &AnnouncementManager::slotPlaylistTracksEdited);
        connect(pLibrary,
                &Library::crateTracksEdited,
                this,
                &AnnouncementManager::slotCrateTracksEdited);
        connect(pLibrary,
                &Library::quickPickerItemHighlighted,
                this,
                &AnnouncementManager::slotQuickPickerItemHighlighted);
        connect(pLibrary,
                &Library::search,
                this,
                &AnnouncementManager::slotSearchTextChanged);
        connect(pLibrary,
                &Library::searchResultCountChanged,
                this,
                &AnnouncementManager::slotSearchResultCount);
    }

    // Track-list sort column/order feedback. [Library],sort_column holds the
    // active TrackModel::SortColumnId and [Library],sort_order the direction
    // (0 = ascending, 1 = descending); both are driven by the keyboard
    // binding sort_column_toggle (and by clicking a column header). A blind
    // user toggling the sort column needs to hear which column the library is
    // now sorted by.
    auto pSortColumn = make_parented<ControlProxy>(
            QStringLiteral("[Library]"),
            QStringLiteral("sort_column"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pSortColumn->connectValueChanged(this, [this](double) {
        m_sortDebounce.start();
    });
    auto pSortOrder = make_parented<ControlProxy>(
            QStringLiteral("[Library]"),
            QStringLiteral("sort_order"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pSortOrder->connectValueChanged(this, [this](double) {
        m_sortDebounce.start();
    });

    connect(pPlayerManager,
            &PlayerManagerInterface::numberOfDecksChanged,
            this,
            &AnnouncementManager::slotNumberOfDecksChanged);

    const int numDecks = pPlayerManager->numberOfDecks();
    for (int i = 0; i < numDecks; ++i) {
        connectDeck(i);
    }
    m_connectedDecks = numDecks;

    auto pFocusedWidget = make_parented<ControlProxy>(
            QStringLiteral("[Library]"),
            QStringLiteral("focused_widget"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pFocusedWidget->connectValueChanged(this, &AnnouncementManager::slotLibraryFocusChanged);

    // Confirm re-enabling TTS audibly, otherwise the toggle (menu item or
    // Alt+Shift+A) gives a blind user no feedback that it worked. Only the
    // on-transition can be spoken; turning TTS off flushes the speech FIFO.
    auto pTtsEnabled = make_parented<ControlProxy>(
            QStringLiteral("[Tts]"),
            QStringLiteral("enabled"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pTtsEnabled->connectValueChanged(this, [this](double value) {
        if (value > 0.0) {
            speak(tr("Speech on"));
        }
    });

    // Recording state: silence here would mean a lost set, so announce both
    // transitions. [Recording],status is 0 = off, 1 = ready, 2 = recording.
    auto pRecording = make_parented<ControlProxy>(
            QStringLiteral("[Recording]"),
            QStringLiteral("status"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pRecording->connectValueChanged(this,
            [this, wasRecording = false](double value) mutable {
                const bool nowRecording = value >= 2.0;
                if (nowRecording == wasRecording ||
                        !m_settings.getAnnounceRecording()) {
                    wasRecording = nowRecording;
                    return;
                }
                wasRecording = nowRecording;
                speak(nowRecording ? tr("Recording started")
                                   : tr("Recording stopped"));
            });

    // Main output clipping. [Main],peak_indicator (alias [Master],
    // PeakIndicator) pulses to 1.0 for ~500 ms after a clipped peak; treat
    // each pulse as a rising edge and throttle so sustained clipping doesn't
    // repeat the warning constantly. Center-panned: this is a whole-mix
    // problem, not a single deck's.
    auto pClipping = make_parented<ControlProxy>(
            QStringLiteral("[Main]"),
            QStringLiteral("peak_indicator"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pClipping->connectValueChanged(this, [this](double value) {
        if (value <= 0.0 || !m_settings.getAnnounceClipping()) {
            return;
        }
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastClippingAnnounceMs < kClippingThrottleMs) {
            return;
        }
        m_lastClippingAnnounceMs = now;
        emitCue(static_cast<int>(EngineEarcon::Id::Clipping), -1, tr("Clipping"));
    });

    // Crossfader lock (Alt+X): always confirmed audibly — it is a direct user
    // action, and silently locking the crossfader would be baffling.
    auto pCrossfaderLock = make_parented<ControlProxy>(
            QStringLiteral("[Master]"),
            QStringLiteral("crossfader_lock"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pCrossfaderLock->connectValueChanged(this, [this](double value) {
        speak(value > 0.0 ? tr("Crossfader locked") : tr("Crossfader unlocked"));
    });

    // Crossfader position, debounced while it moves. -1 is full left, +1 full
    // right; treat the middle five percent as center. While the lock is on the
    // engine ignores the control, so a position readout would describe a fader
    // that isn't actually doing anything — stay quiet instead.
    auto pCrossfader = make_parented<ControlProxy>(
            QStringLiteral("[Master]"),
            QStringLiteral("crossfader"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pCrossfader->connectValueChanged(this,
            [this, pLock = static_cast<ControlProxy*>(pCrossfaderLock)](double value) {
                if (!m_settings.getAnnounceMixer() || pLock->toBool()) {
                    return;
                }
                QString valueText;
                if (std::abs(value) < 0.05) {
                    valueText = tr("center");
                } else {
                    const bool asPercent = mixerReadoutAsPercent();
                    const int detail = mixerFractionDenominator();
                    valueText = value < 0
                            ? tr("left %1").arg(fractionText(-value, asPercent, detail))
                            : tr("right %1").arg(fractionText(value, asPercent, detail));
                }
                announceControlDebounced(QStringLiteral("[Master]crossfader"),
                        tr("Crossfader"),
                        valueText);
            });

    // Headphone mix knob: -1 is full cue (PFL'd decks only), +1 is full main
    // (audience mix), debounced while it moves. A linear ControlPotmeter, so
    // the raw value is already the -1..1 position — no taper conversion
    // needed here (unlike the volume/trim/gain knobs above). "Cue"/"main"
    // wording rather than centerSplitText's generic plus/minus, since a
    // signed fraction alone would not say which side (cue vs. main) it leans
    // toward.
    auto pHeadMix = make_parented<ControlProxy>(
            QStringLiteral("[Master]"),
            QStringLiteral("headMix"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pHeadMix->connectValueChanged(this, [this](double value) {
        if (!m_settings.getAnnounceMixer()) {
            return;
        }
        QString valueText;
        if (std::abs(value) < 0.05) {
            valueText = tr("even");
        } else {
            const bool asPercent = mixerReadoutAsPercent();
            const int detail = mixerFractionDenominator();
            valueText = value < 0
                    ? tr("cue %1").arg(fractionText(-value, asPercent, detail))
                    : tr("main %1").arg(fractionText(value, asPercent, detail));
        }
        announceControlDebounced(QStringLiteral("[Master]headMix"),
                tr("Headphone mix"),
                valueText);
    });

    // Main and headphone volume knobs, debounced. Both are ControlAudioTaperPot
    // with neutral parameter 0.5 (center of the knob = unity), so read the
    // knob position via getParameter(), not the dB-tapered gain value. Spoken
    // as plain knob travel ("a half", "three quarters") — a center-split
    // readout ("minus a quarter") made testers think the volume itself had
    // gone negative.
    const struct {
        const char* control;
        QString name;
    } masterGains[] = {
            {"gain", tr("Main volume")},
            {"headGain", tr("Headphone volume")},
    };
    for (const auto& gain : masterGains) {
        auto pGain = make_parented<ControlProxy>(
                QStringLiteral("[Master]"),
                QLatin1String(gain.control),
                this,
                ControlFlag::AllowMissingOrInvalid);
        pGain->connectValueChanged(this,
                [this,
                        name = gain.name,
                        // The explicit QString(...) forces eager evaluation.
                        // With QT_USE_QSTRINGBUILDER, operator+ here returns a
                        // QStringBuilder that stores a reference to its
                        // operands; without the cast, `key`'s auto-deduced
                        // type would keep that dangling reference alive past
                        // this statement, corrupting memory whenever the
                        // lambda is later invoked.
                        key = QString(QStringLiteral("[Master]") + QLatin1String(gain.control)),
                        pGainRaw = static_cast<ControlProxy*>(pGain)](double) {
                    if (!m_settings.getAnnounceMixer()) {
                        return;
                    }
                    announceControlDebounced(key,
                            name,
                            fractionText(
                                    std::clamp(pGainRaw->getParameter(), 0.0, 1.0),
                                    mixerReadoutAsPercent(),
                                    mixerFractionDenominator()));
                });
    }

    // Effect unit knobs: the dry/wet mix and the super knob for the four
    // standard units, debounced under AnnounceMixer.
    for (int unit = 1; unit <= 4; ++unit) {
        const QString unitGroup =
                QStringLiteral("[EffectRack1_EffectUnit%1]").arg(unit);
        const struct {
            const char* control;
            QString text;
        } knobs[] = {
                {"mix", tr("Effect %1 mix")},
                {"super1", tr("Effect %1 super")},
        };
        for (const auto& knob : knobs) {
            auto pKnob = make_parented<ControlProxy>(unitGroup,
                    QLatin1String(knob.control),
                    this,
                    ControlFlag::AllowMissingOrInvalid);
            pKnob->connectValueChanged(this,
                    [this,
                            name = knob.text.arg(unit),
                            // See the comment on the equivalent capture above
                            // (near "[Master]" + gain.control): without the
                            // QString(...) cast, QT_USE_QSTRINGBUILDER makes
                            // this a dangling reference to unitGroup once the
                            // loop iteration ends.
                            key = QString(unitGroup + QLatin1String(knob.control))](
                            double value) {
                        if (!m_settings.getAnnounceMixer()) {
                            return;
                        }
                        announceControlDebounced(key,
                                name,
                                fractionText(
                                        std::clamp(value, 0.0, 1.0),
                                        mixerReadoutAsPercent(),
                                        mixerFractionDenominator()));
                    });
        }
    }

    // Per-deck split cue toggle (Alt+H): always confirmed audibly.
    auto pHeadSplitDecks = make_parented<ControlProxy>(
            QStringLiteral("[Master]"),
            QStringLiteral("headSplitDecks"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pHeadSplitDecks->connectValueChanged(this, [this](double value) {
        speak(value > 0.0 ? tr("Split cue on. Deck 1 left, deck 2 right")
                          : tr("Split cue off"));
    });

    // Beat click metronome toggle (Alt+B): always confirmed audibly.
    auto pBeatClick = make_parented<ControlProxy>(
            QStringLiteral("[BeatClick]"),
            QStringLiteral("enabled"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pBeatClick->connectValueChanged(this, [this](double value) {
        speak(value > 0.0 ? tr("Beat click on") : tr("Beat click off"));
    });

    // Jog wheel touch lock (Alt+J): disables click-and-drag scratching on the
    // on-screen waveform and vinyl widgets for both decks, so an accidental
    // touch can't disturb playback. Always confirmed audibly.
    auto pDisableTouchScratch = make_parented<ControlProxy>(
            QStringLiteral("[Master]"),
            QStringLiteral("disable_touch_scratch"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pDisableTouchScratch->connectValueChanged(this, [this](double value) {
        speak(value > 0.0 ? tr("Jog wheel touch locked") : tr("Jog wheel touch unlocked"));
    });

    // Standard effect units: per-effect enable toggles and effect selection.
    // Slot groups are [EffectRack1_EffectUnitU_EffectS]; the loaded_effect CO
    // holds a 1-based index into the visible effects list (0 = empty), so the
    // spoken name comes from the injected resolver when available.
    for (int unit = 1; unit <= 4; ++unit) {
        for (int slot = 1; slot <= 4; ++slot) {
            const QString slotGroup =
                    QStringLiteral("[EffectRack1_EffectUnit%1_Effect%2]")
                            .arg(unit)
                            .arg(slot);
            auto pEnabled = make_parented<ControlProxy>(slotGroup,
                    QStringLiteral("enabled"),
                    this,
                    ControlFlag::AllowMissingOrInvalid);
            pEnabled->connectValueChanged(this, [this, unit, slot](double value) {
                if (!m_settings.getAnnounceEffects()) {
                    return;
                }
                QString name = m_effectNameResolver
                        ? m_effectNameResolver(unit, slot)
                        : QString();
                if (name.isEmpty()) {
                    name = tr("effect %1").arg(slot);
                }
                speak((value > 0.0 ? tr("Unit %1 %2 on") : tr("Unit %1 %2 off"))
                                .arg(QString::number(unit), name));
            });

            auto pLoaded = make_parented<ControlProxy>(slotGroup,
                    QStringLiteral("loaded_effect"),
                    this,
                    ControlFlag::AllowMissingOrInvalid);
            pLoaded->connectValueChanged(this, [this, unit, slot](double value) {
                if (!m_settings.getAnnounceEffects()) {
                    return;
                }
                if (value <= 0.0) {
                    announceControlDebounced(
                            tr("Unit %1 effect %2 cleared").arg(unit).arg(slot));
                    return;
                }
                QString name = m_effectNameResolver
                        ? m_effectNameResolver(unit, slot)
                        : QString();
                if (name.isEmpty()) {
                    name = tr("effect %1").arg(static_cast<int>(value));
                }
                // Debounced: the effect selector knob can tick through
                // several effects per second.
                announceControlDebounced(
                        tr("Unit %1: %2 loaded").arg(QString::number(unit), name));
            });
        }
    }

    // Which effect slot within a unit is "focused" (the DDJ-400's BEAT FX
    // </> paddles move this via changeFocusedEffectBy()). The CO holds a
    // 1-based slot index; moving focus does not load/unload an effect, so
    // the loaded_effect observer above never fires for it.
    for (int unit = 1; unit <= 4; ++unit) {
        auto pFocused = make_parented<ControlProxy>(
                QStringLiteral("[EffectRack1_EffectUnit%1]").arg(unit),
                QStringLiteral("focused_effect"),
                this,
                ControlFlag::AllowMissingOrInvalid);
        pFocused->connectValueChanged(this, [this, unit](double value) {
            if (!m_settings.getAnnounceEffects() || value <= 0.0) {
                return;
            }
            const int slot = static_cast<int>(value);
            QString name = m_effectNameResolver ? m_effectNameResolver(unit, slot)
                                                 : QString();
            if (name.isEmpty()) {
                name = tr("effect %1").arg(slot);
            }
            // Debounced: the focus paddle can be stepped through several
            // slots per second.
            announceControlDebounced(tr("Unit %1: %2 focused").arg(QString::number(unit), name));
        });
    }

    // Repeat the last announcement on demand (mapped to Alt+Shift+R). A blind
    // user who missed an announcement can re-hear it instead of guessing.
    // Trigger mode so each keypress fires even though the value doesn't change.
    auto pRepeat = std::make_unique<ControlPushButton>(
            ConfigKey(QStringLiteral("[Tts]"), QStringLiteral("repeat")));
    pRepeat->setButtonMode(mixxx::control::ButtonMode::Trigger);
    connect(pRepeat.get(),
            &ControlObject::valueChanged,
            this,
            [this](double value) {
                if (value > 0.0 && !m_lastSpoken.isEmpty()) {
                    speak(m_lastSpoken);
                }
            });
    m_pRepeatButton = std::move(pRepeat);

    // Controller feedback hooks. Neither control does anything by itself —
    // a controller mapping drives them, which makes wiring them up the
    // mapping's opt-in.
    //
    // [Tts],shift: set to 1 while the hardware shift button is held.
    // Announced on the press only; narrating the release too would double
    // the chatter for no information.
    auto pShift = std::make_unique<ControlPushButton>(
            ConfigKey(QStringLiteral("[Tts]"), QStringLiteral("shift")));
    connect(pShift.get(),
            &ControlObject::valueChanged,
            this,
            [this](double value) {
                if (value > 0.0) {
                    speak(tr("Shift"));
                }
            });
    m_pShiftControl = std::move(pShift);

    // [Tts],pad_mode: which layer the controller's performance pads are in.
    // Values are a fixed cross-controller vocabulary; a mapping sets the one
    // matching the mode button that was pressed. Setting the same value
    // again is silent (no CO change), which conveniently deduplicates
    // hardware that fires one mode press for both decks at once (Numark
    // Scratch).
    auto pPadMode = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Tts]"), QStringLiteral("pad_mode")));
    connect(pPadMode.get(),
            &ControlObject::valueChanged,
            this,
            [this](double value) {
                QString mode;
                switch (static_cast<int>(value)) {
                case 1:
                    mode = tr("hot cues");
                    break;
                case 2:
                    mode = tr("beat loop");
                    break;
                case 3:
                    mode = tr("beat jump");
                    break;
                case 4:
                    mode = tr("sampler");
                    break;
                case 5:
                    mode = tr("keyboard");
                    break;
                case 6:
                    mode = tr("pad effects 1");
                    break;
                case 7:
                    mode = tr("pad effects 2");
                    break;
                case 8:
                    mode = tr("key shift");
                    break;
                case 9:
                    mode = tr("loop roll");
                    break;
                default:
                    return;
                }
                speak(tr("Pads, %1").arg(mode));
            });
    m_pPadModeControl = std::move(pPadMode);
}

AnnouncementManager::~AnnouncementManager() = default;

void AnnouncementManager::onTtsSinkDestroyed() {
    // The engine sink (and its [Tts],enabled control) is being destroyed. Drop
    // the raw pointer so speak() bails instead of dereferencing freed memory,
    // and clear the TtsEngine's own sink pointer so its say() path can't reach
    // the destroyed sink either. The ControlProxy observing [Tts],enabled is
    // parented to this object and will be destroyed with it; until then its
    // valueChanged lambda must not reach into the sink.
    m_ttsSinkDestroyed = true;
    m_pTtsSink = nullptr;
    if (m_pTts) {
        m_pTts->setSink(nullptr);
    }
}

void AnnouncementManager::speak(const QString& text) {
    // Any announcement invalidates the knob/fader name-once context: after an
    // unrelated utterance the next control move must name the control again.
    // slotAnnouncePendingControl() restores the context after its own speak().
    m_lastControlKey.clear();

    // Test hook (--tts-log): append every spoken string to a file so automated
    // accessibility tests can assert on what was spoken. Log before the
    // TTS-disabled early return so the hook captures all utterances.
    const QString ttsLogPath = CmdlineArgs::Instance().getTtsLogPath();
    if (!ttsLogPath.isEmpty()) {
        QFile logFile(ttsLogPath);
        if (logFile.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&logFile);
            out << text << "\n";
        }
    }

    // Skip if TTS is disabled via the user toggle.
    if (m_pTtsSink && !m_pTtsSink->isUserEnabled()) {
        return;
    }

    // The engine sink has been destroyed (shutdown). There is nowhere to render
    // the speech and the TtsEngine's sink pointer has been cleared, so bail
    // rather than synthesize into a torn-down sink (issue #30).
    if (m_ttsSinkDestroyed) {
        return;
    }

    const QString voiceId = m_settings.getTtsVoice();
    if (voiceId != m_currentTtsVoiceId) {
        m_pTts->setVoice(voiceId);
        m_currentTtsVoiceId = voiceId;
    }
    const int rate = m_settings.getTtsRate();
    if (rate != m_currentTtsRate) {
        m_pTts->setRate(rate);
        m_currentTtsRate = rate;
    }
    // The synthesizer renders into the engine sink, which mixes speech into the
    // selected output bus (headphone or main) with ducking. Keep the sink's
    // routing and the render sample rate in sync with the engine and settings.
    // Sample rate must be set before synthesizing (fallback to 44.1 kHz).
    int sampleRate = 44100;
    if (m_pSampleRate) {
        int engineRate = static_cast<int>(m_pSampleRate->get());
        if (engineRate > 0) {
            sampleRate = engineRate;
        }
    }
    m_pTts->setSampleRate(sampleRate);

    if (m_pTtsSink) {
        const int route = m_settings.getTtsRoute();
        if (route != m_currentTtsRoute) {
            m_pTtsSink->setRoute(route);
            m_currentTtsRoute = route;
        }
    }
    m_lastSpoken = text;
    m_pTts->say(text);
}

void AnnouncementManager::emitCue(int earconId, int deckIndex, const QString& speechText) {
    // Per-event feedback mode: 0 = speech, 1 = sounds, 2 = both.
    int mode = 2;
    switch (static_cast<EngineEarcon::Id>(earconId)) {
    case EngineEarcon::Id::Play:
        mode = m_settings.getFeedbackModePlay();
        break;
    case EngineEarcon::Id::Stop:
        mode = m_settings.getFeedbackModeStop();
        break;
    case EngineEarcon::Id::EndOfTrack:
        mode = m_settings.getFeedbackModeEndOfTrack();
        break;
    case EngineEarcon::Id::CueOn:
    case EngineEarcon::Id::CueOff:
    case EngineEarcon::Id::CuePreview:
        mode = m_settings.getFeedbackModeCue();
        break;
    case EngineEarcon::Id::Restart:
        mode = m_settings.getFeedbackModeRestart();
        break;
    case EngineEarcon::Id::LoopOn:
    case EngineEarcon::Id::LoopOff:
        mode = m_settings.getFeedbackModeLoop();
        break;
    case EngineEarcon::Id::Clipping:
        mode = m_settings.getFeedbackModeClipping();
        break;
    }
    if (mode != 1) {
        speak(speechText);
    }
    if (mode != 0 && m_pEarcon) {
        const auto pan = deckIndex == 0 ? EngineEarcon::Pan::Left
                : deckIndex == 1        ? EngineEarcon::Pan::Right
                                        : EngineEarcon::Pan::Center;
        m_pEarcon->trigger(static_cast<EngineEarcon::Id>(earconId), pan);
    }
}

void AnnouncementManager::connectGroupControls(const QString& group, int deckIndex) {
    // On-demand readouts: pressing a mapped key speaks the deck's full status
    // or a single fact. Trigger mode so every press fires. Each is an
    // ordinary control, so they are controller-mappable too.
    const struct {
        const char* control;
        QString (AnnouncementManager::*format)(const QString&, int) const;
    } readouts[] = {
            {"tts_status", &AnnouncementManager::formatDeckStatus},
            {"tts_time", &AnnouncementManager::formatTimeRemaining},
            {"tts_bpm", &AnnouncementManager::formatBpm},
            {"tts_key", &AnnouncementManager::formatKey},
            {"tts_bar", &AnnouncementManager::formatBarPosition},
            {"tts_track", &AnnouncementManager::formatTrackName},
    };
    for (const auto& readout : readouts) {
        auto pButton = std::make_unique<ControlPushButton>(
                ConfigKey(group, QLatin1String(readout.control)));
        pButton->setButtonMode(mixxx::control::ButtonMode::Trigger);
        connect(pButton.get(),
                &ControlObject::valueChanged,
                this,
                [this, group, deckIndex, format = readout.format](double value) {
                    if (value > 0.0) {
                        speak((this->*format)(group, deckIndex));
                    }
                });
        m_pStatusButtons.push_back(std::move(pButton));
    }

    auto pPlay = make_parented<ControlProxy>(group, QStringLiteral("play"), this);
    pPlay->connectValueChanged(this, [this, group, deckIndex](double value) {
        const bool nowPlaying = value > 0.0;
        const bool wasPlaying = m_deckIsPlaying.value(group, false);
        const bool hasTrack = m_deckHasTrack.value(group, false);

        if (nowPlaying && !wasPlaying && hasTrack && m_settings.getAnnouncePlay()) {
            // Holding the transport cue button previews from the cue point;
            // the deck is technically playing, but saying "Playing" misleads —
            // the user pressed cue, so say that. cue_default is already held
            // when CueControl starts the preview, so this read is race-free.
            if (readGroupControl(group, QStringLiteral("cue_default")) > 0.0) {
                m_deckCuePreview[group] = true;
                // Earcon-capable: repeated cue taps while beat-matching turned
                // "Cue" into an irritating chant — the Cue feedback mode lets
                // the user pick a short tick instead.
                emitCue(static_cast<int>(EngineEarcon::Id::CuePreview),
                        deckIndex,
                        tr("Cue"));
            } else {
                emitCue(static_cast<int>(EngineEarcon::Id::Play), deckIndex, tr("Playing"));
            }
        } else if (!nowPlaying && wasPlaying && m_settings.getAnnounceStop()) {
            // Releasing the cue button ends the preview; that is not a "stop"
            // the user needs narrated.
            const bool wasCuePreview = m_deckCuePreview.take(group);
            // Suppress the stop announcement when end-of-track fired it —
            // the end-of-track announcement already covered this transition.
            const bool atEnd =
                    readGroupControl(group, QStringLiteral("end_of_track")) > 0.0;
            if (!wasCuePreview && !atEnd) {
                emitCue(static_cast<int>(EngineEarcon::Id::Stop), deckIndex, tr("Stopped"));
            }
        }
        m_deckIsPlaying[group] = nowPlaying;
    });

    auto pEndOfTrack = make_parented<ControlProxy>(
            group, QStringLiteral("end_of_track"), this, ControlFlag::AllowMissingOrInvalid);
    pEndOfTrack->connectValueChanged(this, [this, group, deckIndex](double value) {
        if (value > 0.0 && m_settings.getAnnounceEndOfTrack()) {
            // Say how much is actually left so the DJ knows how long they
            // have to bring in the next track. In sounds-only mode the earcon
            // is the alert; the time is spoken only when speech is on.
            const double duration = readGroupControl(group, QStringLiteral("duration"));
            QString text;
            if (duration > 0.0) {
                const double playPos =
                        readGroupControl(group, QStringLiteral("playposition"));
                text = tr("End of track. %1.")
                               .arg(remainingText(static_cast<int>(std::lround(
                                       duration * (1.0 - playPos)))));
            } else {
                text = tr("End of track");
            }
            emitCue(static_cast<int>(EngineEarcon::Id::EndOfTrack), deckIndex, text);
        }
    });

    // Jumping back to the start (the Start key, cue-goto-and-stop, or the
    // DDJ-400's Shift+CUE, which the fork remaps to start_stop) gives no
    // audible feedback of its own; narrate it.
    for (const char* backControl : {"start", "cue_gotoandstop", "start_stop"}) {
        auto pBack = make_parented<ControlProxy>(group,
                QLatin1String(backControl),
                this,
                ControlFlag::AllowMissingOrInvalid);
        pBack->connectValueChanged(this, [this, group, deckIndex](double value) {
            if (value > 0.0 && m_settings.getAnnouncePlay()) {
                emitCue(static_cast<int>(EngineEarcon::Id::Restart),
                        deckIndex,
                        tr("%1 back to start").arg(deckName(group, deckIndex)));
            }
        });
    }

    auto pPfl = make_parented<ControlProxy>(
            group, QStringLiteral("pfl"), this, ControlFlag::AllowMissingOrInvalid);
    pPfl->connectValueChanged(this, [this, group, deckIndex](double value) {
        if (m_settings.getAnnounceCue()) {
            // "Headphone cue", not just "Cue": a DJ would otherwise confuse
            // this with the transport cue button or hotcues. Name the deck so
            // it is clear which channel was cued.
            const QString deck = deckName(group, deckIndex);
            if (value > 0.0) {
                emitCue(static_cast<int>(EngineEarcon::Id::CueOn),
                        deckIndex,
                        tr("%1 headphone cue on").arg(deck));
            } else {
                emitCue(static_cast<int>(EngineEarcon::Id::CueOff),
                        deckIndex,
                        tr("%1 headphone cue off").arg(deck));
            }
        }
    });

    // Sync is a hold-to-latch button: a short press beat-syncs once (the CO
    // pulses 1 then reverts), holding past the latch threshold locks sync on.
    // Announce what actually happened instead of narrating the pulse as
    // "sync on ... sync off", which testers found misleading.
    auto* pSyncLatchProbe = new QTimer(this);
    pSyncLatchProbe->setSingleShot(true);
    pSyncLatchProbe->setInterval(kSyncLatchProbeMs);
    auto pSyncEnabled = make_parented<ControlProxy>(group,
            QStringLiteral("sync_enabled"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    connect(pSyncLatchProbe,
            &QTimer::timeout,
            this,
            [this, group, deckIndex, pSync = static_cast<ControlProxy*>(pSyncEnabled)]() {
                if (pSync->toBool() && m_settings.getAnnounceSync()) {
                    speak(tr("%1 sync locked").arg(deckName(group, deckIndex)));
                }
            });
    pSyncEnabled->connectValueChanged(this,
            [this, group, deckIndex, pSyncLatchProbe](double value) {
                if (!m_settings.getAnnounceSync()) {
                    pSyncLatchProbe->stop();
                    return;
                }
                if (value > 0.0) {
                    // Might be a short press; wait out the latch window
                    // before claiming sync is locked.
                    pSyncLatchProbe->start();
                } else if (pSyncLatchProbe->isActive()) {
                    // Released inside the window: a one-shot beat sync.
                    pSyncLatchProbe->stop();
                    speak(tr("%1 beat synced. Hold sync to lock")
                                    .arg(deckName(group, deckIndex)));
                } else {
                    speak(tr("%1 sync off").arg(deckName(group, deckIndex)));
                }
            });

    // Simple on/off toggles a performing DJ relies on. All gated by
    // AnnounceSync since they are aspects of staying in time with the mix.
    const struct {
        const char* control;
        QString enabledText;
        QString disabledText;
    } toggles[] = {
            {"keylock", tr("%1 key lock on"), tr("%1 key lock off")},
            {"quantize", tr("%1 quantize on"), tr("%1 quantize off")},
    };
    for (const auto& toggle : toggles) {
        auto pToggle = make_parented<ControlProxy>(group,
                QLatin1String(toggle.control),
                this,
                ControlFlag::AllowMissingOrInvalid);
        pToggle->connectValueChanged(this,
                [this,
                        group,
                        deckIndex,
                        enabledText = toggle.enabledText,
                        disabledText = toggle.disabledText](double value) {
                    if (m_settings.getAnnounceSync()) {
                        speak((value > 0.0 ? enabledText : disabledText)
                                        .arg(deckName(group, deckIndex)));
                    }
                });
    }

    auto pLoopEnabled = make_parented<ControlProxy>(group,
            QStringLiteral("loop_enabled"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pLoopEnabled->connectValueChanged(this, [this, group, deckIndex](double value) {
        if (!m_settings.getAnnounceLoop()) {
            return;
        }
        const QString deck = deckName(group, deckIndex);
        if (value > 0.0) {
            const double beats = ControlProxy(group,
                    QStringLiteral("beatloop_size"),
                    nullptr,
                    ControlFlag::AllowMissingOrInvalid)
                                         .get();
            // Seed the loop_scale tracker below so CUE/LOOP CALL halve/double
            // presses have a sane starting size to scale from.
            m_deckLoopBeats[group] = beats;
            const QString text = beats > 0.0
                    ? tr("%1 loop %2 beats").arg(deck, QString::number(beats))
                    : tr("%1 loop on").arg(deck);
            emitCue(static_cast<int>(EngineEarcon::Id::LoopOn), deckIndex, text);
        } else {
            emitCue(static_cast<int>(EngineEarcon::Id::LoopOff),
                    deckIndex,
                    tr("%1 loop off").arg(deck));
        }
    });

    // Hotcue set/cleared feedback. Status is 0 = empty, 1 = set, 2 = active
    // (saved-loop); only the empty <-> set transitions are user-meaningful.
    for (int i = 1; i <= kNumAnnouncedHotcues; ++i) {
        auto pHotcue = make_parented<ControlProxy>(group,
                QStringLiteral("hotcue_%1_status").arg(i),
                this,
                ControlFlag::AllowMissingOrInvalid);
        pHotcue->connectValueChanged(this,
                [this, group, deckIndex, i, prev = 0.0](double value) mutable {
                    const double before = prev;
                    prev = value;
                    if (!m_settings.getAnnounceHotcue() || recentTrackChange(group)) {
                        return;
                    }
                    const QString deck = deckName(group, deckIndex);
                    if (before < 1.0 && value >= 1.0) {
                        speak(tr("%1 hotcue %2 set").arg(deck).arg(i));
                    } else if (before >= 1.0 && value < 1.0) {
                        speak(tr("%1 hotcue %2 cleared").arg(deck).arg(i));
                    }
                });

        // Pressing a hotcue that is already set jumps to it; confirm which
        // pad was hit. When the pad was empty the press sets the cue instead,
        // and the status observer above announces "set" — stay quiet here to
        // avoid speaking twice.
        auto pActivate = make_parented<ControlProxy>(group,
                QStringLiteral("hotcue_%1_activate").arg(i),
                this,
                ControlFlag::AllowMissingOrInvalid);
        pActivate->connectValueChanged(this,
                [this, group, deckIndex, i](double value) {
                    if (value <= 0.0 || !m_settings.getAnnounceHotcue() ||
                            recentTrackChange(group)) {
                        return;
                    }
                    const double status = readGroupControl(group,
                            QStringLiteral("hotcue_%1_status").arg(i));
                    if (status >= 1.0) {
                        speak(tr("%1 hotcue %2")
                                        .arg(deckName(group, deckIndex))
                                        .arg(i));
                    }
                });
    }

    // Setting the main cue point.
    auto pCueSet = make_parented<ControlProxy>(group,
            QStringLiteral("cue_set"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pCueSet->connectValueChanged(this, [this, group, deckIndex](double value) {
        if (value > 0.0 && m_settings.getAnnounceHotcue()) {
            speak(tr("%1 cue set").arg(deckName(group, deckIndex)));
        }
    });

    // Loop size changes (halve/double or direct selection), debounced since
    // the size is often stepped several times in a row.
    auto pLoopSize = make_parented<ControlProxy>(group,
            QStringLiteral("beatloop_size"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pLoopSize->connectValueChanged(this, [this, group, deckIndex](double value) {
        if (!m_settings.getAnnounceLoop() || value <= 0.0) {
            return;
        }
        m_deckLoopBeats[group] = value;
        announceControlDebounced(tr("%1 loop size %2")
                        .arg(mixerDeckName(group, deckIndex),
                                QString::number(value)));
    });

    // CUE/LOOP CALL <>/> (loop_scale) halves/doubles the active loop's actual
    // length directly — see LoopingControl::slotLoopScale, which deliberately
    // clears the active beatloop rather than reconciling beatloop_size after
    // a scale — so the beatloop_size observer above never fires for this
    // control and the loop-size change goes unannounced. Scale our own
    // best-known loop size (seeded above from loop_enabled/beatloop_size) so
    // repeated presses keep announcing an accurate size.
    auto pLoopScale = make_parented<ControlProxy>(group,
            QStringLiteral("loop_scale"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pLoopScale->connectValueChanged(this, [this, group, deckIndex](double scaleFactor) {
        if (!m_settings.getAnnounceLoop() || scaleFactor <= 0.0) {
            return;
        }
        double beats = m_deckLoopBeats.value(group, 0.0);
        if (beats <= 0.0) {
            beats = readGroupControl(group, QStringLiteral("beatloop_size"));
        }
        if (beats <= 0.0) {
            return;
        }
        beats *= scaleFactor;
        m_deckLoopBeats[group] = beats;
        announceControlDebounced(tr("%1 loop size %2")
                        .arg(mixerDeckName(group, deckIndex),
                                QString::number(beats)));
    });

    // Beat jump: size changes and the actual jumps. Debounced — the size
    // ticker and repeated jump presses can fire several times a second.
    auto pBeatjumpSize = make_parented<ControlProxy>(group,
            QStringLiteral("beatjump_size"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pBeatjumpSize->connectValueChanged(this, [this, group, deckIndex](double value) {
        if (!m_settings.getAnnounceLoop() || value <= 0.0) {
            return;
        }
        announceControlDebounced(tr("%1 beat jump size %2")
                        .arg(mixerDeckName(group, deckIndex),
                                QString::number(value)));
    });

    const struct {
        const char* control;
        QString text;
    } beatjumps[] = {
            {"beatjump_forward", tr("%1 jump forward %2 beats")},
            {"beatjump_backward", tr("%1 jump back %2 beats")},
    };
    for (const auto& jump : beatjumps) {
        auto pJump = make_parented<ControlProxy>(group,
                QLatin1String(jump.control),
                this,
                ControlFlag::AllowMissingOrInvalid);
        pJump->connectValueChanged(this,
                [this, group, deckIndex, text = jump.text](double value) {
                    if (!m_settings.getAnnounceLoop() || value <= 0.0) {
                        return;
                    }
                    const double size = readGroupControl(
                            group, QStringLiteral("beatjump_size"));
                    announceControlDebounced(text.arg(
                            mixerDeckName(group, deckIndex),
                            QString::number(size > 0.0 ? size : 1.0)));
                });
    }

    // Continuously-variable deck controls, debounced so only the value where
    // the control comes to rest is spoken.
    auto pRateRatio = make_parented<ControlProxy>(group,
            QStringLiteral("rate_ratio"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pRateRatio->connectValueChanged(this, [this, group, deckIndex](double value) {
        if (!m_settings.getAnnounceTempo()) {
            return;
        }
        const QString deviation = pitchDeviationText(value);
        QString valueText = deviation.isEmpty() ? tr("zero") : deviation;
        // Say the resulting BPM — the number the DJ is actually chasing.
        const double bpm = readGroupControl(group, QStringLiteral("bpm"));
        if (bpm > 0.0) {
            valueText += tr(". %1 B P M").arg(static_cast<int>(std::lround(bpm)));
        }
        announceControlDebounced(group + QStringLiteral("rate_ratio"),
                tr("%1 pitch").arg(mixerDeckName(group, deckIndex)),
                valueText);
    });

    // Fixing a half/double-tempo misanalysis (common for 160+ BPM genres —
    // the analyzer has no tempo-range hint) is done with beats_set_halve /
    // beats_set_double; confirm the action. The new BPM is not read here:
    // the engine updates the bpm CO on its own schedule, so reading it back
    // immediately would race — the tts_bpm hotkey gives the exact number.
    const struct {
        const char* control;
        QString text;
    } beatsAdjust[] = {
            {"beats_set_halve", tr("%1 B P M halved")},
            {"beats_set_double", tr("%1 B P M doubled")},
    };
    for (const auto& adjust : beatsAdjust) {
        auto pAdjust = make_parented<ControlProxy>(group,
                QLatin1String(adjust.control),
                this,
                ControlFlag::AllowMissingOrInvalid);
        pAdjust->connectValueChanged(this,
                [this, group, deckIndex, text = adjust.text](double value) {
                    if (value > 0.0 && m_settings.getAnnounceTempo()) {
                        speak(text.arg(deckName(group, deckIndex)));
                    }
                });
    }

    // volume is a ControlAudioTaperPot (dB-tapered): get() returns the linear
    // gain multiplier, not the fader position, so a physical half-way fader
    // reads out as roughly a quarter of the way (dB taper drops much faster
    // than linear near the top). Read getParameter() instead, which is the
    // 0..1 knob/fader position that matches what the user actually moved.
    auto pVolume = make_parented<ControlProxy>(group,
            QStringLiteral("volume"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pVolume->connectValueChanged(this,
            [this,
                    group,
                    deckIndex,
                    pVolumeRaw = static_cast<ControlProxy*>(pVolume)](double) {
                if (!m_settings.getAnnounceMixer()) {
                    return;
                }
                announceControlDebounced(group + QStringLiteral("volume"),
                        tr("%1 volume").arg(mixerDeckName(group, deckIndex)),
                        fractionText(
                                std::clamp(pVolumeRaw->getParameter(), 0.0, 1.0),
                                mixerReadoutAsPercent(),
                                mixerFractionDenominator()));
            });

    // Trim / channel pregain: also a ControlAudioTaperPot, neutral at
    // parameter 0.5 (center of the knob = unity gain), so the same
    // parameter-vs-value distinction applies as for volume above.
    auto pPregain = make_parented<ControlProxy>(group,
            QStringLiteral("pregain"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pPregain->connectValueChanged(this,
            [this,
                    group,
                    deckIndex,
                    pPregainRaw = static_cast<ControlProxy*>(pPregain)](double) {
                if (!m_settings.getAnnounceMixer()) {
                    return;
                }
                announceControlDebounced(group + QStringLiteral("pregain"),
                        tr("%1 trim").arg(mixerDeckName(group, deckIndex)),
                        centerSplitText(
                                (pPregainRaw->getParameter() - 0.5) * 2.0,
                                mixerReadoutAsPercent(),
                                mixerFractionDenominator()));
            });

    // EQ knobs. Values run 0..4 with unity at 1; spoken as a signed fraction
    // from center ("low minus a quarter") — the way a DJ pictures the knob.
    const QString eqGroup = QStringLiteral("[EqualizerRack1_%1_Effect1]").arg(group);
    const struct {
        const char* control;
        QString name;
        QString conciseName;
    } eqBands[] = {
            {"parameter1", tr("E Q low"), tr("low")},
            {"parameter2", tr("E Q mid"), tr("mid")},
            {"parameter3", tr("E Q high"), tr("high")},
    };
    for (const auto& band : eqBands) {
        auto pKnob = make_parented<ControlProxy>(eqGroup,
                QLatin1String(band.control),
                this,
                ControlFlag::AllowMissingOrInvalid);
        pKnob->connectValueChanged(this,
                [this,
                        group,
                        deckIndex,
                        bandName = band.name,
                        conciseName = band.conciseName,
                        // See the comment on the equivalent capture in the
                        // "[Master]" gain lambda above: without the
                        // QString(...) cast, QT_USE_QSTRINGBUILDER makes this
                        // a dangling reference to eqGroup.
                        key = QString(eqGroup + QLatin1String(band.control))](double value) {
                    if (!m_settings.getAnnounceMixer()) {
                        return;
                    }
                    const QString name = m_settings.getConciseAnnouncements()
                            ? conciseName
                            : bandName;
                    announceControlDebounced(key,
                            QStringLiteral("%1 %2").arg(
                                    mixerDeckName(group, deckIndex), name),
                            centerSplitText(
                                    normalizeUnityGain(value),
                                    mixerReadoutAsPercent(),
                                    mixerFractionDenominator()));
                });
    }

    // Filter (QuickEffect super knob): 0..1 with center at 0.5.
    const QString quickEffectGroup =
            QStringLiteral("[QuickEffectRack1_%1]").arg(group);
    auto pFilter = make_parented<ControlProxy>(quickEffectGroup,
            QStringLiteral("super1"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pFilter->connectValueChanged(this, [this, group, deckIndex, quickEffectGroup](double value) {
        if (!m_settings.getAnnounceMixer()) {
            return;
        }
        announceControlDebounced(quickEffectGroup + QStringLiteral("super1"),
                tr("%1 filter").arg(mixerDeckName(group, deckIndex)),
                centerSplitText((value - 0.5) * 2.0,
                        mixerReadoutAsPercent(),
                        mixerFractionDenominator()));
    });

    // Effect unit routing: [EffectRack1_EffectUnitU],group_[ChannelN]_enable
    // switches this deck through unit U. A distinct event from the per-effect
    // enables above — this is "is my deck going through the unit at all".
    for (int unit = 1; unit <= 4; ++unit) {
        auto pRouting = make_parented<ControlProxy>(
                QStringLiteral("[EffectRack1_EffectUnit%1]").arg(unit),
                QStringLiteral("group_%1_enable").arg(group),
                this,
                ControlFlag::AllowMissingOrInvalid);
        pRouting->connectValueChanged(this,
                [this, group, deckIndex, unit](double value) {
                    if (!m_settings.getAnnounceEffects()) {
                        return;
                    }
                    speak((value > 0.0 ? tr("%1 effect unit %2 on")
                                       : tr("%1 effect unit %2 off"))
                                    .arg(deckName(group, deckIndex),
                                            QString::number(unit)));
                });
    }

    // Filter knob effect type: the QuickEffect chain preset. The CO holds an
    // index into the chain preset list; the resolver supplies the name.
    auto pQuickEffectPreset = make_parented<ControlProxy>(quickEffectGroup,
            QStringLiteral("loaded_chain_preset"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pQuickEffectPreset->connectValueChanged(this,
            [this, group, deckIndex](double value) {
                if (!m_settings.getAnnounceEffects()) {
                    return;
                }
                QString name = m_quickEffectNameResolver
                        ? m_quickEffectNameResolver(group)
                        : QString();
                if (name.isEmpty()) {
                    name = tr("preset %1").arg(static_cast<int>(value));
                }
                // Debounced: the preset selector can tick through several
                // presets per second.
                announceControlDebounced(tr("%1 filter: %2")
                                .arg(mixerDeckName(group, deckIndex), name));
            });

    // Vinyl control (DVS) state. Not gated by a preference: mode changes also
    // happen automatically (a loop or a seek drops absolute mode to relative,
    // the end of the record switches to constant mode), and without feedback
    // a blind DJ has no way to know why the deck stopped following the
    // turntable.
    auto pVinylEnabled = make_parented<ControlProxy>(group,
            QStringLiteral("vinylcontrol_enabled"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pVinylEnabled->connectValueChanged(this, [this, group, deckIndex](double value) {
        speak((value > 0.0 ? tr("%1 vinyl control on")
                           : tr("%1 vinyl control off"))
                        .arg(deckName(group, deckIndex)));
    });

    auto pVinylMode = make_parented<ControlProxy>(group,
            QStringLiteral("vinylcontrol_mode"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pVinylMode->connectValueChanged(this, [this, group, deckIndex](double value) {
        QString modeText;
        switch (static_cast<int>(value)) {
        case MIXXX_VCMODE_ABSOLUTE:
            modeText = tr("%1 vinyl absolute mode");
            break;
        case MIXXX_VCMODE_RELATIVE:
            modeText = tr("%1 vinyl relative mode");
            break;
        case MIXXX_VCMODE_CONSTANT:
            modeText = tr("%1 vinyl constant mode");
            break;
        default:
            return;
        }
        speak(modeText.arg(deckName(group, deckIndex)));
    });

    auto pVinylCueing = make_parented<ControlProxy>(group,
            QStringLiteral("vinylcontrol_cueing"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    pVinylCueing->connectValueChanged(this, [this, group, deckIndex](double value) {
        QString cueingText;
        switch (static_cast<int>(value)) {
        case MIXXX_RELATIVE_CUE_OFF:
            cueingText = tr("%1 needle drop cueing off");
            break;
        case MIXXX_RELATIVE_CUE_ONECUE:
            cueingText = tr("%1 needle drop goes to cue point");
            break;
        case MIXXX_RELATIVE_CUE_HOTCUE:
            cueingText = tr("%1 needle drop goes to nearest hotcue");
            break;
        default:
            return;
        }
        speak(cueingText.arg(deckName(group, deckIndex)));
    });
}

void AnnouncementManager::setEffectNameResolvers(
        std::function<QString(int unit, int slot)> effectName,
        std::function<QString(const QString& deckGroup)> quickEffectName) {
    m_effectNameResolver = std::move(effectName);
    m_quickEffectNameResolver = std::move(quickEffectName);
}

void AnnouncementManager::setDeckHasTrack(const QString& group, bool value) {
    m_deckHasTrack[group] = value;
}

void AnnouncementManager::connectDeck(int deckIndex) {
    BaseTrackPlayer* pDeck = m_pPlayerManager->getDeckBase(deckIndex);
    if (!pDeck) {
        return;
    }
    const QString group = pDeck->getGroup();

    // Pre-populate for tracks that were already loaded before this manager
    // was created (e.g. session restore at startup).
    m_deckHasTrack[group] = (pDeck->getLoadedTrack() != nullptr);

    connect(pDeck, &BaseTrackPlayer::newTrackLoaded, this, [this, deckIndex](TrackPointer pTrack) {
        slotNewTrackLoaded(pTrack, deckIndex);
    });

    connect(pDeck, &BaseTrackPlayer::newTrackLoaded, this, [this, group](TrackPointer) {
        m_deckHasTrack[group] = true;
        noteTrackChanged(group);
    });
    connect(pDeck, &BaseTrackPlayer::trackUnloaded, this, [this, group](TrackPointer) {
        m_deckHasTrack[group] = false;
        m_deckIsPlaying[group] = false;
        noteTrackChanged(group);
    });

    connectGroupControls(group, deckIndex);
}

void AnnouncementManager::slotNumberOfDecksChanged(int decks) {
    for (int i = m_connectedDecks; i < decks; ++i) {
        connectDeck(i);
    }
    m_connectedDecks = decks;
}

void AnnouncementManager::slotTrackSelected(TrackPointer pTrack) {
    // Only announce selection when the user is actively browsing the track list.
    // If focus is on the sidebar, the signal fires for the first track in the
    // newly-loaded feature view — not something the user deliberately selected.
    if (m_lastFocusWidget != FocusWidget::TracksTable) {
        return;
    }
    m_pendingTrack = pTrack;
    m_selectionDebounce.start();
}

void AnnouncementManager::slotAnnounceSelectedTrack() {
    if (m_pendingTrack && m_settings.getAnnounceTrackSelection()) {
        speak(formatForBrowsing(m_pendingTrack));
    }
}

void AnnouncementManager::slotNewTrackLoaded(TrackPointer pTrack, int deckIndex) {
    if (pTrack && m_settings.getAnnounceTrackLoad()) {
        speak(formatForLoad(pTrack, deckIndex));
    }

    // Smart cue (like Denon players): the freshly loaded track becomes what
    // the headphones preview — the whole point of loading a track is to
    // hear it next, so the cue follows the load instead of making the DJ
    // hunt for the right cue button. Only when the target deck is not
    // playing: a live deck never has its cue stolen mid-mix. The pfl
    // changes themselves are announced by the existing cue observers.
    if (!pTrack || deckIndex < 0 || !m_pPlayerManager ||
            !m_pConfig->getValue(kConfigKeySmartCue, kDefaultSmartCue)) {
        return;
    }
    const QString group = PlayerManager::groupForDeck(deckIndex);
    if (readGroupControl(group, QStringLiteral("play")) > 0.0) {
        return;
    }
    const int numDecks = m_pPlayerManager->numberOfDecks();
    for (int i = 0; i < numDecks; ++i) {
        ControlProxy(PlayerManager::groupForDeck(i),
                QStringLiteral("pfl"),
                nullptr,
                ControlFlag::AllowMissingOrInvalid)
                .set(i == deckIndex ? 1.0 : 0.0);
    }
}

// static
QString AnnouncementManager::formatForBrowsing(TrackPointer pTrack) {
    const QString artist = pTrack->getArtist().trimmed();
    const QString title = pTrack->getTitle().trimmed();
    if (artist.isEmpty()) {
        return title;
    }
    if (title.isEmpty()) {
        return artist;
    }
    return artist + QStringLiteral(", ") + title;
}

void AnnouncementManager::slotSkinLoaded() {
    if (m_settings.getAnnounceStartup()) {
        speak(tr("Mixxx ready"));
    }
}

void AnnouncementManager::slotSidebarItemActivated(const QString& title,
        int row,
        int siblingCount,
        int childCount,
        bool expanded) {
    if (!m_settings.getAnnounceLibraryFocus() || title.isEmpty()) {
        return;
    }
    QString text = title;
    // Position among siblings so the user knows where they are in the list.
    if (row >= 0 && siblingCount > 1) {
        text += tr(", %1 of %2").arg(row + 1).arg(siblingCount);
    }
    // Container items: expand state and how many children are inside.
    if (childCount > 0) {
        text += expanded
                ? tr(", expanded, %1 items").arg(childCount)
                : tr(", collapsed, %1 items").arg(childCount);
    }
    // Deduplicate: currentChanged and featureSelect can both fire for the same
    // item on a mouse click. Keyed on the full text so an expand/collapse of
    // the same item still re-announces with the new state.
    if (text == m_lastAnnouncedSidebarItem) {
        return;
    }
    m_lastAnnouncedSidebarItem = text;
    speak(text);
}

void AnnouncementManager::slotPlaylistTracksEdited(
        const QString& name, int added, int removed) {
    if (!m_settings.getAnnouncePlaylist()) {
        return;
    }
    if (added > 0) {
        speak(tr("Added to playlist %1").arg(name));
    } else if (removed > 0) {
        speak(tr("Removed from playlist %1").arg(name));
    }
}

void AnnouncementManager::slotCrateTracksEdited(
        const QString& name, int added, int removed) {
    if (!m_settings.getAnnouncePlaylist()) {
        return;
    }
    if (added > 0 && removed > 0) {
        return; // bulk reshuffle, not a user add/remove
    }
    if (added == 1) {
        speak(tr("Added to crate %1").arg(name));
    } else if (added > 1) {
        speak(tr("Added %1 tracks to crate %2").arg(added).arg(name));
    } else if (removed == 1) {
        speak(tr("Removed from crate %1").arg(name));
    } else if (removed > 1) {
        speak(tr("Removed %1 tracks from crate %2").arg(removed).arg(name));
    }
}

void AnnouncementManager::slotQuickPickerItemHighlighted(
        const QString& text, int row, int siblingCount) {
    // Unconditional: the picker was opened on purpose, so hearing its items
    // isn't gated by a settings toggle the way ambient sidebar browsing is.
    if (text.isEmpty()) {
        return;
    }
    QString spoken = text;
    if (row >= 0 && siblingCount > 1) {
        spoken += tr(", %1 of %2").arg(row + 1).arg(siblingCount);
    }
    speak(spoken);
}

void AnnouncementManager::slotLibraryFocusChanged(double value) {
    const auto newFocus = static_cast<FocusWidget>(static_cast<int>(value));
    const FocusWidget prevFocus = m_lastFocusWidget;
    m_lastFocusWidget = newFocus;

    // Reset sidebar dedup so that re-entering the sidebar re-announces the
    // current item. Done unconditionally (before the None-suppression guard)
    // so the first ever focus-enter to the sidebar (prevFocus == None at
    // startup) also clears the stale "Tracks" set by activateDefaultSelection.
    if (newFocus == FocusWidget::Sidebar) {
        m_lastAnnouncedSidebarItem.clear();
    }

    // Suppress the announcement when the OS returns focus to the window — the
    // CO transitions from None (lost focus) back to whatever widget was active.
    // We only want to announce intentional navigation between library panels.
    if (prevFocus == FocusWidget::None) {
        return;
    }

    if (!m_settings.getAnnounceLibraryFocus()) {
        return;
    }
    switch (newFocus) {
    case FocusWidget::Searchbar:
        speak(tr("Search bar"));
        break;
    case FocusWidget::Sidebar:
        speak(tr("Sidebar"));
        break;
    case FocusWidget::TracksTable:
        speak(tr("Track list"));
        break;
    default:
        break;
    }
}

void AnnouncementManager::slotSearchTextChanged(const QString& text) {
    m_pendingSearch = text;
    // The result count for the new query arrives separately (the track table
    // applies the search synchronously and reports back); forget the count of
    // the previous query so a missing report can't attach a stale number.
    m_pendingSearchCount = -1;
    m_searchDebounce.start();
}

void AnnouncementManager::slotSearchResultCount(int count) {
    m_pendingSearchCount = count;
}

void AnnouncementManager::slotAnnounceSearch() {
    if (!m_settings.getAnnounceSearch()) {
        return;
    }
    QString text = m_pendingSearch.isEmpty()
            ? tr("Search cleared")
            : tr("Searching: %1").arg(m_pendingSearch);
    // Say how many tracks the filter left — the whole point of filtering is
    // knowing whether anything (or too much) matched.
    if (m_pendingSearchCount == 0) {
        text += tr(". No tracks");
    } else if (m_pendingSearchCount == 1) {
        text += tr(". 1 track");
    } else if (m_pendingSearchCount > 1) {
        text += tr(". %1 tracks").arg(m_pendingSearchCount);
    }
    speak(text);
}

void AnnouncementManager::slotAnnounceSort() {
    if (!m_settings.getAnnounceSort()) {
        return;
    }
    ControlProxy sortColumn(QStringLiteral("[Library]"),
            QStringLiteral("sort_column"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    ControlProxy sortOrder(QStringLiteral("[Library]"),
            QStringLiteral("sort_order"),
            this,
            ControlFlag::AllowMissingOrInvalid);
    const auto columnId =
            static_cast<TrackModel::SortColumnId>(static_cast<int>(sortColumn.get()));
    const QString columnName = sortColumnName(columnId);
    if (columnName.isEmpty()) {
        return;
    }
    const bool ascending = sortOrder.get() == 0.0;
    speak(tr("Sorting by %1 %2")
                    .arg(columnName,
                            ascending ? tr("ascending") : tr("descending")));
}

QString AnnouncementManager::sortColumnName(TrackModel::SortColumnId column) {
    // Spoken column names, mirroring the display titles in columncache.cpp
    // (BaseTrackTableModel/BaseSqlTableModel translation contexts).
    switch (column) {
    case TrackModel::SortColumnId::Artist:
        return tr("artist");
    case TrackModel::SortColumnId::Title:
        return tr("title");
    case TrackModel::SortColumnId::Album:
        return tr("album");
    case TrackModel::SortColumnId::AlbumArtist:
        return tr("album artist");
    case TrackModel::SortColumnId::Year:
        return tr("year");
    case TrackModel::SortColumnId::Genre:
        return tr("genre");
    case TrackModel::SortColumnId::Composer:
        return tr("composer");
    case TrackModel::SortColumnId::Grouping:
        return tr("grouping");
    case TrackModel::SortColumnId::TrackNumber:
        return tr("track number");
    case TrackModel::SortColumnId::FileType:
        return tr("file type");
    case TrackModel::SortColumnId::NativeLocation:
        return tr("location");
    case TrackModel::SortColumnId::Comment:
        return tr("comment");
    case TrackModel::SortColumnId::Duration:
        return tr("duration");
    case TrackModel::SortColumnId::BitRate:
        return tr("bitrate");
    case TrackModel::SortColumnId::Bpm:
        return tr("BPM");
    case TrackModel::SortColumnId::ReplayGain:
        return tr("replay gain");
    case TrackModel::SortColumnId::DateTimeAdded:
        return tr("date added");
    case TrackModel::SortColumnId::TimesPlayed:
        return tr("times played");
    case TrackModel::SortColumnId::Rating:
        return tr("rating");
    case TrackModel::SortColumnId::Key:
        return tr("key");
    case TrackModel::SortColumnId::Preview:
        return tr("preview");
    case TrackModel::SortColumnId::CoverArt:
        return tr("cover art");
    case TrackModel::SortColumnId::Position:
        return tr("position");
    case TrackModel::SortColumnId::PlaylistId:
        return tr("playlist");
    case TrackModel::SortColumnId::Location:
        return tr("location");
    case TrackModel::SortColumnId::Filename:
        return tr("filename");
    case TrackModel::SortColumnId::FileModifiedTime:
        return tr("modified time");
    case TrackModel::SortColumnId::FileCreationTime:
        return tr("creation time");
    case TrackModel::SortColumnId::SampleRate:
        return tr("sample rate");
    case TrackModel::SortColumnId::Color:
        return tr("color");
    case TrackModel::SortColumnId::LastPlayedAt:
        return tr("last played");
    case TrackModel::SortColumnId::PlaylistDateTimeAdded:
        return tr("date added");
    default:
        return QString();
    }
}

// static
QString AnnouncementManager::formatForLoad(TrackPointer pTrack, int deckIndex) {
    const QString artist = pTrack->getArtist().trimmed();
    const QString title = pTrack->getTitle().trimmed();
    const double bpm = pTrack->getBpm();
    const QString keyText = keyForSpeechInNotation(pTrack->getKey());

    // "B P M" with spaces causes TTS engines to read each letter individually
    // rather than trying to pronounce it as a word.
    QStringList parts;
    // Comma before the letter so TTS says "Loaded deck, Alpha" rather than
    // gluing it into "Loaded decka"; the letter is spelled out phonetically
    // for the same reason as deckName().
    parts << tr("Loaded deck, %1").arg(phoneticLetter(QChar(u'A' + deckIndex)));
    if (!artist.isEmpty()) {
        parts << artist;
    }
    if (!title.isEmpty()) {
        parts << title;
    }
    if (bpm > 0.0) {
        parts << tr("%1 B P M").arg(static_cast<int>(bpm + 0.5));
    }
    if (!keyText.isEmpty()) {
        parts << tr("Key: %1").arg(keyText);
    }
    return parts.join(QStringLiteral(". ")) + QStringLiteral(".");
}

QString AnnouncementManager::formatDeckStatus(const QString& group, int deckIndex) const {
    const QString deck = deckName(group, deckIndex);
    if (!m_deckHasTrack.value(group, false)) {
        return tr("%1. No track loaded.").arg(deck);
    }

    QStringList parts;
    parts << deck;
    parts << (readGroupControl(group, QStringLiteral("play")) > 0.0
                    ? tr("Playing")
                    : tr("Stopped"));

    const double duration = readGroupControl(group, QStringLiteral("duration"));
    if (duration > 0.0) {
        const double playPos = readGroupControl(group, QStringLiteral("playposition"));
        parts << remainingText(
                static_cast<int>(std::lround(duration * (1.0 - playPos))));
    }

    const double bpm = readGroupControl(group, QStringLiteral("bpm"));
    if (bpm > 0.0) {
        parts << tr("%1 B P M").arg(static_cast<int>(std::lround(bpm)));
    }

    const QString pitch = pitchText(readGroupControl(group, QStringLiteral("rate_ratio")));
    if (!pitch.isEmpty()) {
        parts << pitch;
    }
    return parts.join(QStringLiteral(". ")) + QStringLiteral(".");
}

QString AnnouncementManager::formatTimeRemaining(const QString& group, int deckIndex) const {
    const QString deck = deckName(group, deckIndex);
    const double duration = readGroupControl(group, QStringLiteral("duration"));
    if (!m_deckHasTrack.value(group, false) || duration <= 0.0) {
        return tr("%1. No track loaded.").arg(deck);
    }
    const double playPos = readGroupControl(group, QStringLiteral("playposition"));
    const QString remaining = remainingText(static_cast<int>(
                                      std::lround(duration * (1.0 - playPos)))) +
            QStringLiteral(".");
    // Concise: the hotkey pressed already identifies the deck.
    if (m_settings.getConciseAnnouncements()) {
        return remaining;
    }
    return deck + QStringLiteral(". ") + remaining;
}

QString AnnouncementManager::formatBpm(const QString& group, int deckIndex) const {
    const QString deck = deckName(group, deckIndex);
    const double bpm = readGroupControl(group, QStringLiteral("bpm"));
    if (bpm <= 0.0) {
        return m_settings.getConciseAnnouncements()
                ? tr("No B P M.")
                : tr("%1. No B P M.").arg(deck);
    }
    const int rounded = static_cast<int>(std::lround(bpm));
    if (m_settings.getConciseAnnouncements()) {
        return tr("%1.").arg(rounded);
    }
    return tr("%1. %2 B P M.").arg(deck).arg(rounded);
}

QString AnnouncementManager::formatKey(const QString& group, int deckIndex) const {
    const QString deck = deckName(group, deckIndex);
    // [ChannelN],key holds KeyUtils::keyToNumericValue(ChromaticKey) and is
    // keylock-aware, so this reads the currently sounding key.
    const int keyValue = static_cast<int>(
            std::lround(readGroupControl(group, QStringLiteral("key"))));
    const QString keyText = keyForSpeechInNotation(
            static_cast<mixxx::track::io::key::ChromaticKey>(keyValue));
    if (keyText.isEmpty()) {
        return m_settings.getConciseAnnouncements()
                ? tr("Key unknown.")
                : tr("%1. Key unknown.").arg(deck);
    }
    if (m_settings.getConciseAnnouncements()) {
        return tr("%1.").arg(keyText);
    }
    return tr("%1. Key: %2.").arg(deck, keyText);
}

QString AnnouncementManager::formatBarPosition(const QString& group, int deckIndex) const {
    const QString deck = deckName(group, deckIndex);
    TrackPointer pTrack;
    if (deckIndex >= 0 && m_pPlayerManager) {
        BaseTrackPlayer* pDeck = m_pPlayerManager->getDeckBase(deckIndex);
        if (pDeck) {
            pTrack = pDeck->getLoadedTrack();
        }
    }
    if (!pTrack) {
        return tr("%1. No track loaded.").arg(deck);
    }
    const mixxx::BeatsPointer pBeats = pTrack->getBeats();
    const mixxx::audio::SampleRate sampleRate = pTrack->getSampleRate();
    const double duration = pTrack->getDuration();
    if (!pBeats || !pBeats->firstBeat().isValid() || !sampleRate.isValid() ||
            duration <= 0.0) {
        return tr("%1. No beat grid.").arg(deck);
    }
    const double playPos = readGroupControl(group, QStringLiteral("playposition"));
    const auto position = mixxx::audio::FramePos(
            playPos * duration * sampleRate.toDouble());
    if (!position.isValid() || position < pBeats->firstBeat()) {
        return tr("%1. Before first beat.").arg(deck);
    }
    // Assumes 4/4, which covers the overwhelming majority of DJ material.
    const int beatsFromStart = pBeats->numBeatsInRange(pBeats->firstBeat(), position);
    const int bar = beatsFromStart / 4 + 1;
    const int beatInBar = beatsFromStart % 4 + 1;
    if (m_settings.getConciseAnnouncements()) {
        return tr("Bar %1, beat %2.").arg(bar).arg(beatInBar);
    }
    return tr("%1. Bar %2, beat %3.").arg(deck).arg(bar).arg(beatInBar);
}

QString AnnouncementManager::formatTrackName(const QString& group, int deckIndex) const {
    const QString deck = deckName(group, deckIndex);
    TrackPointer pTrack;
    if (deckIndex >= 0 && m_pPlayerManager) {
        BaseTrackPlayer* pDeck = m_pPlayerManager->getDeckBase(deckIndex);
        if (pDeck) {
            pTrack = pDeck->getLoadedTrack();
        }
    }
    if (!pTrack) {
        return m_settings.getConciseAnnouncements()
                ? tr("No track loaded.")
                : tr("%1. No track loaded.").arg(deck);
    }
    const QString trackText = formatForBrowsing(pTrack);
    if (m_settings.getConciseAnnouncements()) {
        return trackText + QStringLiteral(".");
    }
    return deck + QStringLiteral(". ") + trackText + QStringLiteral(".");
}

QString AnnouncementManager::deckName(const QString& group, int deckIndex) const {
    if (deckIndex < 0) {
        return group;
    }
    if (m_settings.getDeckNamesAsNumbers()) {
        return tr("Deck %1").arg(deckIndex + 1);
    }
    // The comma is deliberate: without a separator TTS engines glue the deck
    // letter onto the word ("Deck A" -> "Decka"). The letter itself is spelled
    // out phonetically (see phoneticLetter()) since a bare "A" is often
    // misread as the article rather than the letter name.
    return tr("Deck, %1").arg(phoneticLetter(QChar(u'A' + deckIndex)));
}

QString AnnouncementManager::mixerDeckName(const QString& group, int deckIndex) const {
    if (m_settings.getConciseAnnouncements() && deckIndex >= 0) {
        return m_settings.getDeckNamesAsNumbers()
                ? QString::number(deckIndex + 1)
                : phoneticLetter(QChar(u'A' + deckIndex));
    }
    return deckName(group, deckIndex);
}

bool AnnouncementManager::mixerReadoutAsPercent() const {
    return m_settings.getMixerReadoutStyle() == 1;
}

int AnnouncementManager::mixerFractionDenominator() const {
    switch (m_settings.getMixerFractionDetail()) {
    case 0:
        return 4;
    case 2:
        return 16;
    default:
        return 8;
    }
}

void AnnouncementManager::announceControlDebounced(const QString& text) {
    m_pendingControlKey.clear();
    m_pendingControlName.clear();
    m_pendingControlValue.clear();
    m_pendingControlText = text;
    startControlDebounce();
}

void AnnouncementManager::announceControlDebounced(
        const QString& key, const QString& name, const QString& valueText) {
    m_pendingControlText.clear();
    m_pendingControlKey = key;
    m_pendingControlName = name;
    m_pendingControlValue = valueText;

    // Name on touch: the first movement of a control names it right away
    // ("Deck 1 volume") so the DJ knows what they grabbed; the value
    // follows once it stops moving. Only when the readout is actually
    // changing — a worn pot jittering on its resting value would otherwise
    // chant the name instead of the value — and not in while-moving mode,
    // which already speaks name and value immediately.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool sameControl = key == m_lastControlKey &&
            now - m_lastControlSpokenMs < kControlContextMs;
    if (sameControl) {
        // Active movement keeps the spoken context alive: a long slow drag
        // must not re-announce the name halfway through.
        m_lastControlSpokenMs = now;
    } else if (!m_settings.getAnnounceWhileMoving() &&
            valueText != m_lastValueByKey.value(key)) {
        speak(name);
        // speak() clears the control context; restore it so the resting
        // value is spoken without repeating the name.
        m_lastControlKey = key;
        m_lastControlSpokenMs = now;
    }
    startControlDebounce();
}

void AnnouncementManager::startControlDebounce() {
    // While-moving mode speaks immediately, throttled so rapid knob sweeps
    // don't queue an utterance per tick; the debounce timer still fires
    // afterwards so the final resting value is always spoken.
    if (m_settings.getAnnounceWhileMoving()) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastMovingSpeakMs >= kMovingThrottleMs) {
            m_lastMovingSpeakMs = now;
            slotAnnouncePendingControl();
            return;
        }
    }
    m_controlDebounce.start();
}

void AnnouncementManager::slotAnnouncePendingControl() {
    if (m_pendingControlKey.isEmpty()) {
        if (!m_pendingControlText.isEmpty()) {
            speak(m_pendingControlText);
            m_pendingControlText.clear();
        }
        return;
    }
    const QString key = m_pendingControlKey;
    const QString name = m_pendingControlName;
    const QString value = m_pendingControlValue;
    m_pendingControlKey.clear();
    m_pendingControlName.clear();
    m_pendingControlValue.clear();

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool sameControl = key == m_lastControlKey &&
            now - m_lastControlSpokenMs < kControlContextMs;
    if (value == m_lastValueByKey.value(key)) {
        // The control settled on the same readout it last announced (a
        // jittery pot does this constantly; so does nudging a control
        // that's already where you want it): stay quiet, but keep the
        // context fresh so a real change still gets the short value-only
        // announcement.
        if (sameControl) {
            m_lastControlSpokenMs = now;
        }
        return;
    }
    const QString fullText = name + QStringLiteral(" ") + value;
    speak(sameControl ? value : fullText);
    // speak() clears the control context (any unrelated announcement
    // invalidates it); restore it, and let the repeat key re-speak the full
    // text even when only the value was said.
    m_lastSpoken = fullText;
    m_lastControlKey = key;
    m_lastValueByKey.insert(key, value);
    m_lastControlSpokenMs = now;
}

void AnnouncementManager::noteTrackChanged(const QString& group) {
    m_lastTrackChangeMs[group] = QDateTime::currentMSecsSinceEpoch();
}

bool AnnouncementManager::recentTrackChange(const QString& group) const {
    const qint64 last = m_lastTrackChangeMs.value(group, -1);
    return last >= 0 &&
            QDateTime::currentMSecsSinceEpoch() - last < kHotcueSuppressMs;
}

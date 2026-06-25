#include "util/announcementmanagerenhanced.h"

#include "control/controlproxy.h"
#include "engine/enginetts.h"
#include "library/library.h"
#include "library/library_decl.h"
#include "mixer/basetrackplayer.h"
#include "mixer/playermanager.h"
#include "moc_announcementmanagerenhanced.cpp"
#include "proto/keys.pb.h"
#include "track/keyutils.h"
#include "track/track.h"
#include "util/parented_ptr.h"
#include "util/ttsengine.h"

namespace {
constexpr int kSelectionDebounceMs = 400;
constexpr int kSearchDebounceMs = 600;

// Returns a fully-spelled pronounceable key name for the given ChromaticKey,
// e.g. A_MINOR → "A Minor", F#_MAJOR → "F Sharp Major".
// Using a lookup table keyed by the enum integer (INVALID=0, C_MAJOR=1 … B_MINOR=24).
QString keyForSpeech(mixxx::track::io::key::ChromaticKey key) {
    using namespace mixxx::track::io::key;
    static const QString kNames[] = {
            QString(),                       // 0  INVALID
            QStringLiteral("C Major"),       // 1
            QStringLiteral("D Flat Major"),  // 2
            QStringLiteral("D Major"),       // 3
            QStringLiteral("E Flat Major"),  // 4
            QStringLiteral("E Major"),       // 5
            QStringLiteral("F Major"),       // 6
            QStringLiteral("F Sharp Major"), // 7
            QStringLiteral("G Major"),       // 8
            QStringLiteral("A Flat Major"),  // 9
            QStringLiteral("A Major"),       // 10
            QStringLiteral("B Flat Major"),  // 11
            QStringLiteral("B Major"),       // 12
            QStringLiteral("C Minor"),       // 13
            QStringLiteral("C Sharp Minor"), // 14
            QStringLiteral("D Minor"),       // 15
            QStringLiteral("E Flat Minor"),  // 16
            QStringLiteral("E Minor"),       // 17
            QStringLiteral("F Minor"),       // 18
            QStringLiteral("F Sharp Minor"), // 19
            QStringLiteral("G Minor"),       // 20
            QStringLiteral("A Flat Minor"),  // 21
            QStringLiteral("A Minor"),       // 22
            QStringLiteral("B Flat Minor"),  // 23
            QStringLiteral("B Minor"),       // 24
    };
    const int idx = static_cast<int>(key);
    if (idx < 0 || idx >= static_cast<int>(std::size(kNames))) {
        return {};
    }
    return kNames[idx];
}

// Format parameter value for speech with appropriate units
QString formatParamValue(double value, const QString& paramName) {
    if (paramName.contains("gain", Qt::CaseInsensitive) ||
            paramName.contains("volume", Qt::CaseInsensitive)) {
        return QString::number(value, 'f', 1) + " dB";
    }
    if (paramName.contains("freq", Qt::CaseInsensitive) ||
            paramName.contains("hz", Qt::CaseInsensitive)) {
        return QString::number(value, 'f', 0) + " Hertz";
    }
    if (paramName.contains("q", Qt::CaseInsensitive)) {
        return QString::number(value, 'f', 2);
    }
    if (paramName.contains("tempo", Qt::CaseInsensitive) ||
            paramName.contains("bpm", Qt::CaseInsensitive)) {
        return QString::number(value, 'f', 0) + " beats per minute";
    }
    if (paramName.contains("pan", Qt::CaseInsensitive)) {
        if (value < 0) {
            return QString::number(-value, 'f', 0) + " left";
        } else if (value > 0) {
            return QString::number(value, 'f', 0) + " right";
        } else {
            return "center";
        }
    }
    return QString::number(value, 'f', 2);
}

} // namespace

// static
std::unique_ptr<EnhancedAnnouncementManager> EnhancedAnnouncementManager::create(
        Library* pLibrary,
        PlayerManagerInterface* pPlayerManager,
        UserSettingsPointer pConfig,
        EngineTts* pTtsSink,
        QObject* parent) {
    return std::make_unique<EnhancedAnnouncementManager>(
            pLibrary, pPlayerManager, std::move(pConfig), TtsEngine::create(), pTtsSink, parent);
}

EnhancedAnnouncementManager::EnhancedAnnouncementManager(
        Library* pLibrary,
        PlayerManagerInterface* pPlayerManager,
        UserSettingsPointer pConfig,
        std::unique_ptr<TtsEngine> pTts,
        EngineTts* pTtsSink,
        QObject* parent)
        : QObject(parent),
          m_pTts(std::move(pTts)),
          m_pTtsSink(pTtsSink),
          m_settings(pConfig),
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
    }
    init(pLibrary, pPlayerManager);
}

void EnhancedAnnouncementManager::init(Library* pLibrary, PlayerManagerInterface* pPlayerManager) {
    m_selectionDebounce.setSingleShot(true);
    m_selectionDebounce.setInterval(kSelectionDebounceMs);
    connect(&m_selectionDebounce,
            &QTimer::timeout,
            this,
            &EnhancedAnnouncementManager::slotAnnounceSelectedTrack);

    m_searchDebounce.setSingleShot(true);
    m_searchDebounce.setInterval(kSearchDebounceMs);
    connect(&m_searchDebounce,
            &QTimer::timeout,
            this,
            &EnhancedAnnouncementManager::slotAnnounceSearch);

    if (pLibrary) {
        connect(pLibrary,
                &Library::trackSelected,
                this,
                &EnhancedAnnouncementManager::slotTrackSelected);
        connect(pLibrary,
                &Library::sidebarItemActivated,
                this,
                &EnhancedAnnouncementManager::slotSidebarItemActivated);
        connect(pLibrary,
                &Library::search,
                this,
                &EnhancedAnnouncementManager::slotSearchTextChanged);
    }

    connect(pPlayerManager,
            &PlayerManagerInterface::numberOfDecksChanged,
            this,
            &EnhancedAnnouncementManager::slotNumberOfDecksChanged);

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
    pFocusedWidget->connectValueChanged(
            this, &EnhancedAnnouncementManager::slotLibraryFocusChanged);
}

EnhancedAnnouncementManager::~EnhancedAnnouncementManager() = default;

void EnhancedAnnouncementManager::speak(const QString& text) {
    // Skip if TTS is disabled via the user toggle.
    if (m_pTtsSink && !m_pTtsSink->isUserEnabled()) {
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
    m_pTts->say(text);
}

void EnhancedAnnouncementManager::connectGroupControls(const QString& group) {
    auto pPlay = make_parented<ControlProxy>(group, QStringLiteral("play"), this);
    pPlay->connectValueChanged(this, [this, group](double value) {
        const bool nowPlaying = value > 0.0;
        const bool wasPlaying = m_deckIsPlaying.value(group, false);
        const bool hasTrack = m_deckHasTrack.value(group, false);

        if (nowPlaying && !wasPlaying && hasTrack && m_settings.getAnnouncePlay()) {
            speak(QStringLiteral("Playing"));
        } else if (!nowPlaying && wasPlaying && m_settings.getAnnounceStop()) {
            // Suppress the stop announcement when end-of-track fired it —
            // the end-of-track announcement already covered this transition.
            const bool atEnd = ControlProxy(group,
                    QStringLiteral("end_of_track"),
                    nullptr,
                    ControlFlag::AllowMissingOrInvalid)
                                       .toBool();
            if (!atEnd) {
                speak(QStringLiteral("Stopped"));
            }
        }
        m_deckIsPlaying[group] = nowPlaying;
    });

    auto pEndOfTrack = make_parented<ControlProxy>(
            group, QStringLiteral("end_of_track"), this, ControlFlag::AllowMissingOrInvalid);
    pEndOfTrack->connectValueChanged(this, [this](double value) {
        if (value > 0.0 && m_settings.getAnnounceEndOfTrack()) {
            speak(QStringLiteral("End of track"));
        }
    });

    auto pPfl = make_parented<ControlProxy>(
            group, QStringLiteral("pfl"), this, ControlFlag::AllowMissingOrInvalid);
    pPfl->connectValueChanged(this, [this](double value) {
        if (m_settings.getAnnounceCue()) {
            speak(value > 0.0 ? QStringLiteral("Cue") : QStringLiteral("Cue off"));
        }
    });

    // Additional controls for enhanced announcements
    auto pFader = make_parented<ControlProxy>(
            group, QStringLiteral("volume"), this, ControlFlag::AllowMissingOrInvalid);
    pFader->connectValueChanged(this, [this, group](double value) {
        // Track fader state changes for announcements
        const double lastValue = m_lastFaderValues.value(group, 0.0);
        if (qAbs(value - lastValue) > 0.01 && m_settings.getAnnounceFaderChange()) {
            speak(formatFaderState(group, value));
        }
        m_lastFaderValues[group] = value;
    });
}

void EnhancedAnnouncementManager::setDeckHasTrack(const QString& group, bool value) {
    m_deckHasTrack[group] = value;
}

void EnhancedAnnouncementManager::connectDeck(int deckIndex) {
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
    });
    connect(pDeck, &BaseTrackPlayer::trackUnloaded, this, [this, group](TrackPointer) {
        m_deckHasTrack[group] = false;
        m_deckIsPlaying[group] = false;
    });

    connectGroupControls(group);
}

void EnhancedAnnouncementManager::slotNumberOfDecksChanged(int decks) {
    for (int i = m_connectedDecks; i < decks; ++i) {
        connectDeck(i);
    }
    m_connectedDecks = decks;
}

void EnhancedAnnouncementManager::slotTrackSelected(TrackPointer pTrack) {
    // Only announce selection when the user is actively browsing the track list.
    // If focus is on the sidebar, the signal fires for the first track in the
    // newly-loaded feature view — not something the user deliberately selected.
    if (m_lastFocusWidget != FocusWidget::TracksTable) {
        return;
    }
    m_pendingTrack = pTrack;
    m_selectionDebounce.start();
}

void EnhancedAnnouncementManager::slotAnnounceSelectedTrack() {
    if (m_pendingTrack && m_settings.getAnnounceTrackSelection()) {
        speak(formatForBrowsing(m_pendingTrack));
    }
}

void EnhancedAnnouncementManager::slotNewTrackLoaded(TrackPointer pTrack, int deckIndex) {
    if (pTrack && m_settings.getAnnounceTrackLoad()) {
        speak(formatForLoad(pTrack, deckIndex));
    }
}

// static
QString EnhancedAnnouncementManager::formatForBrowsing(TrackPointer pTrack) {
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

void EnhancedAnnouncementManager::slotSkinLoaded() {
    if (m_settings.getAnnounceStartup()) {
        speak(QStringLiteral("Mixxx ready"));
    }
}

void EnhancedAnnouncementManager::slotSidebarItemActivated(const QString& title) {
    if (!m_settings.getAnnounceLibraryFocus() || title.isEmpty()) {
        return;
    }
    // Deduplicate: currentChanged and featureSelect can both fire for the same
    // item on a mouse click.
    if (title == m_lastAnnouncedSidebarItem) {
        return;
    }
    m_lastAnnouncedSidebarItem = title;
    speak(title);
}

void EnhancedAnnouncementManager::slotLibraryFocusChanged(double value) {
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
        speak(QStringLiteral("Search bar"));
        break;
    case FocusWidget::Sidebar:
        speak(QStringLiteral("Sidebar"));
        break;
    case FocusWidget::TracksTable:
        speak(QStringLiteral("Track list"));
        break;
    default:
        break;
    }
}

void EnhancedAnnouncementManager::slotSearchTextChanged(const QString& text) {
    m_pendingSearch = text;
    m_searchDebounce.start();
}

void EnhancedAnnouncementManager::slotAnnounceSearch() {
    if (!m_settings.getAnnounceSearch()) {
        return;
    }
    if (m_pendingSearch.isEmpty()) {
        speak(QStringLiteral("Search cleared"));
    } else {
        speak(QStringLiteral("Searching: ") + m_pendingSearch);
    }
}

// static
QString EnhancedAnnouncementManager::formatForLoad(TrackPointer pTrack, int deckIndex) {
    const QString artist = pTrack->getArtist().trimmed();
    const QString title = pTrack->getTitle().trimmed();
    const double bpm = pTrack->getBpm();
    const QString keyText = keyForSpeech(pTrack->getKey());

    // "B P M" with spaces causes TTS engines to read each letter individually
    // rather than trying to pronounce it as a word.
    QStringList parts;
    parts << QStringLiteral("Loaded ") + QChar(u'A' + deckIndex);
    if (!artist.isEmpty()) {
        parts << artist;
    }
    if (!title.isEmpty()) {
        parts << title;
    }
    if (bpm > 0.0) {
        parts << QString::number(static_cast<int>(bpm + 0.5)) + QStringLiteral(" B P M");
    }
    if (!keyText.isEmpty()) {
        parts << QStringLiteral("Key: ") + keyText;
    }
    return parts.join(QStringLiteral(". ")) + QStringLiteral(".");
}

// Enhanced announcement slots
void EnhancedAnnouncementManager::slotAnnounceEqParamChanged(
        const QString& group, const QString& paramName, double value) {
    if (m_settings.getAnnounceEq()) {
        speak(formatEqParam(group, paramName, value));
    }
}

void EnhancedAnnouncementManager::slotAnnounceFilterParamChanged(
        const QString& group, const QString& paramName, double value) {
    if (m_settings.getAnnounceFilter()) {
        speak(formatFilterParam(group, paramName, value));
    }
}

void EnhancedAnnouncementManager::slotAnnounceTrimParamChanged(
        const QString& group, const QString& paramName, double value) {
    if (m_settings.getAnnounceTrim()) {
        speak(formatTrimParam(group, paramName, value));
    }
}

void EnhancedAnnouncementManager::slotAnnounceMasterParamChanged(
        const QString& group, const QString& paramName, double value) {
    if (m_settings.getAnnounceMaster()) {
        speak(formatMasterParam(group, paramName, value));
    }
}

void EnhancedAnnouncementManager::slotAnnounceMixParamChanged(
        const QString& group, const QString& paramName, double value) {
    if (m_settings.getAnnounceMix()) {
        speak(formatMixParam(group, paramName, value));
    }
}

void EnhancedAnnouncementManager::slotAnnounceEffectSelected(const QString& effectId) {
    if (m_settings.getAnnounceEffect()) {
        speak(QStringLiteral("Effect ") + effectId + QStringLiteral(" selected"));
    }
}

void EnhancedAnnouncementManager::slotAnnounceSyncStatusChanged(bool enabled) {
    if (m_settings.getAnnounceSync()) {
        speak(enabled ? QStringLiteral("Sync enabled") : QStringLiteral("Sync disabled"));
    }
}

void EnhancedAnnouncementManager::slotAnnounceTempoSliderChanged(double value) {
    if (m_settings.getAnnounceTempo()) {
        speak(formatTempoSlider(value));
    }
}

void EnhancedAnnouncementManager::slotAnnounceCrossFaderChanged(double value) {
    if (m_settings.getAnnounceCrossFader()) {
        speak(formatCrossFader(value));
    }
}

void EnhancedAnnouncementManager::slotAnnounceFaderStateChanged(
        const QString& group, double value) {
    if (m_settings.getAnnounceFaderChange()) {
        speak(formatFaderState(group, value));
    }
}

void EnhancedAnnouncementManager::slotAnnouncePreventJoggingStateChanged(bool enabled) {
    m_preventJoggingEnabled = enabled;
    if (m_settings.getAnnouncePreventJogging()) {
        speak(enabled ? QStringLiteral("Prevent jogging enabled")
                      : QStringLiteral("Prevent jogging disabled"));
    }
}

void EnhancedAnnouncementManager::slotAnnounceTouchSurfaceState(bool enabled) {
    m_touchSurfaceEnabled = enabled;
    if (m_settings.getAnnounceTouchSurface()) {
        speak(enabled ? QStringLiteral("Touch surface enabled")
                      : QStringLiteral("Touch surface disabled"));
    }
}

void EnhancedAnnouncementManager::slotTtsToggled(bool enabled) {
    // Handle TTS being toggled on/off
    if (m_settings.getAnnounceTtsToggle()) {
        speak(enabled ? QStringLiteral("Text-to-speech enabled")
                      : QStringLiteral("Text-to-speech disabled"));
    }
}

// Format helpers
QString EnhancedAnnouncementManager::formatEqParam(
        const QString& group, const QString& paramName, double value) {
    QString deckName = group;
    if (group.startsWith("[Channel")) {
        // Extract deck number from group
        QRegularExpression regex(R"(\[Channel(\d+)\])");
        auto match = regex.match(group);
        if (match.hasMatch()) {
            deckName = "Deck " + match.captured(1);
        }
    }
    return deckName + " EQ " + paramName + " " + formatParamValue(value, paramName);
}

QString EnhancedAnnouncementManager::formatFilterParam(
        const QString& group, const QString& paramName, double value) {
    QString deckName = group;
    if (group.startsWith("[Channel")) {
        QRegularExpression regex(R"(\[Channel(\d+)\])");
        auto match = regex.match(group);
        if (match.hasMatch()) {
            deckName = "Deck " + match.captured(1);
        }
    }
    return deckName + " Filter " + paramName + " " + formatParamValue(value, paramName);
}

QString EnhancedAnnouncementManager::formatTrimParam(
        const QString& group, const QString& paramName, double value) {
    QString deckName = group;
    if (group.startsWith("[Channel")) {
        QRegularExpression regex(R"(\[Channel(\d+)\])");
        auto match = regex.match(group);
        if (match.hasMatch()) {
            deckName = "Deck " + match.captured(1);
        }
    }
    return deckName + " Trim " + paramName + " " + formatParamValue(value, paramName);
}

QString EnhancedAnnouncementManager::formatMasterParam(
        const QString& group, const QString& paramName, double value) {
    return "Master " + paramName + " " + formatParamValue(value, paramName);
}

QString EnhancedAnnouncementManager::formatMixParam(
        const QString& group, const QString& paramName, double value) {
    return "Mix " + paramName + " " + formatParamValue(value, paramName);
}

QString EnhancedAnnouncementManager::formatTempoSlider(double value) {
    // Convert to percentage or BPM depending on setting
    if (value > -1.0 && value < 1.0) {
        return "Tempo " + QString::number(value * 100, 'f', 0) + " percent";
    } else {
        return "Tempo " + QString::number(value, 'f', 0) + " beats per minute";
    }
}

QString EnhancedAnnouncementManager::formatCrossFader(double value) {
    if (value < 0) {
        return "Crossfader " + QString::number(-value * 100, 'f', 0) + " percent left";
    } else if (value > 0) {
        return "Crossfader " + QString::number(value * 100, 'f', 0) + " percent right";
    } else {
        return "Crossfader center";
    }
}

QString EnhancedAnnouncementManager::formatFaderState(const QString& group, double value) {
    QString deckName = group;
    if (group.startsWith("[Channel")) {
        QRegularExpression regex(R"(\[Channel(\d+)\])");
        auto match = regex.match(group);
        if (match.hasMatch()) {
            deckName = "Deck " + match.captured(1);
        }
    }
    return deckName + " fader " + QString::number(value * 100, 'f', 0) + " percent";
}

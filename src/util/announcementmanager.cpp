#include "util/announcementmanager.h"

#include <algorithm>
#include <cmath>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "engine/enginetts.h"
#include "library/library.h"
#include "library/library_decl.h"
#include "mixer/basetrackplayer.h"
#include "mixer/playermanager.h"
#include "moc_announcementmanager.cpp"
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
} // namespace

// static
std::unique_ptr<AnnouncementManager> AnnouncementManager::create(
        Library* pLibrary,
        PlayerManagerInterface* pPlayerManager,
        UserSettingsPointer pConfig,
        EngineTts* pTtsSink,
        QObject* parent) {
    return std::make_unique<AnnouncementManager>(
            pLibrary, pPlayerManager, std::move(pConfig), TtsEngine::create(), pTtsSink, parent);
}

AnnouncementManager::AnnouncementManager(
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
                &Library::search,
                this,
                &AnnouncementManager::slotSearchTextChanged);
    }

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

    // Repeat the last announcement on demand (mapped to Alt+Shift+R). A blind
    // user who missed an announcement can re-hear it instead of guessing.
    m_pRepeatButton = std::make_unique<ControlObject>(
            ConfigKey(QStringLiteral("[Tts]"), QStringLiteral("repeat")));
    connect(m_pRepeatButton.get(),
            &ControlObject::valueChanged,
            this,
            [this](double value) {
                if (value > 0.0 && !m_lastSpoken.isEmpty()) {
                    speak(m_lastSpoken);
                }
            });
}

AnnouncementManager::~AnnouncementManager() = default;

void AnnouncementManager::speak(const QString& text) {
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
    m_lastSpoken = text;
    m_pTts->say(text);
}

void AnnouncementManager::connectGroupControls(const QString& group, int deckIndex) {
    // On-demand status readout: pressing the mapped key sets this CO and the
    // deck's current state is spoken.
    auto pStatus = std::make_unique<ControlObject>(
            ConfigKey(group, QStringLiteral("tts_status")));
    connect(pStatus.get(),
            &ControlObject::valueChanged,
            this,
            [this, group, deckIndex](double value) {
                if (value > 0.0) {
                    speak(formatDeckStatus(group, deckIndex));
                }
            });
    m_pStatusButtons.push_back(std::move(pStatus));

    auto pPlay = make_parented<ControlProxy>(group, QStringLiteral("play"), this);
    pPlay->connectValueChanged(this, [this, group](double value) {
        const bool nowPlaying = value > 0.0;
        const bool wasPlaying = m_deckIsPlaying.value(group, false);
        const bool hasTrack = m_deckHasTrack.value(group, false);

        if (nowPlaying && !wasPlaying && hasTrack && m_settings.getAnnouncePlay()) {
            speak(tr("Playing"));
        } else if (!nowPlaying && wasPlaying && m_settings.getAnnounceStop()) {
            // Suppress the stop announcement when end-of-track fired it —
            // the end-of-track announcement already covered this transition.
            const bool atEnd = ControlProxy(group, QStringLiteral("end_of_track"),
                                       nullptr,
                                       ControlFlag::AllowMissingOrInvalid)
                                       .toBool();
            if (!atEnd) {
                speak(tr("Stopped"));
            }
        }
        m_deckIsPlaying[group] = nowPlaying;
    });

    auto pEndOfTrack = make_parented<ControlProxy>(
            group, QStringLiteral("end_of_track"), this, ControlFlag::AllowMissingOrInvalid);
    pEndOfTrack->connectValueChanged(this, [this](double value) {
        if (value > 0.0 && m_settings.getAnnounceEndOfTrack()) {
            speak(tr("End of track"));
        }
    });

    auto pPfl = make_parented<ControlProxy>(
            group, QStringLiteral("pfl"), this, ControlFlag::AllowMissingOrInvalid);
    pPfl->connectValueChanged(this, [this](double value) {
        if (m_settings.getAnnounceCue()) {
            // "Headphone cue", not just "Cue": a DJ would otherwise confuse
            // this with the transport cue button or hotcues.
            speak(value > 0.0 ? tr("Headphone cue on") : tr("Headphone cue off"));
        }
    });
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
    });
    connect(pDeck, &BaseTrackPlayer::trackUnloaded, this, [this, group](TrackPointer) {
        m_deckHasTrack[group] = false;
        m_deckIsPlaying[group] = false;
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

void AnnouncementManager::slotSidebarItemActivated(const QString& title) {
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
    m_searchDebounce.start();
}

void AnnouncementManager::slotAnnounceSearch() {
    if (!m_settings.getAnnounceSearch()) {
        return;
    }
    if (m_pendingSearch.isEmpty()) {
        speak(tr("Search cleared"));
    } else {
        speak(tr("Searching: %1").arg(m_pendingSearch));
    }
}

// static
QString AnnouncementManager::formatForLoad(TrackPointer pTrack, int deckIndex) {
    const QString artist = pTrack->getArtist().trimmed();
    const QString title = pTrack->getTitle().trimmed();
    const double bpm = pTrack->getBpm();
    const QString keyText = keyForSpeech(pTrack->getKey());

    // "B P M" with spaces causes TTS engines to read each letter individually
    // rather than trying to pronounce it as a word.
    QStringList parts;
    parts << tr("Loaded %1").arg(QChar(u'A' + deckIndex));
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
    const QString deckName = deckIndex >= 0
            ? tr("Deck %1").arg(QChar(u'A' + deckIndex))
            : group;
    if (!m_deckHasTrack.value(group, false)) {
        return tr("%1. No track loaded.").arg(deckName);
    }

    auto readControl = [&group](const QString& name) {
        return ControlProxy(group, name, nullptr, ControlFlag::AllowMissingOrInvalid).get();
    };

    QStringList parts;
    parts << deckName;
    parts << (readControl(QStringLiteral("play")) > 0.0 ? tr("Playing") : tr("Stopped"));

    const double duration = readControl(QStringLiteral("duration"));
    if (duration > 0.0) {
        const double playPos = readControl(QStringLiteral("playposition"));
        const int remaining = std::max(0,
                static_cast<int>(std::lround(duration * (1.0 - playPos))));
        const int minutes = remaining / 60;
        const int seconds = remaining % 60;
        const QString minuteText = minutes == 1
                ? tr("1 minute")
                : tr("%1 minutes").arg(minutes);
        const QString secondText = seconds == 1
                ? tr("1 second")
                : tr("%1 seconds").arg(seconds);
        if (minutes > 0) {
            parts << tr("%1 %2 remaining").arg(minuteText, secondText);
        } else {
            parts << tr("%1 remaining").arg(secondText);
        }
    }

    const double bpm = readControl(QStringLiteral("bpm"));
    if (bpm > 0.0) {
        parts << tr("%1 B P M").arg(static_cast<int>(std::lround(bpm)));
    }

    // Pitch fader, spoken as a percentage deviation from normal speed.
    // Words instead of a sign because TTS engines don't read "+" reliably.
    const double rateRatio = readControl(QStringLiteral("rate_ratio"));
    if (rateRatio > 0.0 && std::abs(rateRatio - 1.0) >= 0.0005) {
        const double percent = std::abs(rateRatio - 1.0) * 100.0;
        QString percentText = QString::number(percent, 'f', 1);
        if (percentText.endsWith(QStringLiteral(".0"))) {
            percentText.chop(2);
        }
        parts << (rateRatio > 1.0
                        ? tr("Pitch up %1 percent").arg(percentText)
                        : tr("Pitch down %1 percent").arg(percentText));
    }
    return parts.join(QStringLiteral(". ")) + QStringLiteral(".");
}

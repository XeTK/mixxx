#include "util/announcementmanager.h"

#include "control/controlproxy.h"
#include "library/library.h"
#include "library/library_decl.h"
#include "mixer/basetrackplayer.h"
#include "mixer/playermanager.h"
#include "moc_announcementmanager.cpp"
#include "track/keyutils.h"
#include "track/track.h"
#include "util/parented_ptr.h"
#include "util/ttsengine.h"

namespace {
constexpr int kSelectionDebounceMs = 400;
} // namespace

AnnouncementManager::AnnouncementManager(
        Library* pLibrary,
        PlayerManagerInterface* pPlayerManager,
        UserSettingsPointer pConfig,
        QObject* parent)
        : QObject(parent),
          m_pTts(TtsEngine::create()),
          m_settings(pConfig),
          m_pPlayerManager(pPlayerManager) {
    init(pLibrary, pPlayerManager);
}

AnnouncementManager::AnnouncementManager(
        Library* pLibrary,
        PlayerManagerInterface* pPlayerManager,
        UserSettingsPointer pConfig,
        std::unique_ptr<TtsEngine> pTts,
        QObject* parent)
        : QObject(parent),
          m_pTts(std::move(pTts)),
          m_settings(pConfig),
          m_pPlayerManager(pPlayerManager) {
    init(pLibrary, pPlayerManager);
}

void AnnouncementManager::init(Library* pLibrary, PlayerManagerInterface* pPlayerManager) {
    m_selectionDebounce.setSingleShot(true);
    m_selectionDebounce.setInterval(kSelectionDebounceMs);

    connect(&m_selectionDebounce,
            &QTimer::timeout,
            this,
            &AnnouncementManager::slotAnnounceSelectedTrack);

    if (pLibrary) {
        connect(pLibrary,
                &Library::trackSelected,
                this,
                &AnnouncementManager::slotTrackSelected);
        connect(pLibrary,
                &Library::sidebarItemActivated,
                this,
                &AnnouncementManager::slotSidebarItemActivated);
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
}

AnnouncementManager::~AnnouncementManager() = default;

void AnnouncementManager::speak(const QString& text) {
    const QString deviceId = m_settings.getTtsOutputDevice();
    if (deviceId != m_currentTtsDeviceId) {
        m_pTts->setOutputDevice(deviceId);
        m_currentTtsDeviceId = deviceId;
    }
    m_pTts->say(text);
}

void AnnouncementManager::connectGroupControls(const QString& group) {
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
            const bool atEnd = ControlProxy(group, QStringLiteral("end_of_track"),
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

    connectGroupControls(group);
}

void AnnouncementManager::slotNumberOfDecksChanged(int decks) {
    for (int i = m_connectedDecks; i < decks; ++i) {
        connectDeck(i);
    }
    m_connectedDecks = decks;
}

void AnnouncementManager::slotTrackSelected(TrackPointer pTrack) {
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
        speak(QStringLiteral("Mixxx ready"));
    }
}

void AnnouncementManager::slotSidebarItemActivated(const QString& title) {
    if (m_settings.getAnnounceLibraryFocus()) {
        speak(title);
    }
}

void AnnouncementManager::slotLibraryFocusChanged(double value) {
    if (!m_settings.getAnnounceLibraryFocus()) {
        return;
    }
    switch (static_cast<FocusWidget>(static_cast<int>(value))) {
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

// static
QString AnnouncementManager::formatForLoad(TrackPointer pTrack, int deckIndex) {
    const QString artist = pTrack->getArtist().trimmed();
    const QString title = pTrack->getTitle().trimmed();
    const double bpm = pTrack->getBpm();
    const QString keyText = pTrack->getKeyText().trimmed();

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

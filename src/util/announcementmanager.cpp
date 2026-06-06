#include "util/announcementmanager.h"

#include "library/library.h"
#include "mixer/basetrackplayer.h"
#include "mixer/playermanager.h"
#include "moc_announcementmanager.cpp"
#include "track/keyutils.h"
#include "track/track.h"
#include "util/ttsengine.h"

namespace {
constexpr int kSelectionDebounceMs = 400;
} // namespace

AnnouncementManager::AnnouncementManager(
        Library* pLibrary,
        PlayerManagerInterface* pPlayerManager,
        QObject* parent)
        : QObject(parent),
          m_pTts(TtsEngine::create()),
          m_pPlayerManager(pPlayerManager) {
    m_selectionDebounce.setSingleShot(true);
    m_selectionDebounce.setInterval(kSelectionDebounceMs);

    connect(&m_selectionDebounce,
            &QTimer::timeout,
            this,
            &AnnouncementManager::slotAnnounceSelectedTrack);

    connect(pLibrary,
            &Library::trackSelected,
            this,
            &AnnouncementManager::slotTrackSelected);

    connect(pPlayerManager,
            &PlayerManagerInterface::numberOfDecksChanged,
            this,
            &AnnouncementManager::slotNumberOfDecksChanged);

    // Connect decks that already exist at startup.
    const int numDecks = pPlayerManager->numberOfDecks();
    for (int i = 1; i <= numDecks; ++i) {
        connectDeck(i);
    }
    m_connectedDecks = numDecks;
}

AnnouncementManager::~AnnouncementManager() = default;

void AnnouncementManager::connectDeck(int deckIndex) {
    BaseTrackPlayer* pDeck = m_pPlayerManager->getDeckBase(deckIndex);
    if (!pDeck) {
        return;
    }
    connect(pDeck,
            &BaseTrackPlayer::newTrackLoaded,
            this,
            &AnnouncementManager::slotNewTrackLoaded);
}

void AnnouncementManager::slotNumberOfDecksChanged(int decks) {
    for (int i = m_connectedDecks + 1; i <= decks; ++i) {
        connectDeck(i);
    }
    m_connectedDecks = decks;
}

void AnnouncementManager::slotTrackSelected(TrackPointer pTrack) {
    m_pendingTrack = pTrack;
    m_selectionDebounce.start();
}

void AnnouncementManager::slotAnnounceSelectedTrack() {
    if (m_pendingTrack) {
        m_pTts->say(formatForBrowsing(m_pendingTrack));
    }
}

void AnnouncementManager::slotNewTrackLoaded(TrackPointer pTrack) {
    if (pTrack) {
        m_pTts->say(formatForLoad(pTrack));
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

// static
QString AnnouncementManager::formatForLoad(TrackPointer pTrack) {
    const QString artist = pTrack->getArtist().trimmed();
    const QString title = pTrack->getTitle().trimmed();
    const double bpm = pTrack->getBpm();
    const QString keyText = pTrack->getKeyText().trimmed();

    QString text = QStringLiteral("Loaded. ");
    if (!artist.isEmpty()) {
        text += artist + QStringLiteral(". ");
    }
    if (!title.isEmpty()) {
        text += title + QStringLiteral(". ");
    }
    if (bpm > 0.0) {
        text += QString::number(static_cast<int>(bpm + 0.5)) +
                QStringLiteral(" B P M. ");
    }
    if (!keyText.isEmpty()) {
        text += QStringLiteral("Key: ") + keyText + QStringLiteral(".");
    }
    return text.trimmed();
}

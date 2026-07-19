#include "library/youtube/dlgyoutubecc.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QtDebug>

#include "library/library.h"
#include "library/trackcollectionmanager.h"
#include "library/youtube/youtubeccdownloader.h"
#include "library/youtube/youtubeccsearchtask.h"
#include "moc_dlgyoutubecc.cpp"
#include "track/track.h"
#include "track/trackid.h"
#include "widget/wlibrary.h"

namespace {

const ConfigKey kYtDlpPathConfigKey = ConfigKey("[youtube_cc]", "ytdlp_path");

QString formatDuration(int secs) {
    if (secs <= 0) {
        return QStringLiteral("--:--");
    }
    return QStringLiteral("%1:%2")
            .arg(secs / 60)
            .arg(secs % 60, 2, 10, QChar('0'));
}

QTableWidgetItem* makeReadOnlyItem(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

} // anonymous namespace

DlgYouTubeCc::DlgYouTubeCc(WLibrary* parent, UserSettingsPointer pConfig, Library* pLibrary)
        : QWidget(parent),
          m_pConfig(pConfig),
          m_pLibrary(pLibrary),
          m_pSearchTask(new YouTubeCcSearchTask(this)),
          m_pDownloader(new YouTubeCcDownloader(this)) {
    setupUi();

    connect(m_pSearchTask,
            &YouTubeCcSearchTask::succeeded,
            this,
            &DlgYouTubeCc::slotSearchSucceeded);
    connect(m_pSearchTask,
            &YouTubeCcSearchTask::failed,
            this,
            &DlgYouTubeCc::slotSearchFailed);
    connect(m_pDownloader,
            &YouTubeCcDownloader::progress,
            this,
            &DlgYouTubeCc::slotDownloadProgress);
    connect(m_pDownloader,
            &YouTubeCcDownloader::succeeded,
            this,
            &DlgYouTubeCc::slotDownloadSucceeded);
    connect(m_pDownloader,
            &YouTubeCcDownloader::failed,
            this,
            &DlgYouTubeCc::slotDownloadFailed);
}

void DlgYouTubeCc::setupUi() {
    auto* pMainLayout = new QVBoxLayout(this);

    m_pResults = new QTableWidget(this);
    m_pResults->setColumnCount(3);
    m_pResults->setHorizontalHeaderLabels(
            {tr("Title"), tr("Uploader"), tr("Length")});
    m_pResults->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pResults->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pResults->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pResults->verticalHeader()->setVisible(false);
    m_pResults->horizontalHeader()->setStretchLastSection(false);
    m_pResults->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_pResults->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_pResults->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_pResults->setAlternatingRowColors(true);
    // The Mixxx skins style their own widget classes (WTrackTableView, ...), not
    // a plain QTableWidget, so it would otherwise render with the default light
    // palette. Match the dark library look here.
    m_pResults->setStyleSheet(QStringLiteral(
            "QTableWidget {"
            "  background-color: #141414;"
            "  alternate-background-color: #1b1b1b;"
            "  color: #d0d0d0;"
            "  gridline-color: #2a2a2a;"
            "  border: 1px solid #2a2a2a;"
            "  selection-background-color: #2f5f8f;"
            "  selection-color: #ffffff;"
            "}"
            "QHeaderView::section {"
            "  background-color: #232323;"
            "  color: #d0d0d0;"
            "  padding: 3px;"
            "  border: 0px;"
            "  border-right: 1px solid #2a2a2a;"
            "  border-bottom: 1px solid #2a2a2a;"
            "}"
            "QTableCornerButton::section {"
            "  background-color: #232323;"
            "  border: 0px;"
            "}"));
    pMainLayout->addWidget(m_pResults);

    auto* pBottomRow = new QHBoxLayout();
    m_pStatus = new QLabel(this);
    m_pStatus->setWordWrap(true);
    m_pStatus->setStyleSheet(QStringLiteral("color: #b0b0b0;"));
    m_pProgress = new QProgressBar(this);
    m_pProgress->setRange(0, 100);
    m_pProgress->setVisible(false);
    m_pProgress->setMaximumWidth(200);
    m_pLoadButton = new QPushButton(tr("Download && Load"), this);
    m_pLoadButton->setEnabled(false);
    pBottomRow->addWidget(m_pStatus, 1);
    pBottomRow->addWidget(m_pProgress);
    pBottomRow->addWidget(m_pLoadButton);
    pMainLayout->addLayout(pBottomRow);

    setStatus(tr("Type in the search box above to find Creative Commons music "
                 "on YouTube."));

    connect(m_pResults, &QTableWidget::cellActivated, this, &DlgYouTubeCc::slotResultActivated);
    connect(m_pResults,
            &QTableWidget::itemSelectionChanged,
            this,
            &DlgYouTubeCc::slotSelectionChanged);
    connect(m_pLoadButton, &QPushButton::clicked, this, &DlgYouTubeCc::slotLoadSelected);
}

void DlgYouTubeCc::onShow() {
    m_pResults->setFocus();
}

bool DlgYouTubeCc::hasFocus() const {
    return m_pResults->hasFocus() || m_pLoadButton->hasFocus();
}

void DlgYouTubeCc::setFocus() {
    m_pResults->setFocus();
}

void DlgYouTubeCc::onSearch(const QString& text) {
    const QString query = text.trimmed();
    if (query.isEmpty()) {
        m_pSearchTask->abort();
        m_pProgress->setVisible(false);
        m_currentResults.clear();
        m_pResults->clearContents();
        m_pResults->setRowCount(0);
        setStatus(tr("Type in the search box above to find Creative Commons "
                     "music on YouTube."));
        return;
    }
    setStatus(tr("Searching…"));
    // Range 0,0 makes the progress bar an indeterminate "busy" spinner.
    m_pProgress->setRange(0, 0);
    m_pProgress->setVisible(true);
    m_pSearchTask->setYtDlpPath(ytDlpPath());
    m_pSearchTask->search(query);
}

QString DlgYouTubeCc::ytDlpPath() const {
    const QString path = m_pConfig->getValueString(kYtDlpPathConfigKey);
    return path.isEmpty() ? QStringLiteral("yt-dlp") : path;
}

QString DlgYouTubeCc::cacheDir() const {
    return m_pConfig->getSettingsPath() + QStringLiteral("/youtube_cc_cache");
}

void DlgYouTubeCc::slotSearchSucceeded(const QList<YouTubeCcTrack>& results) {
    m_pProgress->setVisible(false);
    m_currentResults = results;
    m_pResults->clearContents();
    m_pResults->setRowCount(results.size());
    for (int row = 0; row < results.size(); ++row) {
        const YouTubeCcTrack& track = results.at(row);
        m_pResults->setItem(row, 0, makeReadOnlyItem(track.title));
        m_pResults->setItem(row, 1, makeReadOnlyItem(track.channelTitle));
        m_pResults->setItem(row, 2, makeReadOnlyItem(formatDuration(track.durationSecs)));
    }
    if (results.isEmpty()) {
        setStatus(tr("No Creative Commons results found."));
    } else {
        setStatus(tr("%n Creative Commons result(s). Double-click to download "
                     "and load.",
                "",
                results.size()));
    }
}

void DlgYouTubeCc::slotSearchFailed(const QString& message) {
    m_pProgress->setVisible(false);
    setStatus(tr("Search failed: %1").arg(message));
}

void DlgYouTubeCc::slotResultActivated(int row, int column) {
    Q_UNUSED(column);
    startDownload(row);
}

void DlgYouTubeCc::slotLoadSelected() {
    startDownload(m_pResults->currentRow());
}

void DlgYouTubeCc::slotSelectionChanged() {
    m_pLoadButton->setEnabled(m_pResults->currentRow() >= 0);
}

void DlgYouTubeCc::startDownload(int row) {
    if (row < 0 || row >= m_currentResults.size()) {
        return;
    }
    if (m_pDownloader->isBusy()) {
        setStatus(tr("A download is already in progress."));
        return;
    }
    const YouTubeCcTrack& track = m_currentResults.at(row);
    m_pDownloader->setYtDlpPath(ytDlpPath());
    m_pDownloader->setCacheDir(cacheDir());
    // Determinate progress for downloads (search uses indeterminate mode).
    m_pProgress->setRange(0, 100);
    m_pProgress->setValue(0);
    m_pProgress->setVisible(true);
    m_pLoadButton->setEnabled(false);
    setStatus(tr("Downloading \"%1\"…").arg(track.title));
    m_pDownloader->download(track);
}

void DlgYouTubeCc::slotDownloadProgress(const QString& videoId, int percent) {
    Q_UNUSED(videoId);
    m_pProgress->setValue(percent);
}

void DlgYouTubeCc::slotDownloadSucceeded(const YouTubeCcTrack& track, const QString& localPath) {
    m_pProgress->setVisible(false);
    m_pLoadButton->setEnabled(m_pResults->currentRow() >= 0);

    TrackCollectionManager* pTcm = m_pLibrary->trackCollectionManager();
    const QList<TrackId> ids = pTcm->resolveTrackIdsFromLocations({localPath});
    if (ids.isEmpty()) {
        setStatus(tr("Could not add the downloaded file to the library."));
        return;
    }
    TrackPointer pTrack = pTcm->getTrackById(ids.first());
    if (!pTrack) {
        setStatus(tr("Could not load the downloaded track."));
        return;
    }

    // Fill in metadata from the YouTube result if the file itself carried none,
    // and always record attribution (Creative Commons requires it).
    if (pTrack->getArtist().trimmed().isEmpty()) {
        pTrack->setArtist(track.channelTitle);
    }
    if (pTrack->getTitle().trimmed().isEmpty()) {
        pTrack->setTitle(track.title);
    }
    pTrack->setComment(
            tr("Source: %1 | Uploader: %2 | License: Creative Commons (CC BY)")
                    .arg(track.watchUrl().toString(), track.channelTitle));

    setStatus(tr("Loaded \"%1\".").arg(pTrack->getTitle()));
    emit downloaded();
    emit loadTrack(pTrack);
}

void DlgYouTubeCc::slotDownloadFailed(const QString& videoId, const QString& message) {
    Q_UNUSED(videoId);
    m_pProgress->setVisible(false);
    m_pLoadButton->setEnabled(m_pResults->currentRow() >= 0);
    setStatus(tr("Download failed: %1").arg(message));
}

void DlgYouTubeCc::setStatus(const QString& message) {
    m_pStatus->setText(message);
}

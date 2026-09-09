#include "library/youtube/dlgyoutubecc.h"

#include <QAbstractItemView>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPair>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QTabWidget>
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

void configureResultTable(QTableWidget* table, const QStringList& headers) {
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int col = 1; col < headers.size(); ++col) {
        table->horizontalHeader()->setSectionResizeMode(
                col, QHeaderView::ResizeToContents);
    }
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

    auto* pSearchRow = new QHBoxLayout();
    m_pSearchEdit = new QLineEdit(this);
    m_pSearchEdit->setPlaceholderText(
            tr("Search Creative Commons music on YouTube…"));
    m_pSearchButton = new QPushButton(tr("Search"), this);
    pSearchRow->addWidget(m_pSearchEdit);
    pSearchRow->addWidget(m_pSearchButton);
    pMainLayout->addLayout(pSearchRow);

    m_pTabs = new QTabWidget(this);

    m_pResults = new QTableWidget(this);
    configureResultTable(m_pResults, {tr("Title"), tr("Uploader"), tr("Length")});
    m_pTabs->addTab(m_pResults, tr("Search results"));

    m_pDownloaded = new QTableWidget(this);
    configureResultTable(m_pDownloaded, {tr("Title"), tr("YouTube ID")});
    m_pTabs->addTab(m_pDownloaded, tr("Downloaded"));

    pMainLayout->addWidget(m_pTabs);

    auto* pBottomRow = new QHBoxLayout();
    m_pStatus = new QLabel(this);
    m_pStatus->setWordWrap(true);
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

    setStatus(tr("Enter a search term. All results are licensed under "
                 "Creative Commons."));

    connect(m_pSearchButton, &QPushButton::clicked, this, &DlgYouTubeCc::slotSearchClicked);
    connect(m_pSearchEdit, &QLineEdit::returnPressed, this, &DlgYouTubeCc::slotSearchClicked);
    connect(m_pResults, &QTableWidget::cellActivated, this, &DlgYouTubeCc::slotResultActivated);
    connect(m_pDownloaded, &QTableWidget::cellActivated, this, &DlgYouTubeCc::slotDownloadedActivated);
    connect(m_pResults,
            &QTableWidget::itemSelectionChanged,
            this,
            &DlgYouTubeCc::updateLoadButtonState);
    connect(m_pDownloaded,
            &QTableWidget::itemSelectionChanged,
            this,
            &DlgYouTubeCc::updateLoadButtonState);
    connect(m_pTabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 1) {
            refreshDownloaded();
        }
        updateLoadButtonState();
    });
    connect(m_pLoadButton, &QPushButton::clicked, this, &DlgYouTubeCc::slotLoadSelected);
}

void DlgYouTubeCc::onShow() {
    refreshDownloaded();
    m_pSearchEdit->setFocus();
}

bool DlgYouTubeCc::hasFocus() const {
    return m_pSearchEdit->hasFocus() || m_pResults->hasFocus() ||
            m_pDownloaded->hasFocus() || m_pSearchButton->hasFocus() ||
            m_pLoadButton->hasFocus();
}

void DlgYouTubeCc::setFocus() {
    m_pSearchEdit->setFocus();
}

QString DlgYouTubeCc::ytDlpPath() const {
    const QString path = m_pConfig->getValueString(kYtDlpPathConfigKey);
    return path.isEmpty() ? QStringLiteral("yt-dlp") : path;
}

QString DlgYouTubeCc::cacheDir() const {
    return m_pConfig->getSettingsPath() + QStringLiteral("/youtube_cc_cache");
}

void DlgYouTubeCc::slotSearchClicked() {
    const QString query = m_pSearchEdit->text().trimmed();
    if (query.isEmpty()) {
        return;
    }
    setStatus(tr("Searching…"));
    m_pSearchButton->setEnabled(false);
    m_pSearchTask->setYtDlpPath(ytDlpPath());
    m_pSearchTask->search(query);
}

void DlgYouTubeCc::slotSearchSucceeded(const QList<YouTubeCcTrack>& results) {
    m_pSearchButton->setEnabled(true);
    m_pTabs->setCurrentIndex(0);
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
    m_pSearchButton->setEnabled(true);
    setStatus(tr("Search failed: %1").arg(message));
}

void DlgYouTubeCc::slotResultActivated(int row, int column) {
    Q_UNUSED(column);
    startDownload(row);
}

void DlgYouTubeCc::slotDownloadedActivated(int row, int column) {
    Q_UNUSED(column);
    if (row < 0 || row >= m_downloadedPaths.size()) {
        return;
    }
    loadCachedPath(m_downloadedPaths.at(row));
}

void DlgYouTubeCc::slotLoadSelected() {
    if (m_pTabs->currentIndex() == 1) {
        slotDownloadedActivated(m_pDownloaded->currentRow(), 0);
    } else {
        startDownload(m_pResults->currentRow());
    }
}

void DlgYouTubeCc::updateLoadButtonState() {
    const bool onDownloaded = (m_pTabs->currentIndex() == 1);
    m_pLoadButton->setText(onDownloaded ? tr("Load") : tr("Download && Load"));
    QTableWidget* pActive = onDownloaded ? m_pDownloaded : m_pResults;
    m_pLoadButton->setEnabled(pActive->currentRow() >= 0);
}

void DlgYouTubeCc::refreshDownloaded() {
    // Filename template is "<title> [<videoId>].<ext>" (see YouTubeCcDownloader).
    static const QRegularExpression reName(
            QStringLiteral("^(.*) \\[([A-Za-z0-9_-]{11})\\]\\.[^.]+$"));

    m_downloadedPaths.clear();
    m_pDownloaded->clearContents();

    const QFileInfoList files =
            QDir(cacheDir()).entryInfoList(QDir::Files, QDir::Time);
    QList<QPair<QString, QString>> rows; // (title, videoId)
    for (const QFileInfo& info : files) {
        const QString name = info.fileName();
        // Skip yt-dlp's in-progress/temp artifacts.
        if (name.endsWith(QStringLiteral(".part")) ||
                name.endsWith(QStringLiteral(".ytdl")) ||
                name.endsWith(QStringLiteral(".temp"))) {
            continue;
        }
        const QRegularExpressionMatch m = reName.match(name);
        QString title;
        QString videoId;
        if (m.hasMatch()) {
            title = m.captured(1);
            videoId = m.captured(2);
        } else {
            title = info.completeBaseName();
        }
        rows.append({title, videoId});
        m_downloadedPaths.append(info.absoluteFilePath());
    }

    m_pDownloaded->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        m_pDownloaded->setItem(row, 0, makeReadOnlyItem(rows.at(row).first));
        m_pDownloaded->setItem(row, 1, makeReadOnlyItem(rows.at(row).second));
    }
    m_pTabs->setTabText(1, tr("Downloaded (%1)").arg(rows.size()));
}

void DlgYouTubeCc::loadCachedPath(const QString& path) {
    TrackCollectionManager* pTcm = m_pLibrary->trackCollectionManager();
    const QList<TrackId> ids = pTcm->resolveTrackIdsFromLocations({path});
    if (ids.isEmpty()) {
        setStatus(tr("Could not add the file to the library."));
        return;
    }
    TrackPointer pTrack = pTcm->getTrackById(ids.first());
    if (!pTrack) {
        setStatus(tr("Could not load the track."));
        return;
    }
    setStatus(tr("Loaded \"%1\".").arg(pTrack->getTitle()));
    emit loadTrack(pTrack);
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
    refreshDownloaded();
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

#pragma once

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <functional>

#include "util/parented_ptr.h"
#include "util/performancetimer.h"

class LibraryScannerDlg : public QDialog {
    Q_OBJECT
  public:
    LibraryScannerDlg(QWidget* pParent = nullptr);

    void resetTaskCount();
    void addQueuedTasks(int num);

    // Injected rather than a hard dependency on Library, so this dialog
    // (constructed by LibraryScanner before Library even exists) stays
    // decoupled from it — the same pattern as AccessMenuController. In
    // production, CoreServices wires this to `Library::announceText` once
    // both exist. Called once, the first time the dialog becomes visible in
    // a scan (issue #63): it appears silently ~2s into a scan and steals
    // focus with no announcement.
    void setAnnounceCallback(std::function<void(const QString&)> callback) {
        m_announceCallback = std::move(callback);
    }

  public slots:
    void slotUpdate(const QString& path);
    void slotUpdateCover(const QString& path);
    void slotUpdateSubstitute(const QString& path);
    void slotCancel();
    void slotScanFinished();
    void slotScanStarted();

  signals:
    void scanCancelled();

  protected:
    void showEvent(QShowEvent* event) override;

  private:
    void updateProgressBar();

    PerformanceTimer m_timer;

    parented_ptr<QLabel> m_pLabelCurrent;
    parented_ptr<QProgressBar> m_pProgressBar;

    bool m_bCancelled;
    int m_tasksDone;
    int m_tasksTotal;
    bool m_showNoTasksQueuedWarning;

    std::function<void(const QString&)> m_announceCallback;
    // Only the first show per scan is announced; per-file progress-label
    // churn is pure visual noise and would spam a screen-reader user.
    bool m_announcedThisScan;
};

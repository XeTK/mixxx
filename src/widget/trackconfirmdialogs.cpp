#include "widget/trackconfirmdialogs.h"

#include <QCoreApplication>
#include <QMessageBox>
#include <QWidget>

#include "library/library.h"

namespace mixxx {
namespace trackconfirm {

namespace {

constexpr const char* kTrContext = "TrackConfirmDialogs";

} // namespace

QString purgeAnnouncement(int trackCount) {
    return QCoreApplication::translate(kTrContext,
            "Purge tracks dialog. Permanently remove %n track(s) from the "
            "library? This only removes the library entry, it does not "
            "delete the file from disk. No is selected by default; press "
            "Escape or Enter for no, or move to Yes and press Enter to "
            "purge.",
            nullptr,
            trackCount);
}

bool confirmPurge(QWidget* pParent, Library* pLibrary, int trackCount) {
    if (pLibrary) {
        pLibrary->announceText(purgeAnnouncement(trackCount));
    }
    QMessageBox::StandardButton btn = QMessageBox::question(pParent,
            QCoreApplication::translate(kTrContext, "Confirm Purge"),
            QCoreApplication::translate(kTrContext,
                    "Permanently remove %n track(s) from the library? "
                    "This does not delete the file(s) from disk.",
                    nullptr,
                    trackCount),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
    return btn == QMessageBox::Yes;
}

QString hideOrRemoveAnnouncement(TrackModel::Capability cap, int trackCount) {
    QString dialogTitle;
    QString action;
    switch (cap) {
    case TrackModel::Capability::Hide:
        dialogTitle = QCoreApplication::translate(kTrContext, "Confirm track hide dialog");
        action = QCoreApplication::translate(kTrContext,
                "hide the selected %n track(s)",
                nullptr,
                trackCount);
        break;
    case TrackModel::Capability::Remove:
        dialogTitle = QCoreApplication::translate(kTrContext, "Confirm track removal dialog");
        action = QCoreApplication::translate(kTrContext,
                "remove the selected %n track(s) from the AutoDJ queue",
                nullptr,
                trackCount);
        break;
    case TrackModel::Capability::RemoveCrate:
        dialogTitle = QCoreApplication::translate(kTrContext, "Confirm track removal dialog");
        action = QCoreApplication::translate(kTrContext,
                "remove the selected %n track(s) from this crate",
                nullptr,
                trackCount);
        break;
    case TrackModel::Capability::RemovePlaylist:
        dialogTitle = QCoreApplication::translate(kTrContext, "Confirm track removal dialog");
        action = QCoreApplication::translate(kTrContext,
                "remove the selected %n track(s) from this playlist",
                nullptr,
                trackCount);
        break;
    default:
        dialogTitle = QCoreApplication::translate(kTrContext, "Confirm track removal dialog");
        action = QCoreApplication::translate(kTrContext,
                "remove the selected %n track(s)",
                nullptr,
                trackCount);
        break;
    }
    return QCoreApplication::translate(kTrContext,
            "%1. Are you sure you want to %2? No is selected by default; "
            "press Escape or Enter for no, or move to Yes and press Enter "
            "to confirm.")
            .arg(dialogTitle, action);
}

QString deleteFromDiskAnnouncement(int trackCount) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    return QCoreApplication::translate(kTrContext,
            "Delete track files dialog. Permanently delete %n track file(s) "
            "from disk? This can not be undone. Cancel is selected by "
            "default; press Escape or Enter to cancel, or move to Delete "
            "Files and press Enter to delete.",
            nullptr,
            trackCount);
#else
    return QCoreApplication::translate(kTrContext,
            "Move track files to trash dialog. Move %n track file(s) to "
            "the trash bin? Cancel is selected by default; press Escape or "
            "Enter to cancel, or move to Okay and press Enter to continue.",
            nullptr,
            trackCount);
#endif
}

} // namespace trackconfirm
} // namespace mixxx

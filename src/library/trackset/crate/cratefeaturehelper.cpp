#include "library/trackset/crate/cratefeaturehelper.h"

#include <QInputDialog>
#include <QLineEdit>

#include "library/library.h"
#include "library/trackcollection.h"
#include "library/trackset/crate/crate.h"
#include "library/trackset/crate/cratesummary.h"
#include "moc_cratefeaturehelper.cpp"

CrateFeatureHelper::CrateFeatureHelper(
        TrackCollection* pTrackCollection,
        UserSettingsPointer pConfig,
        Library* pLibrary)
        : m_pTrackCollection(pTrackCollection),
          m_pConfig(pConfig),
          m_pLibrary(pLibrary) {
}

QString CrateFeatureHelper::proposeNameForNewCrate(
        const QString& initialName) const {
    DEBUG_ASSERT(!initialName.isEmpty());
    QString proposedName;
    int suffixCounter = 0;
    do {
        if (suffixCounter++ > 0) {
            // Append suffix " 2", " 3", ...
            proposedName = QStringLiteral("%1 %2")
                                   .arg(initialName, QString::number(suffixCounter));
        } else {
            proposedName = initialName;
        }
    } while (m_pTrackCollection->crates().readCrateByName(proposedName));
    // Found an unused crate name
    return proposedName;
}

CrateId CrateFeatureHelper::createEmptyCrate() {
    const QString proposedCrateName =
            proposeNameForNewCrate(tr("New Crate"));
    // Accessibility: the dialog's own title/label are standard Qt (and
    // readable by a screen reader), but Mixxx's own speech announces it too,
    // consistent with everything else in the accessibility fork, and spells
    // out that the proposed name is preselected and ready to type over.
    if (m_pLibrary) {
        m_pLibrary->announceText(tr(
                "New crate dialog. A text box is filled "
                "in with %1, selected. Type a name, "
                "then press Enter to create it, or "
                "Escape to cancel.")
                        .arg(proposedCrateName));
    }
    Crate newCrate;
    for (;;) {
        bool ok = false;
        auto newName =
                QInputDialog::getText(
                        nullptr,
                        tr("Create New Crate"),
                        tr("Enter name for new crate:"),
                        QLineEdit::Normal,
                        proposedCrateName,
                        &ok)
                        .trimmed();
        if (!ok) {
            return CrateId();
        }
        // Accessibility: echo back what was actually entered/accepted,
        // since a blind user can't proofread it visually before it's used
        // as the new crate's name.
        if (m_pLibrary) {
            m_pLibrary->announceText(
                    newName.isEmpty() ? tr("You entered nothing.")
                                      : tr("You entered: %1").arg(newName));
        }
        if (newName.isEmpty()) {
            if (m_pLibrary) {
                m_pLibrary->announceText(tr("A crate cannot have a blank name."));
            }
            QMessageBox::warning(
                    nullptr,
                    tr("Creating Crate Failed"),
                    tr("A crate cannot have a blank name."));
            continue;
        }
        if (m_pTrackCollection->crates().readCrateByName(newName)) {
            // Accessibility: speak the failure too, rather than leaving it
            // to the screen reader to notice and read the message box.
            if (m_pLibrary) {
                m_pLibrary->announceText(tr("A crate by that name already exists."));
            }
            QMessageBox::warning(
                    nullptr,
                    tr("Creating Crate Failed"),
                    tr("A crate by that name already exists."));
            continue;
        }
        newCrate.setName(std::move(newName));
        DEBUG_ASSERT(newCrate.hasName());
        break;
    }

    CrateId newCrateId;
    if (m_pTrackCollection->insertCrate(newCrate, &newCrateId)) {
        DEBUG_ASSERT(newCrateId.isValid());
        newCrate.setId(newCrateId);
        qDebug() << "Created new crate" << newCrate;
    } else {
        DEBUG_ASSERT(!newCrateId.isValid());
        qWarning() << "Failed to create new crate"
                   << "->" << newCrate.getName();
        QMessageBox::warning(
                nullptr,
                tr("Creating Crate Failed"),
                tr("An unknown error occurred while creating crate: ") + newCrate.getName());
    }
    return newCrateId;
}

CrateId CrateFeatureHelper::duplicateCrate(const Crate& oldCrate) {
    const QString proposedCrateName =
            proposeNameForNewCrate(
                    QStringLiteral("%1 %2")
                            .arg(oldCrate.getName(), tr("copy", "//:")));
    Crate newCrate;
    for (;;) {
        bool ok = false;
        auto newName =
                QInputDialog::getText(
                        nullptr,
                        tr("Duplicate Crate"),
                        tr("Enter name for new crate:"),
                        QLineEdit::Normal,
                        proposedCrateName,
                        &ok)
                        .trimmed();
        if (!ok) {
            return CrateId();
        }
        if (newName.isEmpty()) {
            QMessageBox::warning(
                    nullptr,
                    tr("Duplicating Crate Failed"),
                    tr("A crate cannot have a blank name."));
            continue;
        }
        if (m_pTrackCollection->crates().readCrateByName(newName)) {
            QMessageBox::warning(
                    nullptr,
                    tr("Duplicating Crate Failed"),
                    tr("A crate by that name already exists."));
            continue;
        }
        newCrate.setName(std::move(newName));
        DEBUG_ASSERT(newCrate.hasName());
        break;
    }

    CrateId newCrateId;
    if (m_pTrackCollection->insertCrate(newCrate, &newCrateId)) {
        DEBUG_ASSERT(newCrateId.isValid());
        newCrate.setId(newCrateId);
        qDebug() << "Created new crate" << newCrate;
        QList<TrackId> trackIds;
        trackIds.reserve(
                m_pTrackCollection->crates().countCrateTracks(oldCrate.getId()));
        {
            CrateTrackSelectResult crateTracks(
                    m_pTrackCollection->crates().selectCrateTracksSorted(oldCrate.getId()));
            while (crateTracks.next()) {
                trackIds.append(crateTracks.trackId());
            }
        }
        if (m_pTrackCollection->addCrateTracks(newCrateId, trackIds)) {
            qDebug() << "Duplicated crate"
                     << oldCrate << "->" << newCrate;
        } else {
            qWarning() << "Failed to copy tracks from"
                       << oldCrate << "into" << newCrate;
        }
    } else {
        qWarning() << "Failed to duplicate crate"
                   << oldCrate << "->" << newCrate.getName();
        QMessageBox::warning(
                nullptr,
                tr("Duplicating Crate Failed"),
                tr("An unknown error occurred while creating crate: ") + newCrate.getName());
    }
    return newCrateId;
}

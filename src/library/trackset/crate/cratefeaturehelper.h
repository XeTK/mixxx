#pragma once

#include <QObject>

#include "library/trackset/crate/crateid.h"
#include "preferences/usersettings.h"

class TrackCollection;
class Crate;
class Library;

class CrateFeatureHelper : public QObject {
    Q_OBJECT

  public:
    CrateFeatureHelper(
            TrackCollection* pTrackCollection,
            UserSettingsPointer pConfig,
            Library* pLibrary = nullptr);
    ~CrateFeatureHelper() override = default;

    CrateId createEmptyCrate();
    CrateId duplicateCrate(const Crate& oldCrate);

  private:
    QString proposeNameForNewCrate(
            const QString& initialName = QString()) const;

    TrackCollection* m_pTrackCollection;

    UserSettingsPointer m_pConfig;

    // Accessibility: speaks the create-dialog announcement. Optional
    // (nullptr) so call sites that don't have a Library handy still compile;
    // in that case the dialog is silent apart from the screen reader.
    Library* m_pLibrary;
};

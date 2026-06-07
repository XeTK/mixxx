#pragma once

#include <QString>

#include "preferences/accessibilitysettings.h"
#include "preferences/dialog/dlgpreferencepage.h"
#include "preferences/dialog/ui_dlgprefaccessibilitydlg.h"
#include "preferences/usersettings.h"
#include "util/ttsengine.h"

class DlgPrefAccessibility : public DlgPreferencePage, public Ui::DlgAccessibilityDlg {
    Q_OBJECT
  public:
    DlgPrefAccessibility(QWidget* parent, UserSettingsPointer pConfig);

  public slots:
    void slotApply() override;
    void slotUpdate() override;
    void slotResetToDefaults() override;

  private:
    void populateDeviceCombo();
    int indexForDeviceId(const QString& deviceId) const;

    AccessibilitySettings m_settings;
    QList<TtsEngine::AudioOutputDevice> m_outputDevices;
    QString m_ttsOutputDeviceId;
    bool m_bAnnounceStartup;
    bool m_bAnnounceSelection;
    bool m_bAnnounceLoad;
    bool m_bAnnouncePlay;
    bool m_bAnnounceStop;
    bool m_bAnnounceEndOfTrack;
    bool m_bAnnounceLibraryFocus;
};

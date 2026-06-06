#pragma once

#include "preferences/accessibilitysettings.h"
#include "preferences/dialog/dlgpreferencepage.h"
#include "preferences/dialog/ui_dlgprefaccessibilitydlg.h"
#include "preferences/usersettings.h"

class DlgPrefAccessibility : public DlgPreferencePage, public Ui::DlgAccessibilityDlg {
    Q_OBJECT
  public:
    DlgPrefAccessibility(QWidget* parent, UserSettingsPointer pConfig);

  public slots:
    void slotApply() override;
    void slotUpdate() override;
    void slotResetToDefaults() override;

  private:
    AccessibilitySettings m_settings;
    bool m_bAnnounceSelection;
    bool m_bAnnounceLoad;
};

#include "preferences/dialog/dlgprefaccessibility.h"

#include "moc_dlgprefaccessibility.cpp"

DlgPrefAccessibility::DlgPrefAccessibility(QWidget* parent, UserSettingsPointer pConfig)
        : DlgPreferencePage(parent),
          m_settings(pConfig),
          m_bAnnounceSelection(m_settings.getAnnounceTrackSelectionDefault()),
          m_bAnnounceLoad(m_settings.getAnnounceTrackLoadDefault()) {
    setupUi(this);

    connect(checkBoxAnnounceSelection,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceSelection = checked; });
    connect(checkBoxAnnounceLoad,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceLoad = checked; });
}

void DlgPrefAccessibility::slotUpdate() {
    m_bAnnounceSelection = m_settings.getAnnounceTrackSelection();
    m_bAnnounceLoad = m_settings.getAnnounceTrackLoad();
    checkBoxAnnounceSelection->setChecked(m_bAnnounceSelection);
    checkBoxAnnounceLoad->setChecked(m_bAnnounceLoad);
}

void DlgPrefAccessibility::slotApply() {
    m_settings.setAnnounceTrackSelection(m_bAnnounceSelection);
    m_settings.setAnnounceTrackLoad(m_bAnnounceLoad);
}

void DlgPrefAccessibility::slotResetToDefaults() {
    m_bAnnounceSelection = m_settings.getAnnounceTrackSelectionDefault();
    m_bAnnounceLoad = m_settings.getAnnounceTrackLoadDefault();
    checkBoxAnnounceSelection->setChecked(m_bAnnounceSelection);
    checkBoxAnnounceLoad->setChecked(m_bAnnounceLoad);
}

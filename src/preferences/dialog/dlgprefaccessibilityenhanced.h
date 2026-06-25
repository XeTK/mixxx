#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include "preferences/accessibilitysettings.h"
#include "preferences/dialog/dlgprefaccessibility.h"

class EnhancedAnnouncementManager;

class DlgPrefAccessibilityEnhanced : public DlgPrefAccessibility {
    Q_OBJECT

public:
  DlgPrefAccessibilityEnhanced(
          QWidget* parent,
          UserSettingsPointer pConfig,
          EngineTts* pTtsSink);
  ~DlgPrefAccessibilityEnhanced() override;

  void load() override;
  void apply() override;

private:
  void setupUi();
  void connectSignals();

  // Enhanced settings
  AccessibilitySettings m_accessibilitySettings;

  // Widgets for the enhanced UI
  QComboBox* m_pDeckNamingCombo;
  QCheckBox* m_pEnableTtsByDefaultCheckbox;

  // Enhanced announcement checkboxes
  QCheckBox* m_pAnnounceEqCheckbox;
  QCheckBox* m_pAnnounceFilterCheckbox;
  QCheckBox* m_pAnnounceTrimCheckbox;
  QCheckBox* m_pAnnounceMasterCheckbox;
  QCheckBox* m_pAnnounceMixCheckbox;
  QCheckBox* m_pAnnounceEffectCheckbox;
  QCheckBox* m_pAnnounceSyncCheckbox;
  QCheckBox* m_pAnnounceTempoCheckbox;
  QCheckBox* m_pAnnounceCrossFaderCheckbox;
  QCheckBox* m_pAnnounceFaderChangeCheckbox;
  QCheckBox* m_pAnnouncePreventJoggingCheckbox;
  QCheckBox* m_pAnnounceTouchSurfaceCheckbox;
  QCheckBox* m_pAnnounceTtsToggleCheckbox;

private slots:
  void onDeckNamingConventionChanged(const QString& convention);
  void onEnableTtsByDefaultChanged(int state);
  void onAnnounceEqChanged(int state);
  void onAnnounceFilterChanged(int state);
  void onAnnounceTrimChanged(int state);
  void onAnnounceMasterChanged(int state);
  void onAnnounceMixChanged(int state);
  void onAnnounceEffectChanged(int state);
  void onAnnounceSyncChanged(int state);
  void onAnnounceTempoChanged(int state);
  void onAnnounceCrossFaderChanged(int state);
  void onAnnounceFaderChangeChanged(int state);
  void onAnnouncePreventJoggingChanged(int state);
  void onAnnounceTouchSurfaceChanged(int state);
  void onAnnounceTtsToggleChanged(int state);
};

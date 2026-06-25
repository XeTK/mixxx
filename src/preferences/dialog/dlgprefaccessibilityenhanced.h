#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QWidget>

#include "preferences/accessibilitysettings.h"
#include "preferences/dialog/dlgprefaccessibility.h"
#include "widget/keyboardnavigation.h"

class DlgPrefAccessibilityEnhanced : public DlgPrefAccessibility {
    Q_OBJECT

public:
  explicit DlgPrefAccessibilityEnhanced(
          QWidget* parent,
          UserSettingsPointer pConfig,
          EngineTts* pTtsSink);

  ~DlgPrefAccessibilityEnhanced() override;

  /// Apply all settings from the UI to the configuration
  void apply() override;

  /// Load all settings from the configuration to the UI
  void load() override;

private slots:
  void onDeckNamingConventionChanged(const QString& convention);
  void onEnableTtsByDefaultChanged(int state);
  void onAnnounceCueChanged(int state);
  void onAnnounceTrackLoadChanged(int state);
  void onAnnouncePlayChanged(int state);
  void onAnnounceStopChanged(int state);
  void onAnnounceEndOfTrackChanged(int state);

private:
  void setupUi();
  void connectSignals();

  // UI Elements
  QComboBox* m_pDeckNamingCombo;
  QCheckBox* m_pEnableTtsByDefaultCheckbox;
  QCheckBox* m_pAnnounceCueCheckbox;
  QCheckBox* m_pAnnounceTrackLoadCheckbox;
  QCheckBox* m_pAnnouncePlayCheckbox;
  QCheckBox* m_pAnnounceStopCheckbox;
  QCheckBox* m_pAnnounceEndOfTrackCheckbox;

  // Settings
  AccessibilitySettings m_accessibilitySettings;
};

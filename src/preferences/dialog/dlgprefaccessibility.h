#pragma once

#include <QString>
#include <memory>

#include "preferences/accessibilitysettings.h"
#include "preferences/dialog/dlgpreferencepage.h"
#include "preferences/dialog/ui_dlgprefaccessibilitydlg.h"
#include "preferences/usersettings.h"
#include "util/ttsengine.h"

class EngineTts;

class DlgPrefAccessibility : public DlgPreferencePage, public Ui::DlgAccessibilityDlg {
    Q_OBJECT
  public:
    DlgPrefAccessibility(QWidget* parent, UserSettingsPointer pConfig, EngineTts* pTtsSink);

  public slots:
    void slotApply() override;
    void slotUpdate() override;
    void slotResetToDefaults() override;

  private slots:
    void slotTestSpeech();

  private:
    void populateRouteCombo();
    void populateVoiceCombo();
    int indexForVoiceId(const QString& voiceId) const;

    AccessibilitySettings m_settings;
    EngineTts* m_pTtsSink;
    QList<TtsEngine::Voice> m_voices;
    int m_ttsRoute;
    QString m_ttsVoiceId;
    int m_ttsRate;
    bool m_bAnnounceStartup;
    bool m_bAnnounceSelection;
    bool m_bAnnounceLoad;
    bool m_bAnnouncePlay;
    bool m_bAnnounceStop;
    bool m_bAnnounceEndOfTrack;
    bool m_bAnnounceLibraryFocus;
    bool m_bAnnounceSearch;

    std::unique_ptr<TtsEngine> m_pTestEngine;
};

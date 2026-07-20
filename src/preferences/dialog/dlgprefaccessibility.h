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
    // macOS only: shows and fills the quality-tier filter combo; hides it
    // (and its label) on other platforms, which have no equivalent concept.
    void populateVoiceQualityCombo();
    // Rebuilds comboBoxTtsVoice from m_allVoices, applying the current
    // quality filter, without touching m_ttsVoiceId.
    void refreshFilteredVoiceCombo();
    void populateFeedbackModeCombos();
    void populateMixerStyleCombo();
    // Apply a preset (0 speech, 1 sounds, 2 both) to all four per-event
    // feedback combos at once.
    void applyFeedbackPreset(int mode);
    // Refresh the "All transport feedback" combo from the four per-event
    // values: the matching preset when they agree, otherwise Custom.
    void syncFeedbackAllCombo();
    int indexForVoiceId(const QString& voiceId) const;

    AccessibilitySettings m_settings;
    EngineTts* m_pTtsSink;
    // All voices as reported by the backend, unfiltered; m_voices below is
    // the (possibly quality-filtered) subset actually shown in the combo box.
    QList<TtsEngine::Voice> m_allVoices;
    QList<TtsEngine::Voice> m_voices;
    int m_ttsRoute;
    QString m_ttsVoiceId;
    // macOS only: 0 = show all voices, 1..3 = TtsEngine::VoiceQuality + 1.
    int m_ttsVoiceQualityFilter;
    int m_ttsRate;
    int m_duckStrengthPercent;
    int m_beatClickVolumePercent;
    int m_mixerReadoutStyle;
    int m_mixerFractionDetail;
    int m_feedbackModePlay;
    int m_feedbackModeStop;
    int m_feedbackModeEndOfTrack;
    int m_feedbackModeCue;
    int m_feedbackModeRestart;
    int m_feedbackModeLoop;
    int m_feedbackModeClipping;
    bool m_bAnnounceClipping;
    bool m_bAnnounceStartup;
    bool m_bAnnounceSelection;
    bool m_bAnnounceLoad;
    bool m_bAnnouncePlay;
    bool m_bAnnounceCue;
    bool m_bSmartCue;
    bool m_bAnnounceStop;
    bool m_bAnnounceEndOfTrack;
    bool m_bAnnounceLibraryFocus;
    bool m_bAnnounceSearch;
    bool m_bAnnouncePlaylist;
    bool m_bAnnounceSync;
    bool m_bAnnounceTempo;
    bool m_bAnnounceLoop;
    bool m_bAnnounceHotcue;
    bool m_bAnnounceRecording;
    bool m_bAnnounceEffects;
    bool m_bAnnounceMixer;
    bool m_bAnnounceWhileMoving;
    bool m_bDeckNumbers;
    bool m_bConcise;

    std::unique_ptr<TtsEngine> m_pTestEngine;
};

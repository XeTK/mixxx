#include "preferences/dialog/dlgprefaccessibility.h"

#include <QLabel>
#include <algorithm>
#include <cmath>

#include "control/controlproxy.h"
#include "engine/enginetts.h"
#include "moc_dlgprefaccessibility.cpp"

namespace {
// Matches kDefaultDuckStrength in enginetts.cpp, as a slider percentage.
constexpr int kDefaultDuckStrengthPercent = 50;

// The ducking strength lives in the persistent [Tts],duckStrength control
// rather than an [Accessibility] config key, because the engine reads it
// directly on the audio thread.
double readDuckStrengthControl() {
    return ControlProxy(QStringLiteral("[Tts]"),
            QStringLiteral("duckStrength"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid)
            .get();
}

void writeDuckStrengthControl(int percent) {
    ControlProxy(QStringLiteral("[Tts]"),
            QStringLiteral("duckStrength"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid)
            .set(percent / 100.0);
}
} // namespace

DlgPrefAccessibility::DlgPrefAccessibility(
        QWidget* parent, UserSettingsPointer pConfig, EngineTts* pTtsSink)
        : DlgPreferencePage(parent),
          m_settings(pConfig),
          m_pTtsSink(pTtsSink),
          m_ttsRoute(m_settings.getTtsRouteDefault()),
          m_ttsVoiceId(m_settings.getTtsVoiceDefault()),
          m_ttsRate(m_settings.getTtsRateDefault()),
          m_duckStrengthPercent(kDefaultDuckStrengthPercent),
          m_feedbackMode(m_settings.getFeedbackModeDefault()),
          m_bAnnounceStartup(m_settings.getAnnounceStartupDefault()),
          m_bAnnounceSelection(m_settings.getAnnounceTrackSelectionDefault()),
          m_bAnnounceLoad(m_settings.getAnnounceTrackLoadDefault()),
          m_bAnnouncePlay(m_settings.getAnnouncePlayDefault()),
          m_bAnnounceCue(m_settings.getAnnounceCueDefault()),
          m_bAnnounceStop(m_settings.getAnnounceStopDefault()),
          m_bAnnounceEndOfTrack(m_settings.getAnnounceEndOfTrackDefault()),
          m_bAnnounceLibraryFocus(m_settings.getAnnounceLibraryFocusDefault()),
          m_bAnnounceSearch(m_settings.getAnnounceSearchDefault()),
          m_bAnnouncePlaylist(m_settings.getAnnouncePlaylistDefault()),
          m_bAnnounceSync(m_settings.getAnnounceSyncDefault()),
          m_bAnnounceTempo(m_settings.getAnnounceTempoDefault()),
          m_bAnnounceLoop(m_settings.getAnnounceLoopDefault()),
          m_bAnnounceHotcue(m_settings.getAnnounceHotcueDefault()),
          m_bAnnounceRecording(m_settings.getAnnounceRecordingDefault()),
          m_bAnnounceMixer(m_settings.getAnnounceMixerDefault()),
          m_bAnnounceWhileMoving(m_settings.getAnnounceWhileMovingDefault()),
          m_bDeckNumbers(m_settings.getDeckNamesAsNumbersDefault()),
          m_bConcise(m_settings.getConciseAnnouncementsDefault()) {
    setupUi(this);
    populateRouteCombo();
    populateVoiceCombo();
    populateFeedbackModeCombo();

    if (!TtsEngine::isAvailable()) {
        auto* pWarning = new QLabel(
                tr("No speech engine is available in this build. Spoken "
                   "announcements are disabled. On Linux, install Qt "
                   "TextToSpeech 6.6 or newer and rebuild Mixxx."),
                this);
        pWarning->setWordWrap(true);
        pWarning->setStyleSheet(QStringLiteral("font-weight: bold;"));
        verticalLayout->insertWidget(0, pWarning);
        pushButtonTestSpeech->setEnabled(false);
    }

    connect(comboBoxTtsRoute,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) { m_ttsRoute = index; });
    connect(comboBoxFeedbackMode,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) { m_feedbackMode = index; });
    connect(comboBoxTtsVoice,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                m_ttsVoiceId = (index > 0 && index <= m_voices.size())
                        ? m_voices.at(index - 1).id
                        : QString();
            });
    connect(sliderTtsRate,
            &QSlider::valueChanged,
            this,
            [this](int value) {
                m_ttsRate = value;
                spinBoxTtsRate->blockSignals(true);
                spinBoxTtsRate->setValue(value);
                spinBoxTtsRate->blockSignals(false);
            });
    connect(spinBoxTtsRate,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                m_ttsRate = value;
                sliderTtsRate->blockSignals(true);
                sliderTtsRate->setValue(value);
                sliderTtsRate->blockSignals(false);
            });
    connect(sliderDuckStrength,
            &QSlider::valueChanged,
            this,
            [this](int value) { m_duckStrengthPercent = value; });
    connect(pushButtonTestSpeech,
            &QPushButton::clicked,
            this,
            &DlgPrefAccessibility::slotTestSpeech);
    connect(checkBoxAnnounceStartup,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceStartup = checked; });
    connect(checkBoxAnnounceSelection,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceSelection = checked; });
    connect(checkBoxAnnounceLoad,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceLoad = checked; });
    connect(checkBoxAnnouncePlay,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnouncePlay = checked; });
    connect(checkBoxAnnounceCue,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceCue = checked; });
    connect(checkBoxAnnounceStop,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceStop = checked; });
    connect(checkBoxAnnounceEndOfTrack,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceEndOfTrack = checked; });
    connect(checkBoxAnnounceLibraryFocus,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceLibraryFocus = checked; });
    connect(checkBoxAnnounceSearch,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceSearch = checked; });
    connect(checkBoxAnnouncePlaylist,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnouncePlaylist = checked; });
    connect(checkBoxAnnounceSync,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceSync = checked; });
    connect(checkBoxAnnounceTempo,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceTempo = checked; });
    connect(checkBoxAnnounceLoop,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceLoop = checked; });
    connect(checkBoxAnnounceHotcue,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceHotcue = checked; });
    connect(checkBoxAnnounceRecording,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceRecording = checked; });
    connect(checkBoxAnnounceMixer,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceMixer = checked; });
    connect(checkBoxAnnounceWhileMoving,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceWhileMoving = checked; });
    connect(checkBoxDeckNumbers,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bDeckNumbers = checked; });
    connect(checkBoxConcise,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bConcise = checked; });
}

void DlgPrefAccessibility::populateRouteCombo() {
    // Order must match EngineTts::Route (0 = Headphones, 1 = Main).
    comboBoxTtsRoute->clear();
    comboBoxTtsRoute->addItem(tr("Headphones / cue (DJ only)"));
    comboBoxTtsRoute->addItem(tr("Main output (audience)"));
}

void DlgPrefAccessibility::populateFeedbackModeCombo() {
    // Order must match AccessibilitySettings FeedbackMode: 0 speech, 1 sounds,
    // 2 both.
    comboBoxFeedbackMode->clear();
    comboBoxFeedbackMode->addItem(tr("Speech"));
    comboBoxFeedbackMode->addItem(tr("Sounds"));
    comboBoxFeedbackMode->addItem(tr("Sounds and speech"));
}

void DlgPrefAccessibility::populateVoiceCombo() {
    m_voices = TtsEngine::enumerateVoices();
    comboBoxTtsVoice->clear();
    comboBoxTtsVoice->addItem(tr("Default (system voice)"));
    for (const TtsEngine::Voice& voice : m_voices) {
        comboBoxTtsVoice->addItem(voice.displayName);
    }
}

int DlgPrefAccessibility::indexForVoiceId(const QString& voiceId) const {
    if (voiceId.isEmpty()) {
        return 0;
    }
    for (int i = 0; i < m_voices.size(); ++i) {
        if (m_voices.at(i).id == voiceId) {
            return i + 1;
        }
    }
    return 0;
}

void DlgPrefAccessibility::slotUpdate() {
    m_ttsRoute = m_settings.getTtsRoute();
    m_ttsVoiceId = m_settings.getTtsVoice();
    m_ttsRate = m_settings.getTtsRate();
    m_feedbackMode = m_settings.getFeedbackMode();
    comboBoxFeedbackMode->setCurrentIndex(
            std::clamp(m_feedbackMode, 0, comboBoxFeedbackMode->count() - 1));
    const double duckStrength = readDuckStrengthControl();
    m_duckStrengthPercent = duckStrength > 0.0
            ? static_cast<int>(std::lround(duckStrength * 100))
            : kDefaultDuckStrengthPercent;
    sliderDuckStrength->setValue(m_duckStrengthPercent);
    m_bAnnounceStartup = m_settings.getAnnounceStartup();
    m_bAnnounceSelection = m_settings.getAnnounceTrackSelection();
    m_bAnnounceLoad = m_settings.getAnnounceTrackLoad();
    m_bAnnouncePlay = m_settings.getAnnouncePlay();
    m_bAnnounceCue = m_settings.getAnnounceCue();
    m_bAnnounceStop = m_settings.getAnnounceStop();
    m_bAnnounceEndOfTrack = m_settings.getAnnounceEndOfTrack();
    m_bAnnounceLibraryFocus = m_settings.getAnnounceLibraryFocus();
    m_bAnnounceSearch = m_settings.getAnnounceSearch();
    m_bAnnouncePlaylist = m_settings.getAnnouncePlaylist();
    m_bAnnounceSync = m_settings.getAnnounceSync();
    m_bAnnounceTempo = m_settings.getAnnounceTempo();
    m_bAnnounceLoop = m_settings.getAnnounceLoop();
    m_bAnnounceHotcue = m_settings.getAnnounceHotcue();
    m_bAnnounceRecording = m_settings.getAnnounceRecording();
    m_bAnnounceMixer = m_settings.getAnnounceMixer();
    m_bAnnounceWhileMoving = m_settings.getAnnounceWhileMoving();
    m_bDeckNumbers = m_settings.getDeckNamesAsNumbers();
    m_bConcise = m_settings.getConciseAnnouncements();
    comboBoxTtsRoute->setCurrentIndex(
            std::clamp(m_ttsRoute, 0, comboBoxTtsRoute->count() - 1));
    comboBoxTtsVoice->setCurrentIndex(indexForVoiceId(m_ttsVoiceId));
    sliderTtsRate->setValue(m_ttsRate);
    spinBoxTtsRate->setValue(m_ttsRate);
    checkBoxAnnounceStartup->setChecked(m_bAnnounceStartup);
    checkBoxAnnounceSelection->setChecked(m_bAnnounceSelection);
    checkBoxAnnounceLoad->setChecked(m_bAnnounceLoad);
    checkBoxAnnouncePlay->setChecked(m_bAnnouncePlay);
    checkBoxAnnounceCue->setChecked(m_bAnnounceCue);
    checkBoxAnnounceStop->setChecked(m_bAnnounceStop);
    checkBoxAnnounceEndOfTrack->setChecked(m_bAnnounceEndOfTrack);
    checkBoxAnnounceLibraryFocus->setChecked(m_bAnnounceLibraryFocus);
    checkBoxAnnounceSearch->setChecked(m_bAnnounceSearch);
    checkBoxAnnouncePlaylist->setChecked(m_bAnnouncePlaylist);
    checkBoxAnnounceSync->setChecked(m_bAnnounceSync);
    checkBoxAnnounceTempo->setChecked(m_bAnnounceTempo);
    checkBoxAnnounceLoop->setChecked(m_bAnnounceLoop);
    checkBoxAnnounceHotcue->setChecked(m_bAnnounceHotcue);
    checkBoxAnnounceRecording->setChecked(m_bAnnounceRecording);
    checkBoxAnnounceMixer->setChecked(m_bAnnounceMixer);
    checkBoxAnnounceWhileMoving->setChecked(m_bAnnounceWhileMoving);
    checkBoxDeckNumbers->setChecked(m_bDeckNumbers);
    checkBoxConcise->setChecked(m_bConcise);
}

void DlgPrefAccessibility::slotApply() {
    m_settings.setTtsRoute(m_ttsRoute);
    m_settings.setTtsVoice(m_ttsVoiceId);
    m_settings.setTtsRate(m_ttsRate);
    m_settings.setFeedbackMode(m_feedbackMode);
    writeDuckStrengthControl(m_duckStrengthPercent);
    m_settings.setAnnounceStartup(m_bAnnounceStartup);
    m_settings.setAnnounceTrackSelection(m_bAnnounceSelection);
    m_settings.setAnnounceTrackLoad(m_bAnnounceLoad);
    m_settings.setAnnouncePlay(m_bAnnouncePlay);
    m_settings.setAnnounceCue(m_bAnnounceCue);
    m_settings.setAnnounceStop(m_bAnnounceStop);
    m_settings.setAnnounceEndOfTrack(m_bAnnounceEndOfTrack);
    m_settings.setAnnounceLibraryFocus(m_bAnnounceLibraryFocus);
    m_settings.setAnnounceSearch(m_bAnnounceSearch);
    m_settings.setAnnouncePlaylist(m_bAnnouncePlaylist);
    m_settings.setAnnounceSync(m_bAnnounceSync);
    m_settings.setAnnounceTempo(m_bAnnounceTempo);
    m_settings.setAnnounceLoop(m_bAnnounceLoop);
    m_settings.setAnnounceHotcue(m_bAnnounceHotcue);
    m_settings.setAnnounceRecording(m_bAnnounceRecording);
    m_settings.setAnnounceMixer(m_bAnnounceMixer);
    m_settings.setAnnounceWhileMoving(m_bAnnounceWhileMoving);
    m_settings.setDeckNamesAsNumbers(m_bDeckNumbers);
    m_settings.setConciseAnnouncements(m_bConcise);
}

void DlgPrefAccessibility::slotTestSpeech() {
    if (!m_pTtsSink) {
        return;
    }
    m_pTestEngine = TtsEngine::create();
    m_pTestEngine->setSink(m_pTtsSink);
    m_pTtsSink->setRoute(m_ttsRoute);
    // Apply the slider live so the test reflects the chosen ducking level.
    writeDuckStrengthControl(m_duckStrengthPercent);
    if (!m_ttsVoiceId.isEmpty()) {
        m_pTestEngine->setVoice(m_ttsVoiceId);
    }
    m_pTestEngine->setRate(m_ttsRate);
    m_pTestEngine->say(tr("Mixxx ready. Artist, Title. One twenty beats per minute."));
}

void DlgPrefAccessibility::slotResetToDefaults() {
    m_duckStrengthPercent = kDefaultDuckStrengthPercent;
    sliderDuckStrength->setValue(m_duckStrengthPercent);
    m_ttsRoute = m_settings.getTtsRouteDefault();
    m_ttsVoiceId = m_settings.getTtsVoiceDefault();
    m_ttsRate = m_settings.getTtsRateDefault();
    m_feedbackMode = m_settings.getFeedbackModeDefault();
    comboBoxFeedbackMode->setCurrentIndex(
            std::clamp(m_feedbackMode, 0, comboBoxFeedbackMode->count() - 1));
    m_bAnnounceStartup = m_settings.getAnnounceStartupDefault();
    m_bAnnounceSelection = m_settings.getAnnounceTrackSelectionDefault();
    m_bAnnounceLoad = m_settings.getAnnounceTrackLoadDefault();
    m_bAnnouncePlay = m_settings.getAnnouncePlayDefault();
    m_bAnnounceCue = m_settings.getAnnounceCueDefault();
    m_bAnnounceStop = m_settings.getAnnounceStopDefault();
    m_bAnnounceEndOfTrack = m_settings.getAnnounceEndOfTrackDefault();
    m_bAnnounceLibraryFocus = m_settings.getAnnounceLibraryFocusDefault();
    m_bAnnounceSearch = m_settings.getAnnounceSearchDefault();
    m_bAnnouncePlaylist = m_settings.getAnnouncePlaylistDefault();
    m_bAnnounceSync = m_settings.getAnnounceSyncDefault();
    m_bAnnounceTempo = m_settings.getAnnounceTempoDefault();
    m_bAnnounceLoop = m_settings.getAnnounceLoopDefault();
    m_bAnnounceHotcue = m_settings.getAnnounceHotcueDefault();
    m_bAnnounceRecording = m_settings.getAnnounceRecordingDefault();
    m_bAnnounceMixer = m_settings.getAnnounceMixerDefault();
    m_bAnnounceWhileMoving = m_settings.getAnnounceWhileMovingDefault();
    m_bDeckNumbers = m_settings.getDeckNamesAsNumbersDefault();
    m_bConcise = m_settings.getConciseAnnouncementsDefault();
    comboBoxTtsRoute->setCurrentIndex(
            std::clamp(m_ttsRoute, 0, comboBoxTtsRoute->count() - 1));
    comboBoxTtsVoice->setCurrentIndex(indexForVoiceId(m_ttsVoiceId));
    sliderTtsRate->setValue(m_ttsRate);
    spinBoxTtsRate->setValue(m_ttsRate);
    checkBoxAnnounceStartup->setChecked(m_bAnnounceStartup);
    checkBoxAnnounceSelection->setChecked(m_bAnnounceSelection);
    checkBoxAnnounceLoad->setChecked(m_bAnnounceLoad);
    checkBoxAnnouncePlay->setChecked(m_bAnnouncePlay);
    checkBoxAnnounceCue->setChecked(m_bAnnounceCue);
    checkBoxAnnounceStop->setChecked(m_bAnnounceStop);
    checkBoxAnnounceEndOfTrack->setChecked(m_bAnnounceEndOfTrack);
    checkBoxAnnounceLibraryFocus->setChecked(m_bAnnounceLibraryFocus);
    checkBoxAnnounceSearch->setChecked(m_bAnnounceSearch);
    checkBoxAnnouncePlaylist->setChecked(m_bAnnouncePlaylist);
    checkBoxAnnounceSync->setChecked(m_bAnnounceSync);
    checkBoxAnnounceTempo->setChecked(m_bAnnounceTempo);
    checkBoxAnnounceLoop->setChecked(m_bAnnounceLoop);
    checkBoxAnnounceHotcue->setChecked(m_bAnnounceHotcue);
    checkBoxAnnounceRecording->setChecked(m_bAnnounceRecording);
    checkBoxAnnounceMixer->setChecked(m_bAnnounceMixer);
    checkBoxAnnounceWhileMoving->setChecked(m_bAnnounceWhileMoving);
    checkBoxDeckNumbers->setChecked(m_bDeckNumbers);
    checkBoxConcise->setChecked(m_bConcise);
}

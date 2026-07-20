#include "preferences/dialog/dlgprefaccessibility.h"

#include <QLabel>
#include <QSignalBlocker>
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

// Matches the default in enginebeatclick.cpp, as a slider percentage.
constexpr int kDefaultBeatClickVolumePercent = 75;

// Also a persistent engine control the audio thread reads directly.
double readBeatClickVolumeControl() {
    return ControlProxy(QStringLiteral("[BeatClick]"),
            QStringLiteral("volume"),
            nullptr,
            ControlFlag::AllowMissingOrInvalid)
            .get();
}

void writeBeatClickVolumeControl(int percent) {
    ControlProxy(QStringLiteral("[BeatClick]"),
            QStringLiteral("volume"),
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
          m_ttsVoiceQualityFilter(m_settings.getTtsVoiceQualityFilterDefault()),
          m_ttsRate(m_settings.getTtsRateDefault()),
          m_duckStrengthPercent(kDefaultDuckStrengthPercent),
          m_beatClickVolumePercent(kDefaultBeatClickVolumePercent),
          m_mixerReadoutStyle(m_settings.getMixerReadoutStyleDefault()),
          m_mixerFractionDetail(m_settings.getMixerFractionDetailDefault()),
          m_feedbackModePlay(m_settings.getFeedbackModePlayDefault()),
          m_feedbackModeStop(m_settings.getFeedbackModeStopDefault()),
          m_feedbackModeEndOfTrack(m_settings.getFeedbackModeEndOfTrackDefault()),
          m_feedbackModeCue(m_settings.getFeedbackModeCueDefault()),
          m_feedbackModeRestart(m_settings.getFeedbackModeRestartDefault()),
          m_feedbackModeLoop(m_settings.getFeedbackModeLoopDefault()),
          m_feedbackModeClipping(m_settings.getFeedbackModeClippingDefault()),
          m_bAnnounceClipping(m_settings.getAnnounceClippingDefault()),
          m_bAnnounceStartup(m_settings.getAnnounceStartupDefault()),
          m_bAnnounceSelection(m_settings.getAnnounceTrackSelectionDefault()),
          m_bAnnounceLoad(m_settings.getAnnounceTrackLoadDefault()),
          m_bAnnouncePlay(m_settings.getAnnouncePlayDefault()),
          m_bAnnounceCue(m_settings.getAnnounceCueDefault()),
          m_bSmartCue(m_settings.getSmartCueDefault()),
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
          m_bAnnounceEffects(m_settings.getAnnounceEffectsDefault()),
          m_bAnnounceMixer(m_settings.getAnnounceMixerDefault()),
          m_bAnnounceWhileMoving(m_settings.getAnnounceWhileMovingDefault()),
          m_bDeckNumbers(m_settings.getDeckNamesAsNumbersDefault()),
          m_bConcise(m_settings.getConciseAnnouncementsDefault()) {
    setupUi(this);
    populateRouteCombo();
    populateVoiceQualityCombo();
    populateVoiceCombo();
    populateFeedbackModeCombos();
    populateMixerStyleCombo();

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
    connect(comboBoxFeedbackAll,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                // Index 0 is Custom (a display state), so ignore it; 1..3 are
                // the presets mapping to modes 0..2.
                if (index >= 1) {
                    applyFeedbackPreset(index - 1);
                }
            });
    connect(comboBoxFeedbackPlay,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                m_feedbackModePlay = index;
                syncFeedbackAllCombo();
            });
    connect(comboBoxFeedbackStop,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                m_feedbackModeStop = index;
                syncFeedbackAllCombo();
            });
    connect(comboBoxFeedbackEndOfTrack,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                m_feedbackModeEndOfTrack = index;
                syncFeedbackAllCombo();
            });
    connect(comboBoxFeedbackCue,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                m_feedbackModeCue = index;
                syncFeedbackAllCombo();
            });
    connect(comboBoxFeedbackRestart,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) { m_feedbackModeRestart = index; });
    connect(comboBoxFeedbackLoop,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) { m_feedbackModeLoop = index; });
    connect(comboBoxFeedbackClipping,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) { m_feedbackModeClipping = index; });
    connect(checkBoxAnnounceClipping,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceClipping = checked; });
    connect(comboBoxMixerStyle,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) { m_mixerReadoutStyle = index; });
    connect(comboBoxFractionDetail,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) { m_mixerFractionDetail = index; });
    connect(comboBoxTtsVoice,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                m_ttsVoiceId = (index > 0 && index <= m_voices.size())
                        ? m_voices.at(index - 1).id
                        : QString();
            });
    connect(comboBoxTtsVoiceQuality,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                m_ttsVoiceQualityFilter = index;
                refreshFilteredVoiceCombo();
            });
    if (comboBoxTtsVoiceQuality->count() > 0) {
        comboBoxTtsVoiceQuality->setCurrentIndex(std::clamp(
                m_ttsVoiceQualityFilter, 0, comboBoxTtsVoiceQuality->count() - 1));
    }
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
    connect(sliderBeatClickVolume,
            &QSlider::valueChanged,
            this,
            [this](int value) { m_beatClickVolumePercent = value; });
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
    connect(checkBoxSmartCue,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bSmartCue = checked; });
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
    connect(checkBoxAnnounceEffects,
            &QCheckBox::toggled,
            this,
            [this](bool checked) { m_bAnnounceEffects = checked; });
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

void DlgPrefAccessibility::populateFeedbackModeCombos() {
    // Order must match the FeedbackMode* settings: 0 speech, 1 sounds, 2 both.
    for (QComboBox* pCombo : {comboBoxFeedbackPlay,
                 comboBoxFeedbackStop,
                 comboBoxFeedbackEndOfTrack,
                 comboBoxFeedbackCue,
                 comboBoxFeedbackRestart,
                 comboBoxFeedbackLoop,
                 comboBoxFeedbackClipping}) {
        pCombo->clear();
        pCombo->addItem(tr("Speech"));
        pCombo->addItem(tr("Sounds"));
        pCombo->addItem(tr("Sounds and speech"));
    }
    // The master preset: index 0 is Custom (shown when the four differ);
    // indexes 1..3 map to modes 0..2 and set all four when chosen.
    comboBoxFeedbackAll->clear();
    comboBoxFeedbackAll->addItem(tr("Custom"));
    comboBoxFeedbackAll->addItem(tr("Speech"));
    comboBoxFeedbackAll->addItem(tr("Sounds"));
    comboBoxFeedbackAll->addItem(tr("Sounds and speech"));
}

void DlgPrefAccessibility::populateMixerStyleCombo() {
    // Order must match MixerReadoutStyle: 0 fractions, 1 percent.
    comboBoxMixerStyle->clear();
    comboBoxMixerStyle->addItem(tr("Fractions (e.g. three quarters)"));
    comboBoxMixerStyle->addItem(tr("Percentages (e.g. 75 percent)"));

    // Order must match MixerFractionDetail: 0 quarters, 1 eighths,
    // 2 sixteenths.
    comboBoxFractionDetail->clear();
    comboBoxFractionDetail->addItem(tr("Quarters (coarse)"));
    comboBoxFractionDetail->addItem(tr("Eighths"));
    comboBoxFractionDetail->addItem(tr("Sixteenths (fine)"));
}

void DlgPrefAccessibility::applyFeedbackPreset(int mode) {
    m_feedbackModePlay = mode;
    m_feedbackModeStop = mode;
    m_feedbackModeEndOfTrack = mode;
    m_feedbackModeCue = mode;
    // Signals are blocked while we set the children so this doesn't recurse
    // back through the per-combo handlers.
    for (QComboBox* pCombo : {comboBoxFeedbackPlay,
                 comboBoxFeedbackStop,
                 comboBoxFeedbackEndOfTrack,
                 comboBoxFeedbackCue}) {
        const QSignalBlocker blocker(pCombo);
        pCombo->setCurrentIndex(mode);
    }
}

void DlgPrefAccessibility::syncFeedbackAllCombo() {
    const bool allEqual = m_feedbackModePlay == m_feedbackModeStop &&
            m_feedbackModePlay == m_feedbackModeEndOfTrack &&
            m_feedbackModePlay == m_feedbackModeCue;
    const QSignalBlocker blocker(comboBoxFeedbackAll);
    comboBoxFeedbackAll->setCurrentIndex(allEqual ? m_feedbackModePlay + 1 : 0);
}

void DlgPrefAccessibility::populateVoiceQualityCombo() {
#ifdef Q_OS_MACOS
    comboBoxTtsVoiceQuality->clear();
    comboBoxTtsVoiceQuality->addItem(tr("All"));
    comboBoxTtsVoiceQuality->addItem(tr("Default"));
    comboBoxTtsVoiceQuality->addItem(tr("Enhanced"));
    comboBoxTtsVoiceQuality->addItem(tr("Premium"));
#else
    // Only macOS's AVSpeechSynthesisVoice reports a real quality tier; other
    // backends have no equivalent concept, so there's nothing to filter by.
    labelVoiceQuality->hide();
    comboBoxTtsVoiceQuality->hide();
#endif
}

void DlgPrefAccessibility::populateVoiceCombo() {
    m_allVoices = TtsEngine::enumerateVoices();
    refreshFilteredVoiceCombo();
}

void DlgPrefAccessibility::refreshFilteredVoiceCombo() {
    m_voices.clear();
    for (const TtsEngine::Voice& voice : m_allVoices) {
        // 0 = show all; 1..3 map to TtsEngine::VoiceQuality + 1.
        if (m_ttsVoiceQualityFilter == 0 ||
                static_cast<int>(voice.quality) + 1 == m_ttsVoiceQualityFilter) {
            m_voices << voice;
        }
    }
    // Block signals: repopulating fires currentIndexChanged as items are
    // added, which would otherwise clobber m_ttsVoiceId with whatever index
    // 0 resolves to before we get a chance to restore the real selection.
    const QSignalBlocker blocker(comboBoxTtsVoice);
    comboBoxTtsVoice->clear();
    comboBoxTtsVoice->addItem(tr("Default (system voice)"));
    for (const TtsEngine::Voice& voice : m_voices) {
        comboBoxTtsVoice->addItem(voice.displayName);
    }
    // Note: this only changes what's visible in the dropdown. If the
    // currently selected voice is filtered out, it shows as "Default (system
    // voice)" here but m_ttsVoiceId is left untouched, so switching the
    // filter back (or applying with the display showing Default) doesn't
    // silently discard the user's actual choice unless they explicitly pick
    // something else from the now-visible list.
    comboBoxTtsVoice->setCurrentIndex(indexForVoiceId(m_ttsVoiceId));
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
    m_ttsVoiceQualityFilter = m_settings.getTtsVoiceQualityFilter();
    if (comboBoxTtsVoiceQuality->count() > 0) {
        const QSignalBlocker blocker(comboBoxTtsVoiceQuality);
        comboBoxTtsVoiceQuality->setCurrentIndex(std::clamp(
                m_ttsVoiceQualityFilter, 0, comboBoxTtsVoiceQuality->count() - 1));
    }
    m_ttsRate = m_settings.getTtsRate();
    m_feedbackModePlay = m_settings.getFeedbackModePlay();
    m_feedbackModeStop = m_settings.getFeedbackModeStop();
    m_feedbackModeEndOfTrack = m_settings.getFeedbackModeEndOfTrack();
    m_feedbackModeCue = m_settings.getFeedbackModeCue();
    comboBoxFeedbackPlay->setCurrentIndex(
            std::clamp(m_feedbackModePlay, 0, comboBoxFeedbackPlay->count() - 1));
    comboBoxFeedbackStop->setCurrentIndex(
            std::clamp(m_feedbackModeStop, 0, comboBoxFeedbackStop->count() - 1));
    comboBoxFeedbackEndOfTrack->setCurrentIndex(std::clamp(
            m_feedbackModeEndOfTrack, 0, comboBoxFeedbackEndOfTrack->count() - 1));
    comboBoxFeedbackCue->setCurrentIndex(
            std::clamp(m_feedbackModeCue, 0, comboBoxFeedbackCue->count() - 1));
    syncFeedbackAllCombo();
    m_feedbackModeRestart = m_settings.getFeedbackModeRestart();
    m_feedbackModeLoop = m_settings.getFeedbackModeLoop();
    comboBoxFeedbackRestart->setCurrentIndex(std::clamp(
            m_feedbackModeRestart, 0, comboBoxFeedbackRestart->count() - 1));
    comboBoxFeedbackLoop->setCurrentIndex(
            std::clamp(m_feedbackModeLoop, 0, comboBoxFeedbackLoop->count() - 1));
    m_feedbackModeClipping = m_settings.getFeedbackModeClipping();
    comboBoxFeedbackClipping->setCurrentIndex(std::clamp(
            m_feedbackModeClipping, 0, comboBoxFeedbackClipping->count() - 1));
    m_bAnnounceClipping = m_settings.getAnnounceClipping();
    checkBoxAnnounceClipping->setChecked(m_bAnnounceClipping);
    m_mixerReadoutStyle = m_settings.getMixerReadoutStyle();
    comboBoxMixerStyle->setCurrentIndex(
            std::clamp(m_mixerReadoutStyle, 0, comboBoxMixerStyle->count() - 1));
    m_mixerFractionDetail = m_settings.getMixerFractionDetail();
    comboBoxFractionDetail->setCurrentIndex(std::clamp(
            m_mixerFractionDetail, 0, comboBoxFractionDetail->count() - 1));
    const double duckStrength = readDuckStrengthControl();
    m_duckStrengthPercent = duckStrength > 0.0
            ? static_cast<int>(std::lround(duckStrength * 100))
            : kDefaultDuckStrengthPercent;
    sliderDuckStrength->setValue(m_duckStrengthPercent);
    const double beatClickVolume = readBeatClickVolumeControl();
    m_beatClickVolumePercent = beatClickVolume > 0.0
            ? static_cast<int>(std::lround(beatClickVolume * 100))
            : kDefaultBeatClickVolumePercent;
    sliderBeatClickVolume->setValue(m_beatClickVolumePercent);
    m_bAnnounceStartup = m_settings.getAnnounceStartup();
    m_bAnnounceSelection = m_settings.getAnnounceTrackSelection();
    m_bAnnounceLoad = m_settings.getAnnounceTrackLoad();
    m_bAnnouncePlay = m_settings.getAnnouncePlay();
    m_bAnnounceCue = m_settings.getAnnounceCue();
    m_bSmartCue = m_settings.getSmartCue();
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
    m_bAnnounceEffects = m_settings.getAnnounceEffects();
    m_bAnnounceMixer = m_settings.getAnnounceMixer();
    m_bAnnounceWhileMoving = m_settings.getAnnounceWhileMoving();
    m_bDeckNumbers = m_settings.getDeckNamesAsNumbers();
    m_bConcise = m_settings.getConciseAnnouncements();
    comboBoxTtsRoute->setCurrentIndex(
            std::clamp(m_ttsRoute, 0, comboBoxTtsRoute->count() - 1));
    refreshFilteredVoiceCombo();
    sliderTtsRate->setValue(m_ttsRate);
    spinBoxTtsRate->setValue(m_ttsRate);
    checkBoxAnnounceStartup->setChecked(m_bAnnounceStartup);
    checkBoxAnnounceSelection->setChecked(m_bAnnounceSelection);
    checkBoxAnnounceLoad->setChecked(m_bAnnounceLoad);
    checkBoxAnnouncePlay->setChecked(m_bAnnouncePlay);
    checkBoxAnnounceCue->setChecked(m_bAnnounceCue);
    checkBoxSmartCue->setChecked(m_bSmartCue);
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
    checkBoxAnnounceEffects->setChecked(m_bAnnounceEffects);
    checkBoxAnnounceMixer->setChecked(m_bAnnounceMixer);
    checkBoxAnnounceWhileMoving->setChecked(m_bAnnounceWhileMoving);
    checkBoxDeckNumbers->setChecked(m_bDeckNumbers);
    checkBoxConcise->setChecked(m_bConcise);
}

void DlgPrefAccessibility::slotApply() {
    m_settings.setTtsRoute(m_ttsRoute);
    m_settings.setTtsVoice(m_ttsVoiceId);
    m_settings.setTtsVoiceQualityFilter(m_ttsVoiceQualityFilter);
    m_settings.setTtsRate(m_ttsRate);
    m_settings.setFeedbackModePlay(m_feedbackModePlay);
    m_settings.setFeedbackModeStop(m_feedbackModeStop);
    m_settings.setFeedbackModeEndOfTrack(m_feedbackModeEndOfTrack);
    m_settings.setFeedbackModeCue(m_feedbackModeCue);
    m_settings.setFeedbackModeRestart(m_feedbackModeRestart);
    m_settings.setFeedbackModeLoop(m_feedbackModeLoop);
    m_settings.setFeedbackModeClipping(m_feedbackModeClipping);
    m_settings.setAnnounceClipping(m_bAnnounceClipping);
    writeDuckStrengthControl(m_duckStrengthPercent);
    writeBeatClickVolumeControl(m_beatClickVolumePercent);
    m_settings.setAnnounceStartup(m_bAnnounceStartup);
    m_settings.setAnnounceTrackSelection(m_bAnnounceSelection);
    m_settings.setAnnounceTrackLoad(m_bAnnounceLoad);
    m_settings.setAnnouncePlay(m_bAnnouncePlay);
    m_settings.setAnnounceCue(m_bAnnounceCue);
    m_settings.setSmartCue(m_bSmartCue);
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
    m_settings.setAnnounceEffects(m_bAnnounceEffects);
    m_settings.setAnnounceMixer(m_bAnnounceMixer);
    m_settings.setAnnounceWhileMoving(m_bAnnounceWhileMoving);
    m_settings.setDeckNamesAsNumbers(m_bDeckNumbers);
    m_settings.setConciseAnnouncements(m_bConcise);
    m_settings.setMixerReadoutStyle(m_mixerReadoutStyle);
    m_settings.setMixerFractionDetail(m_mixerFractionDetail);
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
    m_beatClickVolumePercent = kDefaultBeatClickVolumePercent;
    sliderBeatClickVolume->setValue(m_beatClickVolumePercent);
    m_ttsRoute = m_settings.getTtsRouteDefault();
    m_ttsVoiceId = m_settings.getTtsVoiceDefault();
    m_ttsVoiceQualityFilter = m_settings.getTtsVoiceQualityFilterDefault();
    if (comboBoxTtsVoiceQuality->count() > 0) {
        const QSignalBlocker blocker(comboBoxTtsVoiceQuality);
        comboBoxTtsVoiceQuality->setCurrentIndex(std::clamp(
                m_ttsVoiceQualityFilter, 0, comboBoxTtsVoiceQuality->count() - 1));
    }
    m_ttsRate = m_settings.getTtsRateDefault();
    m_feedbackModePlay = m_settings.getFeedbackModePlayDefault();
    m_feedbackModeStop = m_settings.getFeedbackModeStopDefault();
    m_feedbackModeEndOfTrack = m_settings.getFeedbackModeEndOfTrackDefault();
    m_feedbackModeCue = m_settings.getFeedbackModeCueDefault();
    comboBoxFeedbackPlay->setCurrentIndex(
            std::clamp(m_feedbackModePlay, 0, comboBoxFeedbackPlay->count() - 1));
    comboBoxFeedbackStop->setCurrentIndex(
            std::clamp(m_feedbackModeStop, 0, comboBoxFeedbackStop->count() - 1));
    comboBoxFeedbackEndOfTrack->setCurrentIndex(std::clamp(
            m_feedbackModeEndOfTrack, 0, comboBoxFeedbackEndOfTrack->count() - 1));
    comboBoxFeedbackCue->setCurrentIndex(
            std::clamp(m_feedbackModeCue, 0, comboBoxFeedbackCue->count() - 1));
    syncFeedbackAllCombo();
    m_feedbackModeRestart = m_settings.getFeedbackModeRestartDefault();
    m_feedbackModeLoop = m_settings.getFeedbackModeLoopDefault();
    comboBoxFeedbackRestart->setCurrentIndex(std::clamp(
            m_feedbackModeRestart, 0, comboBoxFeedbackRestart->count() - 1));
    comboBoxFeedbackLoop->setCurrentIndex(
            std::clamp(m_feedbackModeLoop, 0, comboBoxFeedbackLoop->count() - 1));
    m_feedbackModeClipping = m_settings.getFeedbackModeClippingDefault();
    comboBoxFeedbackClipping->setCurrentIndex(std::clamp(
            m_feedbackModeClipping, 0, comboBoxFeedbackClipping->count() - 1));
    m_bAnnounceClipping = m_settings.getAnnounceClippingDefault();
    checkBoxAnnounceClipping->setChecked(m_bAnnounceClipping);
    m_mixerReadoutStyle = m_settings.getMixerReadoutStyleDefault();
    comboBoxMixerStyle->setCurrentIndex(
            std::clamp(m_mixerReadoutStyle, 0, comboBoxMixerStyle->count() - 1));
    m_mixerFractionDetail = m_settings.getMixerFractionDetailDefault();
    comboBoxFractionDetail->setCurrentIndex(std::clamp(
            m_mixerFractionDetail, 0, comboBoxFractionDetail->count() - 1));
    m_bAnnounceStartup = m_settings.getAnnounceStartupDefault();
    m_bAnnounceSelection = m_settings.getAnnounceTrackSelectionDefault();
    m_bAnnounceLoad = m_settings.getAnnounceTrackLoadDefault();
    m_bAnnouncePlay = m_settings.getAnnouncePlayDefault();
    m_bAnnounceCue = m_settings.getAnnounceCueDefault();
    m_bSmartCue = m_settings.getSmartCueDefault();
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
    m_bAnnounceEffects = m_settings.getAnnounceEffectsDefault();
    m_bAnnounceMixer = m_settings.getAnnounceMixerDefault();
    m_bAnnounceWhileMoving = m_settings.getAnnounceWhileMovingDefault();
    m_bDeckNumbers = m_settings.getDeckNamesAsNumbersDefault();
    m_bConcise = m_settings.getConciseAnnouncementsDefault();
    comboBoxTtsRoute->setCurrentIndex(
            std::clamp(m_ttsRoute, 0, comboBoxTtsRoute->count() - 1));
    refreshFilteredVoiceCombo();
    sliderTtsRate->setValue(m_ttsRate);
    spinBoxTtsRate->setValue(m_ttsRate);
    checkBoxAnnounceStartup->setChecked(m_bAnnounceStartup);
    checkBoxAnnounceSelection->setChecked(m_bAnnounceSelection);
    checkBoxAnnounceLoad->setChecked(m_bAnnounceLoad);
    checkBoxAnnouncePlay->setChecked(m_bAnnouncePlay);
    checkBoxAnnounceCue->setChecked(m_bAnnounceCue);
    checkBoxSmartCue->setChecked(m_bSmartCue);
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
    checkBoxAnnounceEffects->setChecked(m_bAnnounceEffects);
    checkBoxAnnounceMixer->setChecked(m_bAnnounceMixer);
    checkBoxAnnounceWhileMoving->setChecked(m_bAnnounceWhileMoving);
    checkBoxDeckNumbers->setChecked(m_bDeckNumbers);
    checkBoxConcise->setChecked(m_bConcise);
}

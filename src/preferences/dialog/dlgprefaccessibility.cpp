#include "preferences/dialog/dlgprefaccessibility.h"

#include <algorithm>

#include "engine/enginetts.h"
#include "moc_dlgprefaccessibility.cpp"

DlgPrefAccessibility::DlgPrefAccessibility(
        QWidget* parent, UserSettingsPointer pConfig, EngineTts* pTtsSink)
        : DlgPreferencePage(parent),
          m_settings(pConfig),
          m_pTtsSink(pTtsSink),
          m_ttsRoute(m_settings.getTtsRouteDefault()),
          m_ttsVoiceId(m_settings.getTtsVoiceDefault()),
          m_ttsRate(m_settings.getTtsRateDefault()),
          m_bAnnounceStartup(m_settings.getAnnounceStartupDefault()),
          m_bAnnounceSelection(m_settings.getAnnounceTrackSelectionDefault()),
          m_bAnnounceLoad(m_settings.getAnnounceTrackLoadDefault()),
          m_bAnnouncePlay(m_settings.getAnnouncePlayDefault()),
          m_bAnnounceCue(m_settings.getAnnounceCueDefault()),
          m_bAnnounceStop(m_settings.getAnnounceStopDefault()),
          m_bAnnounceEndOfTrack(m_settings.getAnnounceEndOfTrackDefault()),
          m_bAnnounceLibraryFocus(m_settings.getAnnounceLibraryFocusDefault()),
          m_bAnnounceSearch(m_settings.getAnnounceSearchDefault()) {
    setupUi(this);
    populateRouteCombo();
    populateVoiceCombo();

    connect(comboBoxTtsRoute,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) { m_ttsRoute = index; });
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
}

void DlgPrefAccessibility::populateRouteCombo() {
    // Order must match EngineTts::Route (0 = Headphones, 1 = Main).
    comboBoxTtsRoute->clear();
    comboBoxTtsRoute->addItem(tr("Headphones / cue (DJ only)"));
    comboBoxTtsRoute->addItem(tr("Main output (audience)"));
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
    m_bAnnounceStartup = m_settings.getAnnounceStartup();
    m_bAnnounceSelection = m_settings.getAnnounceTrackSelection();
    m_bAnnounceLoad = m_settings.getAnnounceTrackLoad();
    m_bAnnouncePlay = m_settings.getAnnouncePlay();
    m_bAnnounceCue = m_settings.getAnnounceCue();
    m_bAnnounceStop = m_settings.getAnnounceStop();
    m_bAnnounceEndOfTrack = m_settings.getAnnounceEndOfTrack();
    m_bAnnounceLibraryFocus = m_settings.getAnnounceLibraryFocus();
    m_bAnnounceSearch = m_settings.getAnnounceSearch();
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
}

void DlgPrefAccessibility::slotApply() {
    m_settings.setTtsRoute(m_ttsRoute);
    m_settings.setTtsVoice(m_ttsVoiceId);
    m_settings.setTtsRate(m_ttsRate);
    m_settings.setAnnounceStartup(m_bAnnounceStartup);
    m_settings.setAnnounceTrackSelection(m_bAnnounceSelection);
    m_settings.setAnnounceTrackLoad(m_bAnnounceLoad);
    m_settings.setAnnouncePlay(m_bAnnouncePlay);
    m_settings.setAnnounceCue(m_bAnnounceCue);
    m_settings.setAnnounceStop(m_bAnnounceStop);
    m_settings.setAnnounceEndOfTrack(m_bAnnounceEndOfTrack);
    m_settings.setAnnounceLibraryFocus(m_bAnnounceLibraryFocus);
    m_settings.setAnnounceSearch(m_bAnnounceSearch);
}

void DlgPrefAccessibility::slotTestSpeech() {
    if (!m_pTtsSink) {
        return;
    }
    m_pTestEngine = TtsEngine::create();
    m_pTestEngine->setSink(m_pTtsSink);
    m_pTtsSink->setRoute(m_ttsRoute);
    if (!m_ttsVoiceId.isEmpty()) {
        m_pTestEngine->setVoice(m_ttsVoiceId);
    }
    m_pTestEngine->setRate(m_ttsRate);
    m_pTestEngine->say(tr("Mixxx ready. Artist, Title. One twenty beats per minute."));
}

void DlgPrefAccessibility::slotResetToDefaults() {
    m_ttsRoute = m_settings.getTtsRouteDefault();
    m_ttsVoiceId = m_settings.getTtsVoiceDefault();
    m_ttsRate = m_settings.getTtsRateDefault();
    m_bAnnounceStartup = m_settings.getAnnounceStartupDefault();
    m_bAnnounceSelection = m_settings.getAnnounceTrackSelectionDefault();
    m_bAnnounceLoad = m_settings.getAnnounceTrackLoadDefault();
    m_bAnnouncePlay = m_settings.getAnnouncePlayDefault();
    m_bAnnounceCue = m_settings.getAnnounceCueDefault();
    m_bAnnounceStop = m_settings.getAnnounceStopDefault();
    m_bAnnounceEndOfTrack = m_settings.getAnnounceEndOfTrackDefault();
    m_bAnnounceLibraryFocus = m_settings.getAnnounceLibraryFocusDefault();
    m_bAnnounceSearch = m_settings.getAnnounceSearchDefault();
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
}

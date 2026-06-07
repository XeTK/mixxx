#include "preferences/dialog/dlgprefaccessibility.h"

#include <algorithm>

#include "moc_dlgprefaccessibility.cpp"

DlgPrefAccessibility::DlgPrefAccessibility(QWidget* parent, UserSettingsPointer pConfig)
        : DlgPreferencePage(parent),
          m_settings(pConfig),
          m_ttsOutputDeviceId(m_settings.getTtsOutputDeviceDefault()),
          m_ttsOutputChannel(m_settings.getTtsOutputChannelDefault()),
          m_ttsVoiceId(m_settings.getTtsVoiceDefault()),
          m_ttsRate(m_settings.getTtsRateDefault()),
          m_bAnnounceStartup(m_settings.getAnnounceStartupDefault()),
          m_bAnnounceSelection(m_settings.getAnnounceTrackSelectionDefault()),
          m_bAnnounceLoad(m_settings.getAnnounceTrackLoadDefault()),
          m_bAnnouncePlay(m_settings.getAnnouncePlayDefault()),
          m_bAnnounceStop(m_settings.getAnnounceStopDefault()),
          m_bAnnounceEndOfTrack(m_settings.getAnnounceEndOfTrackDefault()),
          m_bAnnounceLibraryFocus(m_settings.getAnnounceLibraryFocusDefault()),
          m_bAnnounceSearch(m_settings.getAnnounceSearchDefault()) {
    setupUi(this);
    populateDeviceCombo();
    populateChannelCombo();
    populateVoiceCombo();

    connect(comboBoxTtsOutputDevice,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                m_ttsOutputDeviceId = (index > 0 && index <= m_outputDevices.size())
                        ? m_outputDevices.at(index - 1).id
                        : QString();
                // Channel routing only applies when a specific device is selected.
                comboBoxTtsOutputChannel->setEnabled(index > 0);
            });
    connect(comboBoxTtsOutputChannel,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) { m_ttsOutputChannel = index; });
    connect(comboBoxTtsVoice,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                m_ttsVoiceId = (index > 0 && index <= m_voices.size())
                        ? m_voices.at(index - 1).id
                        : QString();
            });
    connect(spinBoxTtsRate,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value) { m_ttsRate = value; });
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

void DlgPrefAccessibility::populateDeviceCombo() {
    m_outputDevices = TtsEngine::enumerateOutputDevices();
    comboBoxTtsOutputDevice->clear();
    comboBoxTtsOutputDevice->addItem(tr("Default (system audio output)"));
    for (const TtsEngine::AudioOutputDevice& dev : m_outputDevices) {
        comboBoxTtsOutputDevice->addItem(dev.displayName);
    }
}

void DlgPrefAccessibility::populateChannelCombo() {
    comboBoxTtsOutputChannel->clear();
    comboBoxTtsOutputChannel->addItem(tr("Channels 1–2 (default)"));
    comboBoxTtsOutputChannel->addItem(tr("Channels 3–4"));
    comboBoxTtsOutputChannel->addItem(tr("Channels 5–6"));
    comboBoxTtsOutputChannel->addItem(tr("Channels 7–8"));
}

void DlgPrefAccessibility::populateVoiceCombo() {
    m_voices = TtsEngine::enumerateVoices();
    comboBoxTtsVoice->clear();
    comboBoxTtsVoice->addItem(tr("Default (system voice)"));
    for (const TtsEngine::Voice& voice : m_voices) {
        comboBoxTtsVoice->addItem(voice.displayName);
    }
}

int DlgPrefAccessibility::indexForDeviceId(const QString& deviceId) const {
    if (deviceId.isEmpty()) {
        return 0;
    }
    for (int i = 0; i < m_outputDevices.size(); ++i) {
        if (m_outputDevices.at(i).id == deviceId) {
            return i + 1;
        }
    }
    return 0;
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
    m_ttsOutputDeviceId = m_settings.getTtsOutputDevice();
    m_ttsOutputChannel = m_settings.getTtsOutputChannel();
    m_ttsVoiceId = m_settings.getTtsVoice();
    m_ttsRate = m_settings.getTtsRate();
    m_bAnnounceStartup = m_settings.getAnnounceStartup();
    m_bAnnounceSelection = m_settings.getAnnounceTrackSelection();
    m_bAnnounceLoad = m_settings.getAnnounceTrackLoad();
    m_bAnnouncePlay = m_settings.getAnnouncePlay();
    m_bAnnounceStop = m_settings.getAnnounceStop();
    m_bAnnounceEndOfTrack = m_settings.getAnnounceEndOfTrack();
    m_bAnnounceLibraryFocus = m_settings.getAnnounceLibraryFocus();
    m_bAnnounceSearch = m_settings.getAnnounceSearch();
    comboBoxTtsOutputDevice->setCurrentIndex(indexForDeviceId(m_ttsOutputDeviceId));
    comboBoxTtsOutputChannel->setCurrentIndex(
            std::clamp(m_ttsOutputChannel, 0, comboBoxTtsOutputChannel->count() - 1));
    comboBoxTtsOutputChannel->setEnabled(!m_ttsOutputDeviceId.isEmpty());
    comboBoxTtsVoice->setCurrentIndex(indexForVoiceId(m_ttsVoiceId));
    spinBoxTtsRate->setValue(m_ttsRate);
    checkBoxAnnounceStartup->setChecked(m_bAnnounceStartup);
    checkBoxAnnounceSelection->setChecked(m_bAnnounceSelection);
    checkBoxAnnounceLoad->setChecked(m_bAnnounceLoad);
    checkBoxAnnouncePlay->setChecked(m_bAnnouncePlay);
    checkBoxAnnounceStop->setChecked(m_bAnnounceStop);
    checkBoxAnnounceEndOfTrack->setChecked(m_bAnnounceEndOfTrack);
    checkBoxAnnounceLibraryFocus->setChecked(m_bAnnounceLibraryFocus);
    checkBoxAnnounceSearch->setChecked(m_bAnnounceSearch);
}

void DlgPrefAccessibility::slotApply() {
    m_settings.setTtsOutputDevice(m_ttsOutputDeviceId);
    m_settings.setTtsOutputChannel(m_ttsOutputChannel);
    m_settings.setTtsVoice(m_ttsVoiceId);
    m_settings.setTtsRate(m_ttsRate);
    m_settings.setAnnounceStartup(m_bAnnounceStartup);
    m_settings.setAnnounceTrackSelection(m_bAnnounceSelection);
    m_settings.setAnnounceTrackLoad(m_bAnnounceLoad);
    m_settings.setAnnouncePlay(m_bAnnouncePlay);
    m_settings.setAnnounceStop(m_bAnnounceStop);
    m_settings.setAnnounceEndOfTrack(m_bAnnounceEndOfTrack);
    m_settings.setAnnounceLibraryFocus(m_bAnnounceLibraryFocus);
    m_settings.setAnnounceSearch(m_bAnnounceSearch);
}

void DlgPrefAccessibility::slotResetToDefaults() {
    m_ttsOutputDeviceId = m_settings.getTtsOutputDeviceDefault();
    m_ttsOutputChannel = m_settings.getTtsOutputChannelDefault();
    m_ttsVoiceId = m_settings.getTtsVoiceDefault();
    m_ttsRate = m_settings.getTtsRateDefault();
    m_bAnnounceStartup = m_settings.getAnnounceStartupDefault();
    m_bAnnounceSelection = m_settings.getAnnounceTrackSelectionDefault();
    m_bAnnounceLoad = m_settings.getAnnounceTrackLoadDefault();
    m_bAnnouncePlay = m_settings.getAnnouncePlayDefault();
    m_bAnnounceStop = m_settings.getAnnounceStopDefault();
    m_bAnnounceEndOfTrack = m_settings.getAnnounceEndOfTrackDefault();
    m_bAnnounceLibraryFocus = m_settings.getAnnounceLibraryFocusDefault();
    m_bAnnounceSearch = m_settings.getAnnounceSearchDefault();
    comboBoxTtsOutputDevice->setCurrentIndex(indexForDeviceId(m_ttsOutputDeviceId));
    comboBoxTtsOutputChannel->setCurrentIndex(
            std::clamp(m_ttsOutputChannel, 0, comboBoxTtsOutputChannel->count() - 1));
    comboBoxTtsOutputChannel->setEnabled(!m_ttsOutputDeviceId.isEmpty());
    comboBoxTtsVoice->setCurrentIndex(indexForVoiceId(m_ttsVoiceId));
    spinBoxTtsRate->setValue(m_ttsRate);
    checkBoxAnnounceStartup->setChecked(m_bAnnounceStartup);
    checkBoxAnnounceSelection->setChecked(m_bAnnounceSelection);
    checkBoxAnnounceLoad->setChecked(m_bAnnounceLoad);
    checkBoxAnnouncePlay->setChecked(m_bAnnouncePlay);
    checkBoxAnnounceStop->setChecked(m_bAnnounceStop);
    checkBoxAnnounceEndOfTrack->setChecked(m_bAnnounceEndOfTrack);
    checkBoxAnnounceLibraryFocus->setChecked(m_bAnnounceLibraryFocus);
    checkBoxAnnounceSearch->setChecked(m_bAnnounceSearch);
}

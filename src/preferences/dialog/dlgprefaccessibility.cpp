#include "preferences/dialog/dlgprefaccessibility.h"

#include "moc_dlgprefaccessibility.cpp"

DlgPrefAccessibility::DlgPrefAccessibility(QWidget* parent, UserSettingsPointer pConfig)
        : DlgPreferencePage(parent),
          m_settings(pConfig),
          m_ttsOutputDeviceId(m_settings.getTtsOutputDeviceDefault()),
          m_bAnnounceStartup(m_settings.getAnnounceStartupDefault()),
          m_bAnnounceSelection(m_settings.getAnnounceTrackSelectionDefault()),
          m_bAnnounceLoad(m_settings.getAnnounceTrackLoadDefault()),
          m_bAnnouncePlay(m_settings.getAnnouncePlayDefault()),
          m_bAnnounceStop(m_settings.getAnnounceStopDefault()),
          m_bAnnounceEndOfTrack(m_settings.getAnnounceEndOfTrackDefault()),
          m_bAnnounceLibraryFocus(m_settings.getAnnounceLibraryFocusDefault()) {
    setupUi(this);
    populateDeviceCombo();

    connect(comboBoxTtsOutputDevice,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                m_ttsOutputDeviceId = (index > 0 && index <= m_outputDevices.size())
                        ? m_outputDevices.at(index - 1).id
                        : QString();
            });
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
}

void DlgPrefAccessibility::populateDeviceCombo() {
    m_outputDevices = TtsEngine::enumerateOutputDevices();
    comboBoxTtsOutputDevice->clear();
    comboBoxTtsOutputDevice->addItem(tr("Default (system audio output)"));
    for (const TtsEngine::AudioOutputDevice& dev : m_outputDevices) {
        comboBoxTtsOutputDevice->addItem(dev.displayName);
    }
}

int DlgPrefAccessibility::indexForDeviceId(const QString& deviceId) const {
    if (deviceId.isEmpty()) {
        return 0;
    }
    for (int i = 0; i < m_outputDevices.size(); ++i) {
        if (m_outputDevices.at(i).id == deviceId) {
            return i + 1; // +1 for the "Default" entry at index 0
        }
    }
    return 0; // fall back to default if not found
}

void DlgPrefAccessibility::slotUpdate() {
    m_ttsOutputDeviceId = m_settings.getTtsOutputDevice();
    m_bAnnounceStartup = m_settings.getAnnounceStartup();
    m_bAnnounceSelection = m_settings.getAnnounceTrackSelection();
    m_bAnnounceLoad = m_settings.getAnnounceTrackLoad();
    m_bAnnouncePlay = m_settings.getAnnouncePlay();
    m_bAnnounceStop = m_settings.getAnnounceStop();
    m_bAnnounceEndOfTrack = m_settings.getAnnounceEndOfTrack();
    m_bAnnounceLibraryFocus = m_settings.getAnnounceLibraryFocus();
    comboBoxTtsOutputDevice->setCurrentIndex(indexForDeviceId(m_ttsOutputDeviceId));
    checkBoxAnnounceStartup->setChecked(m_bAnnounceStartup);
    checkBoxAnnounceSelection->setChecked(m_bAnnounceSelection);
    checkBoxAnnounceLoad->setChecked(m_bAnnounceLoad);
    checkBoxAnnouncePlay->setChecked(m_bAnnouncePlay);
    checkBoxAnnounceStop->setChecked(m_bAnnounceStop);
    checkBoxAnnounceEndOfTrack->setChecked(m_bAnnounceEndOfTrack);
    checkBoxAnnounceLibraryFocus->setChecked(m_bAnnounceLibraryFocus);
}

void DlgPrefAccessibility::slotApply() {
    m_settings.setTtsOutputDevice(m_ttsOutputDeviceId);
    m_settings.setAnnounceStartup(m_bAnnounceStartup);
    m_settings.setAnnounceTrackSelection(m_bAnnounceSelection);
    m_settings.setAnnounceTrackLoad(m_bAnnounceLoad);
    m_settings.setAnnouncePlay(m_bAnnouncePlay);
    m_settings.setAnnounceStop(m_bAnnounceStop);
    m_settings.setAnnounceEndOfTrack(m_bAnnounceEndOfTrack);
    m_settings.setAnnounceLibraryFocus(m_bAnnounceLibraryFocus);
}

void DlgPrefAccessibility::slotResetToDefaults() {
    m_ttsOutputDeviceId = m_settings.getTtsOutputDeviceDefault();
    m_bAnnounceStartup = m_settings.getAnnounceStartupDefault();
    m_bAnnounceSelection = m_settings.getAnnounceTrackSelectionDefault();
    m_bAnnounceLoad = m_settings.getAnnounceTrackLoadDefault();
    m_bAnnouncePlay = m_settings.getAnnouncePlayDefault();
    m_bAnnounceStop = m_settings.getAnnounceStopDefault();
    m_bAnnounceEndOfTrack = m_settings.getAnnounceEndOfTrackDefault();
    m_bAnnounceLibraryFocus = m_settings.getAnnounceLibraryFocusDefault();
    comboBoxTtsOutputDevice->setCurrentIndex(indexForDeviceId(m_ttsOutputDeviceId));
    checkBoxAnnounceStartup->setChecked(m_bAnnounceStartup);
    checkBoxAnnounceSelection->setChecked(m_bAnnounceSelection);
    checkBoxAnnounceLoad->setChecked(m_bAnnounceLoad);
    checkBoxAnnouncePlay->setChecked(m_bAnnouncePlay);
    checkBoxAnnounceStop->setChecked(m_bAnnounceStop);
    checkBoxAnnounceEndOfTrack->setChecked(m_bAnnounceEndOfTrack);
    checkBoxAnnounceLibraryFocus->setChecked(m_bAnnounceLibraryFocus);
}

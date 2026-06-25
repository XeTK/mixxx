#include "preferences/dialog/dlgprefaccessibilityenhanced.h"

#include "core/services/coreservices.h"
#include "mixxxapplication.h"
#include "mixxxmainwindow.h"
#include "util/announcementmanagerenhanced.h"

DlgPrefAccessibilityEnhanced::DlgPrefAccessibilityEnhanced(
        QWidget* parent,
        UserSettingsPointer pConfig,
        EngineTts* pTtsSink)
        : DlgPrefAccessibility(parent, pConfig, pTtsSink),
          m_accessibilitySettings(pConfig),
          m_pDeckNamingCombo(nullptr),
          m_pEnableTtsByDefaultCheckbox(nullptr),
          m_pAnnounceCueCheckbox(nullptr),
          m_pAnnounceTrackLoadCheckbox(nullptr),
          m_pAnnouncePlayCheckbox(nullptr),
          m_pAnnounceStopCheckbox(nullptr),
          m_pAnnounceEndOfTrackCheckbox(nullptr) {
    setupUi();
    connectSignals();
    load();
}

DlgPrefAccessibilityEnhanced::~DlgPrefAccessibilityEnhanced() = default;

void DlgPrefAccessibilityEnhanced::setupUi() {
    // Create main layout
    QVBoxLayout* pMainLayout = new QVBoxLayout(this);

    // Create group boxes
    QGroupBox* pDeckSettingsBox = new QGroupBox(tr("Deck Settings"));
    QVBoxLayout* pDeckLayout = new QVBoxLayout(pDeckSettingsBox);

    QGroupBox* pTtsSettingsBox = new QGroupBox(tr("Text-to-Speech Settings"));
    QVBoxLayout* pTtsLayout = new QVBoxLayout(pTtsSettingsBox);

    QGroupBox* pAnnouncementsBox = new QGroupBox(tr("Announcement Settings"));
    QVBoxLayout* pAnnouncementsLayout = new QVBoxLayout(pAnnouncementsBox);

    // Deck naming convention
    QLabel* pDeckNamingLabel = new QLabel(tr("Deck Naming Convention:"));
    m_pDeckNamingCombo = new QComboBox();
    m_pDeckNamingCombo->addItem(tr("Deck 1"), "Deck1");
    m_pDeckNamingCombo->addItem(tr("Deck A"), "DeckA");
    m_pDeckNamingCombo->setToolTip(tr("Choose how decks are named in audio announcements"));

    // Enable TTS by default
    m_pEnableTtsByDefaultCheckbox = new QCheckBox(tr("Enable TTS by Default"));
    m_pEnableTtsByDefaultCheckbox->setToolTip(tr("Enable Text-to-Speech announcements by default"));

    // Announcement checkboxes (enhanced with new settings)
    m_pAnnounceCueCheckbox = new QCheckBox(tr("Announce Cue Button"));
    m_pAnnounceCueCheckbox->setToolTip(tr("Announce when cue mode is activated or deactivated"));

    m_pAnnounceTrackLoadCheckbox = new QCheckBox(tr("Announce Track Load"));
    m_pAnnounceTrackLoadCheckbox->setToolTip(tr("Announce when tracks are loaded to decks"));

    m_pAnnouncePlayCheckbox = new QCheckBox(tr("Announce Play"));
    m_pAnnouncePlayCheckbox->setToolTip(tr("Announce when decks start playing"));

    m_pAnnounceStopCheckbox = new QCheckBox(tr("Announce Stop"));
    m_pAnnounceStopCheckbox->setToolTip(tr("Announce when decks are stopped"));

    m_pAnnounceEndOfTrackCheckbox = new QCheckBox(tr("Announce End of Track"));
    m_pAnnounceEndOfTrackCheckbox->setToolTip(tr("Announce when tracks finish playing"));

    // Add elements to layouts
    pDeckLayout->addWidget(pDeckNamingLabel);
    pDeckLayout->addWidget(m_pDeckNamingCombo);

    pTtsLayout->addWidget(m_pEnableTtsByDefaultCheckbox);

    pAnnouncementsLayout->addWidget(m_pAnnounceCueCheckbox);
    pAnnouncementsLayout->addWidget(m_pAnnounceTrackLoadCheckbox);
    pAnnouncementsLayout->addWidget(m_pAnnouncePlayCheckbox);
    pAnnouncementsLayout->addWidget(m_pAnnounceStopCheckbox);
    pAnnouncementsLayout->addWidget(m_pAnnounceEndOfTrackCheckbox);

    // Add group boxes to main layout
    pMainLayout->addWidget(pDeckSettingsBox);
    pMainLayout->addWidget(pTtsSettingsBox);
    pMainLayout->addWidget(pAnnouncementsBox);

    // Add stretch to push everything to the top
    pMainLayout->addStretch();
}

void DlgPrefAccessibilityEnhanced::connectSignals() {
    connect(m_pDeckNamingCombo,
            &QComboBox::currentTextChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onDeckNamingConventionChanged);
    connect(m_pEnableTtsByDefaultCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onEnableTtsByDefaultChanged);
    connect(m_pAnnounceCueCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnounceCueChanged);
    connect(m_pAnnounceTrackLoadCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnounceTrackLoadChanged);
    connect(m_pAnnouncePlayCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnouncePlayChanged);
    connect(m_pAnnounceStopCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnounceStopChanged);
    connect(m_pAnnounceEndOfTrackCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnounceEndOfTrackChanged);
}

void DlgPrefAccessibilityEnhanced::load() {
    // Load deck naming convention
    QString deckNaming = m_accessibilitySettings.DeckNamingConvention();
    int index = m_pDeckNamingCombo->findData(deckNaming);
    if (index >= 0) {
        m_pDeckNamingCombo->setCurrentIndex(index);
    }

    // Load TTS enablement
    m_pEnableTtsByDefaultCheckbox->setChecked(m_accessibilitySettings.EnableTtsByDefault());

    // Load announcement settings
    m_pAnnounceCueCheckbox->setChecked(m_accessibilitySettings.AnnounceCue());
    m_pAnnounceTrackLoadCheckbox->setChecked(m_accessibilitySettings.AnnounceTrackLoad());
    m_pAnnouncePlayCheckbox->setChecked(m_accessibilitySettings.AnnouncePlay());
    m_pAnnounceStopCheckbox->setChecked(m_accessibilitySettings.AnnounceStop());
    m_pAnnounceEndOfTrackCheckbox->setChecked(m_accessibilitySettings.AnnounceEndOfTrack());
}

void DlgPrefAccessibilityEnhanced::apply() {
    // Save deck naming convention
    QString deckNaming = m_pDeckNamingCombo->currentData().toString();
    m_accessibilitySettings.setDeckNamingConvention(deckNaming);

    // Save TTS enablement
    m_accessibilitySettings.setEnableTtsByDefault(m_pEnableTtsByDefaultCheckbox->isChecked());

    // Save announcement settings
    m_accessibilitySettings.setAnnounceCue(m_pAnnounceCueCheckbox->isChecked());
    m_accessibilitySettings.setAnnounceTrackLoad(m_pAnnounceTrackLoadCheckbox->isChecked());
    m_accessibilitySettings.setAnnouncePlay(m_pAnnouncePlayCheckbox->isChecked());
    m_accessibilitySettings.setAnnounceStop(m_pAnnounceStopCheckbox->isChecked());
    m_accessibilitySettings.setAnnounceEndOfTrack(m_pAnnounceEndOfTrackCheckbox->isChecked());

    // Notify core services of the change
    auto pApp = MixxxApplication::instance();
    if (pApp && pApp->getMainWindow()) {
        auto pAnnouncementManager = pApp->getMainWindow()->getAnnouncementManager();
        if (pAnnouncementManager) {
            // Ensure enhanced manager is properly updated if needed
            if (auto pEnhancedManager =
                            dynamic_cast<EnhancedAnnouncementManager*>(
                                    pAnnouncementManager)) {
                pEnhancedManager->setDeckNamingConvention(deckNaming);
            }
        }
    }
}

void DlgPrefAccessibilityEnhanced::onDeckNamingConventionChanged(const QString& convention) {
    // This is handled in apply() when the preferences are saved
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onEnableTtsByDefaultChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnounceCueChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnounceTrackLoadChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnouncePlayChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnounceStopChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnounceEndOfTrackChanged(int state) {
    emit changed();
}

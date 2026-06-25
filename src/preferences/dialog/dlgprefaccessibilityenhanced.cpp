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
          m_pAnnounceEqCheckbox(nullptr),
          m_pAnnounceFilterCheckbox(nullptr),
          m_pAnnounceTrimCheckbox(nullptr),
          m_pAnnounceMasterCheckbox(nullptr),
          m_pAnnounceMixCheckbox(nullptr),
          m_pAnnounceEffectCheckbox(nullptr),
          m_pAnnounceSyncCheckbox(nullptr),
          m_pAnnounceTempoCheckbox(nullptr),
          m_pAnnounceCrossFaderCheckbox(nullptr),
          m_pAnnounceFaderChangeCheckbox(nullptr),
          m_pAnnouncePreventJoggingCheckbox(nullptr),
          m_pAnnounceTouchSurfaceCheckbox(nullptr),
          m_pAnnounceTtsToggleCheckbox(nullptr) {
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

    // Enhanced announcement checkboxes
    m_pAnnounceEqCheckbox = new QCheckBox(tr("Announce EQ Parameters"));
    m_pAnnounceEqCheckbox->setToolTip(tr("Announce when EQ parameters are adjusted"));

    m_pAnnounceFilterCheckbox = new QCheckBox(tr("Announce Filter Parameters"));
    m_pAnnounceFilterCheckbox->setToolTip(tr("Announce when filter parameters are adjusted"));

    m_pAnnounceTrimCheckbox = new QCheckBox(tr("Announce Trim Parameters"));
    m_pAnnounceTrimCheckbox->setToolTip(tr("Announce when trim parameters are adjusted"));

    m_pAnnounceMasterCheckbox = new QCheckBox(tr("Announce Master Parameters"));
    m_pAnnounceMasterCheckbox->setToolTip(tr("Announce when master parameters are adjusted"));

    m_pAnnounceMixCheckbox = new QCheckBox(tr("Announce Mix Parameters"));
    m_pAnnounceMixCheckbox->setToolTip(tr("Announce when mix parameters are adjusted"));

    m_pAnnounceEffectCheckbox = new QCheckBox(tr("Announce Effect Selection"));
    m_pAnnounceEffectCheckbox->setToolTip(tr("Announce when an effect is selected"));

    m_pAnnounceSyncCheckbox = new QCheckBox(tr("Announce Sync Button"));
    m_pAnnounceSyncCheckbox->setToolTip(tr("Announce sync button state changes"));

    m_pAnnounceTempoCheckbox = new QCheckBox(tr("Announce Tempo Changes"));
    m_pAnnounceTempoCheckbox->setToolTip(tr("Announce when tempo is changed"));

    m_pAnnounceCrossFaderCheckbox = new QCheckBox(tr("Announce Crossfader Changes"));
    m_pAnnounceCrossFaderCheckbox->setToolTip(tr("Announce when crossfader is moved"));

    m_pAnnounceFaderChangeCheckbox = new QCheckBox(tr("Announce Fader Changes"));
    m_pAnnounceFaderChangeCheckbox->setToolTip(tr("Announce when faders are adjusted"));

    m_pAnnouncePreventJoggingCheckbox = new QCheckBox(tr("Announce Prevent Jogging"));
    m_pAnnouncePreventJoggingCheckbox->setToolTip(tr("Announce prevent jogging setting changes"));

    m_pAnnounceTouchSurfaceCheckbox = new QCheckBox(tr("Announce Touch Surface"));
    m_pAnnounceTouchSurfaceCheckbox->setToolTip(tr("Announce touch surface state changes"));

    m_pAnnounceTtsToggleCheckbox = new QCheckBox(tr("Announce TTS Toggle"));
    m_pAnnounceTtsToggleCheckbox->setToolTip(tr("Announce when TTS is enabled/disabled"));

    // Add elements to layouts
    pDeckLayout->addWidget(pDeckNamingLabel);
    pDeckLayout->addWidget(m_pDeckNamingCombo);

    pTtsLayout->addWidget(m_pEnableTtsByDefaultCheckbox);

    pAnnouncementsLayout->addWidget(m_pAnnounceEqCheckbox);
    pAnnouncementsLayout->addWidget(m_pAnnounceFilterCheckbox);
    pAnnouncementsLayout->addWidget(m_pAnnounceTrimCheckbox);
    pAnnouncementsLayout->addWidget(m_pAnnounceMasterCheckbox);
    pAnnouncementsLayout->addWidget(m_pAnnounceMixCheckbox);
    pAnnouncementsLayout->addWidget(m_pAnnounceEffectCheckbox);
    pAnnouncementsLayout->addWidget(m_pAnnounceSyncCheckbox);
    pAnnouncementsLayout->addWidget(m_pAnnounceTempoCheckbox);
    pAnnouncementsLayout->addWidget(m_pAnnounceCrossFaderCheckbox);
    pAnnouncementsLayout->addWidget(m_pAnnounceFaderChangeCheckbox);
    pAnnouncementsLayout->addWidget(m_pAnnouncePreventJoggingCheckbox);
    pAnnouncementsLayout->addWidget(m_pAnnounceTouchSurfaceCheckbox);
    pAnnouncementsLayout->addWidget(m_pAnnounceTtsToggleCheckbox);

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
    connect(m_pAnnounceEqCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnounceEqChanged);
    connect(m_pAnnounceFilterCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnounceFilterChanged);
    connect(m_pAnnounceTrimCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnounceTrimChanged);
    connect(m_pAnnounceMasterCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnounceMasterChanged);
    connect(m_pAnnounceMixCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnounceMixChanged);
    connect(m_pAnnounceEffectCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnounceEffectChanged);
    connect(m_pAnnounceSyncCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnounceSyncChanged);
    connect(m_pAnnounceTempoCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnounceTempoChanged);
    connect(m_pAnnounceCrossFaderCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnounceCrossFaderChanged);
    connect(m_pAnnounceFaderChangeCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnounceFaderChangeChanged);
    connect(m_pAnnouncePreventJoggingCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnouncePreventJoggingChanged);
    connect(m_pAnnounceTouchSurfaceCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnounceTouchSurfaceChanged);
    connect(m_pAnnounceTtsToggleCheckbox,
            &QCheckBox::stateChanged,
            this,
            &DlgPrefAccessibilityEnhanced::onAnnounceTtsToggleChanged);
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
    m_pAnnounceEqCheckbox->setChecked(m_accessibilitySettings.AnnounceEq());
    m_pAnnounceFilterCheckbox->setChecked(m_accessibilitySettings.AnnounceFilter());
    m_pAnnounceTrimCheckbox->setChecked(m_accessibilitySettings.AnnounceTrim());
    m_pAnnounceMasterCheckbox->setChecked(m_accessibilitySettings.AnnounceMaster());
    m_pAnnounceMixCheckbox->setChecked(m_accessibilitySettings.AnnounceMix());
    m_pAnnounceEffectCheckbox->setChecked(m_accessibilitySettings.AnnounceEffect());
    m_pAnnounceSyncCheckbox->setChecked(m_accessibilitySettings.AnnounceSync());
    m_pAnnounceTempoCheckbox->setChecked(m_accessibilitySettings.AnnounceTempo());
    m_pAnnounceCrossFaderCheckbox->setChecked(m_accessibilitySettings.AnnounceCrossFader());
    m_pAnnounceFaderChangeCheckbox->setChecked(m_accessibilitySettings.AnnounceFaderChange());
    m_pAnnouncePreventJoggingCheckbox->setChecked(m_accessibilitySettings.AnnouncePreventJogging());
    m_pAnnounceTouchSurfaceCheckbox->setChecked(m_accessibilitySettings.AnnounceTouchSurface());
    m_pAnnounceTtsToggleCheckbox->setChecked(m_accessibilitySettings.AnnounceTtsToggle());
}

void DlgPrefAccessibilityEnhanced::apply() {
    // Save deck naming convention
    QString deckNaming = m_pDeckNamingCombo->currentData().toString();
    m_accessibilitySettings.setDeckNamingConvention(deckNaming);

    // Save TTS enablement
    m_accessibilitySettings.setEnableTtsByDefault(m_pEnableTtsByDefaultCheckbox->isChecked());

    // Save announcement settings
    m_accessibilitySettings.setAnnounceEq(m_pAnnounceEqCheckbox->isChecked());
    m_accessibilitySettings.setAnnounceFilter(m_pAnnounceFilterCheckbox->isChecked());
    m_accessibilitySettings.setAnnounceTrim(m_pAnnounceTrimCheckbox->isChecked());
    m_accessibilitySettings.setAnnounceMaster(m_pAnnounceMasterCheckbox->isChecked());
    m_accessibilitySettings.setAnnounceMix(m_pAnnounceMixCheckbox->isChecked());
    m_accessibilitySettings.setAnnounceEffect(m_pAnnounceEffectCheckbox->isChecked());
    m_accessibilitySettings.setAnnounceSync(m_pAnnounceSyncCheckbox->isChecked());
    m_accessibilitySettings.setAnnounceTempo(m_pAnnounceTempoCheckbox->isChecked());
    m_accessibilitySettings.setAnnounceCrossFader(m_pAnnounceCrossFaderCheckbox->isChecked());
    m_accessibilitySettings.setAnnounceFaderChange(m_pAnnounceFaderChangeCheckbox->isChecked());
    m_accessibilitySettings.setAnnouncePreventJogging(
            m_pAnnouncePreventJoggingCheckbox->isChecked());
    m_accessibilitySettings.setAnnounceTouchSurface(m_pAnnounceTouchSurfaceCheckbox->isChecked());
    m_accessibilitySettings.setAnnounceTtsToggle(m_pAnnounceTtsToggleCheckbox->isChecked());

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
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onEnableTtsByDefaultChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnounceEqChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnounceFilterChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnounceTrimChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnounceMasterChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnounceMixChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnounceEffectChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnounceSyncChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnounceTempoChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnounceCrossFaderChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnounceFaderChangeChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnouncePreventJoggingChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnounceTouchSurfaceChanged(int state) {
    emit changed();
}

void DlgPrefAccessibilityEnhanced::onAnnounceTtsToggleChanged(int state) {
    emit changed();
}

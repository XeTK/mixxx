#include "preferences/dialog/dlgpreferences.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "core/services/coreservices.h"
#include "engine/enginetts.h"
#include "preferences/dialog/dlgprefaccessibility.h"
#include "preferences/dialog/dlgprefaccessibilityenhanced.h"
#include "preferences/dialog/dlgprefautodj.h"
#include "preferences/dialog/dlgprefbeats.h"
#include "preferences/dialog/dlgprefbroadcast.h"
#include "preferences/dialog/dlgprefcolors.h"
#include "preferences/dialog/dlgprefdeck.h"
#include "preferences/dialog/dlgprefeffects.h"
#include "preferences/dialog/dlgprefinterface.h"
#include "preferences/dialog/dlgprefkey.h"
#include "preferences/dialog/dlgpreflibrary.h"
#include "preferences/dialog/dlgprefmixer.h"
#include "preferences/dialog/dlgprefmodplug.h"
#include "preferences/dialog/dlgprefrecord.h"
#include "preferences/dialog/dlgprefreplaygain.h"
#include "preferences/dialog/dlgprefsound.h"
#include "preferences/dialog/dlgprefvinyl.h"
#include "preferences/dialog/dlgprefwaveform.h"
#include "util/announcementmanagerenhanced.h"

DlgPreferences::DlgPreferences(
        ScreensaverManager* pScreensaverManager,
        SkinLoader* pSkinLoader,
        UserSettingsPointer pConfig,
        EngineTts* pTtsSink,
        QWidget* parent)
        : QDialog(parent),
          m_pConfig(std::move(pConfig)),
          m_pTtsSink(pTtsSink),
          m_pScreensaverManager(pScreensaverManager),
          m_pSkinLoader(pSkinLoader),
          m_pDlgPrefAccessibility(nullptr),
          m_pDlgPrefAccessibilityEnhanced(nullptr),
          m_pDlgPrefAutodj(nullptr),
          m_pDlgPrefBeats(nullptr),
          m_pDlgPrefBroadcast(nullptr),
          m_pDlgPrefColors(nullptr),
          m_pDlgPrefDeck(nullptr),
          m_pDlgPrefEffects(nullptr),
          m_pDlgPrefInterface(nullptr),
          m_pDlgPrefKey(nullptr),
          m_pDlgPrefLibrary(nullptr),
          m_pDlgPrefMixer(nullptr),
          m_pDlgPrefModplug(nullptr),
          m_pDlgPrefRecord(nullptr),
          m_pDlgPrefReplayGain(nullptr),
          m_pDlgPrefSound(nullptr),
          m_pDlgPrefVinyl(nullptr),
          m_pDlgPrefWaveform(nullptr),
          m_pPageContainer(nullptr),
          m_pPageList(nullptr),
          m_pButtonBox(nullptr),
          m_pMainLayout(nullptr) {
    setupUi();
    load();
    connectSignals();
    updatePageList();
}

DlgPreferences::~DlgPreferences() {
    // Clean up the dynamically allocated dialogs
    delete m_pDlgPrefAccessibility;
    delete m_pDlgPrefAccessibilityEnhanced;
    delete m_pDlgPrefAutodj;
    delete m_pDlgPrefBeats;
    delete m_pDlgPrefBroadcast;
    delete m_pDlgPrefColors;
    delete m_pDlgPrefDeck;
    delete m_pDlgPrefEffects;
    delete m_pDlgPrefInterface;
    delete m_pDlgPrefKey;
    delete m_pDlgPrefLibrary;
    delete m_pDlgPrefMixer;
    delete m_pDlgPrefModplug;
    delete m_pDlgPrefRecord;
    delete m_pDlgPrefReplayGain;
    delete m_pDlgPrefSound;
    delete m_pDlgPrefVinyl;
    delete m_pDlgPrefWaveform;
}

void DlgPreferences::setupUi() {
    resize(750, 500);
    setWindowTitle(tr("Preferences"));

    m_pMainLayout = new QVBoxLayout(this);

    QHBoxLayout* pHLayout = new QHBoxLayout();
    m_pPageList = new QListWidget();
    m_pPageList->setMaximumWidth(200);
    m_pPageList->setSpacing(1);

    m_pPageContainer = new QStackedWidget();

    pHLayout->addWidget(m_pPageList);
    pHLayout->addWidget(m_pPageContainer);
    pHLayout->setStretchFactor(m_pPageList, 0);
    pHLayout->setStretchFactor(m_pPageContainer, 1);

    m_pMainLayout->addLayout(pHLayout);

    // Button box
    m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
            QDialogButtonBox::Apply);
    m_pMainLayout->addWidget(m_pButtonBox);

    // Connect button signals
    connect(m_pButtonBox->button(QDialogButtonBox::Ok),
            &QPushButton::clicked,
            this,
            &DlgPreferences::accept);
    connect(m_pButtonBox->button(QDialogButtonBox::Cancel),
            &QPushButton::clicked,
            this,
            &DlgPreferences::reject);
    connect(m_pButtonBox->button(QDialogButtonBox::Apply),
            &QPushButton::clicked,
            this,
            &DlgPreferences::apply);
}

void DlgPreferences::updatePageList() {
    m_pPageList->clear();
    m_pPageList->addItem(tr("Interface"));
    m_pPageList->addItem(tr("Sound Hardware"));
    m_pPageList->addItem(tr("Library"));
    m_pPageList->addItem(tr("Decks"));
    m_pPageList->addItem(tr("Mixer"));
    m_pPageList->addItem(tr("Effects"));
    m_pPageList->addItem(tr("Key Detection"));
    m_pPageList->addItem(tr("Auto DJ"));
    m_pPageList->addItem(tr("Recording"));
    m_pPageList->addItem(tr("Replay Gain"));
    m_pPageList->addItem(tr("Vinyl Control"));
    m_pPageList->addItem(tr("Waveform"));
    m_pPageList->addItem(tr("Broadcast"));
    m_pPageList->addItem(tr("Colors"));
    m_pPageList->addItem(tr("Accessibility"));
    m_pPageList->addItem(tr("Advanced"));
}

void DlgPreferences::load() {
    // Create the preference dialogs
    m_pDlgPrefAccessibility = new DlgPrefAccessibility(this, m_pConfig, m_pTtsSink);
    m_pDlgPrefAccessibilityEnhanced = new DlgPrefAccessibilityEnhanced(this, m_pConfig, m_pTtsSink);
    m_pDlgPrefAutodj = new DlgPrefAutodj(this, m_pConfig);
    m_pDlgPrefBeats = new DlgPrefBeats(this, m_pConfig);
    m_pDlgPrefBroadcast = new DlgPrefBroadcast(this, m_pConfig);
    m_pDlgPrefColors = new DlgPrefColors(this, m_pConfig);
    m_pDlgPrefDeck = new DlgPrefDeck(this, m_pConfig);
    m_pDlgPrefEffects = new DlgPrefEffects(this, m_pConfig);
    m_pDlgPrefInterface = new DlgPrefInterface(this, m_pConfig, m_pScreensaverManager);
    m_pDlgPrefKey = new DlgPrefKey(this, m_pConfig);
    m_pDlgPrefLibrary = new DlgPrefLibrary(this, m_pConfig);
    m_pDlgPrefMixer = new DlgPrefMixer(this, m_pConfig);
    m_pDlgPrefModplug = new DlgPrefModplug(this, m_pConfig);
    m_pDlgPrefRecord = new DlgPrefRecord(this, m_pConfig);
    m_pDlgPrefReplayGain = new DlgPrefReplayGain(this, m_pConfig);
    m_pDlgPrefSound = new DlgPrefSound(this, m_pConfig, m_pTtsSink);
    m_pDlgPrefVinyl = new DlgPrefVinyl(this, m_pConfig);
    m_pDlgPrefWaveform = new DlgPrefWaveform(this, m_pConfig);

    // Add all pages to the container
    m_pPageContainer->addWidget(m_pDlgPrefInterface);
    m_pPageContainer->addWidget(m_pDlgPrefSound);
    m_pPageContainer->addWidget(m_pDlgPrefLibrary);
    m_pPageContainer->addWidget(m_pDlgPrefDeck);
    m_pPageContainer->addWidget(m_pDlgPrefMixer);
    m_pPageContainer->addWidget(m_pDlgPrefEffects);
    m_pPageContainer->addWidget(m_pDlgPrefKey);
    m_pPageContainer->addWidget(m_pDlgPrefAutodj);
    m_pPageContainer->addWidget(m_pDlgPrefRecord);
    m_pPageContainer->addWidget(m_pDlgPrefReplayGain);
    m_pPageContainer->addWidget(m_pDlgPrefVinyl);
    m_pPageContainer->addWidget(m_pDlgPrefWaveform);
    m_pPageContainer->addWidget(m_pDlgPrefBroadcast);
    m_pPageContainer->addWidget(m_pDlgPrefColors);
    m_pPageContainer->addWidget(m_pDlgPrefAccessibilityEnhanced); // Use enhanced version
    m_pPageContainer->addWidget(m_pDlgPrefAccessibility);         // Fallback for basic
    m_pPageContainer->addWidget(
            m_pDlgPrefAutodj); // Advanced would go here but we're keeping it

    // Load data into preference dialogs
    m_pDlgPrefAccessibility->slotUpdate();
    m_pDlgPrefAccessibilityEnhanced->slotUpdate();
    m_pDlgPrefAutodj->slotUpdate();
    m_pDlgPrefBeats->slotUpdate();
    m_pDlgPrefBroadcast->slotUpdate();
    m_pDlgPrefColors->slotUpdate();
    m_pDlgPrefDeck->slotUpdate();
    m_pDlgPrefEffects->slotUpdate();
    m_pDlgPrefInterface->slotUpdate();
    m_pDlgPrefKey->slotUpdate();
    m_pDlgPrefLibrary->slotUpdate();
    m_pDlgPrefMixer->slotUpdate();
    m_pDlgPrefModplug->slotUpdate();
    m_pDlgPrefRecord->slotUpdate();
    m_pDlgPrefReplayGain->slotUpdate();
    m_pDlgPrefSound->slotUpdate();
    m_pDlgPrefVinyl->slotUpdate();
    m_pDlgPrefWaveform->slotUpdate();

    // Select the first page
    m_pPageList->setCurrentRow(0);
    m_pPageContainer->setCurrentIndex(0);
}

void DlgPreferences::connectSignals() {
    connect(m_pPageList, &QListWidget::currentRowChanged, this, &DlgPreferences::slotPageChanged);

    // Connect all dialogs to notify of changes
    connect(m_pDlgPrefAccessibility,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
    connect(m_pDlgPrefAccessibilityEnhanced,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
    connect(m_pDlgPrefAutodj,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
    connect(m_pDlgPrefBeats,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
    connect(m_pDlgPrefBroadcast,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
    connect(m_pDlgPrefColors,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
    connect(m_pDlgPrefDeck,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
    connect(m_pDlgPrefEffects,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
    connect(m_pDlgPrefInterface,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
    connect(m_pDlgPrefKey,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
    connect(m_pDlgPrefLibrary,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
    connect(m_pDlgPrefMixer,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
    connect(m_pDlgPrefModplug,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
    connect(m_pDlgPrefRecord,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
    connect(m_pDlgPrefReplayGain,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
    connect(m_pDlgPrefSound,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
    connect(m_pDlgPrefVinyl,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
    connect(m_pDlgPrefWaveform,
            &DlgPreferencePage::changed,
            this,
            &DlgPreferences::slotPreferenceChanged);
}

void DlgPreferences::slotPageChanged(int row) {
    if (row >= 0 && row < m_pPageContainer->count()) {
        m_pPageContainer->setCurrentIndex(row);
    }
}

void DlgPreferences::slotPreferenceChanged() {
    // This can be used for enabling/disabling Apply button or other UI updates
}

void DlgPreferences::accept() {
    apply();
    QDialog::accept();
}

void DlgPreferences::reject() {
    // Load settings back to original values if needed
    load();
    QDialog::reject();
}

void DlgPreferences::apply() {
    m_pDlgPrefAccessibility->slotApply();
    m_pDlgPrefAccessibilityEnhanced->slotApply();
    m_pDlgPrefAutodj->slotApply();
    m_pDlgPrefBeats->slotApply();
    m_pDlgPrefBroadcast->slotApply();
    m_pDlgPrefColors->slotApply();
    m_pDlgPrefDeck->slotApply();
    m_pDlgPrefEffects->slotApply();
    m_pDlgPrefInterface->slotApply();
    m_pDlgPrefKey->slotApply();
    m_pDlgPrefLibrary->slotApply();
    m_pDlgPrefMixer->slotApply();
    m_pDlgPrefModplug->slotApply();
    m_pDlgPrefRecord->slotApply();
    m_pDlgPrefReplayGain->slotApply();
    m_pDlgPrefSound->slotApply();
    m_pDlgPrefVinyl->slotApply();
    m_pDlgPrefWaveform->slotApply();

    // Notify about preference changes
    emit preferenceChanged();
}

void DlgPreferences::showSoundHardwarePage(mixxx::preferences::SoundHardwareTab tab) {
    m_pPageList->setCurrentRow(1); // Sound Hardware page index
    m_pPageContainer->setCurrentIndex(1);
    if (m_pDlgPrefSound) {
        m_pDlgPrefSound->showTab(tab);
    }
}

void DlgPreferences::showAccessibilityPage() {
    m_pPageList->setCurrentRow(14); // Accessibility page index
    m_pPageContainer->setCurrentIndex(14);
}

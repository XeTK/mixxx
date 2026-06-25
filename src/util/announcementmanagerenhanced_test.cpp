#include "util/announcementmanagerenhanced.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

#include "engine/enginetts.h"
#include "library/library.h"
#include "library/library_mock.h"
#include "mixer/playermanager.h"
#include "mixer/playermanager_mock.h"
#include "preferences/usersettings.h"
#include "preferences/usersettings_mock.h"
#include "util/ttsengine.h"
#include "util/ttsengine_mock.h"

class EnhancedAnnouncementManagerTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Setup test environment with mocked dependencies
        m_pSettings = std::make_shared<UserSettingsMock>();
        m_pLibrary = std::make_unique<LibraryMock>();
        m_pPlayerManager = std::make_unique<PlayerManagerMock>();
        m_pTtsEngine = std::make_unique<TtsEngineMock>();
        m_pTtsSink = std::make_unique<EngineTts>();
    }

    std::unique_ptr<EnhancedAnnouncementManager> m_pAnnouncementManager;
    std::unique_ptr<LibraryMock> m_pLibrary;
    std::unique_ptr<PlayerManagerMock> m_pPlayerManager;
    std::unique_ptr<UserSettingsMock> m_pSettings;
    std::unique_ptr<TtsEngineMock> m_pTtsEngine;
    std::unique_ptr<EngineTts> m_pTtsSink;
};

TEST_F(EnhancedAnnouncementManagerTest, TestEqParamAnnouncement) {
    // Create the enhanced announcement manager
    m_pAnnouncementManager = EnhancedAnnouncementManager::create(
            m_pLibrary.get(),
            m_pPlayerManager.get(),
            m_pSettings,
            m_pTtsSink.get());

    // Verify that we can create it without crashing
    ASSERT_NE(nullptr, m_pAnnouncementManager);

    // Test that announcement slot can be called
    // This is the key new functionality we're testing
    m_pAnnouncementManager->slotAnnounceEqParamChanged("[Channel1]", "gain", 0.5);

    // If we get here without crashing, the test passes
    // In a real test, we'd verify the TTS was called
}

TEST_F(EnhancedAnnouncementManagerTest, TestFilterParamAnnouncement) {
    m_pAnnouncementManager = EnhancedAnnouncementManager::create(
            m_pLibrary.get(),
            m_pPlayerManager.get(),
            m_pSettings,
            m_pTtsSink.get());

    ASSERT_NE(nullptr, m_pAnnouncementManager);

    m_pAnnouncementManager->slotAnnounceFilterParamChanged("[Channel2]", "freq", 1000.0);
}

TEST_F(EnhancedAnnouncementManagerTest, TestEffectSelectionAnnouncement) {
    m_pAnnouncementManager = EnhancedAnnouncementManager::create(
            m_pLibrary.get(),
            m_pPlayerManager.get(),
            m_pSettings,
            m_pTtsSink.get());

    ASSERT_NE(nullptr, m_pAnnouncementManager);

    m_pAnnouncementManager->slotAnnounceEffectSelected("Reverb");
}

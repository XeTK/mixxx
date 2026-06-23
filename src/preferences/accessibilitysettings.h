#pragma once

#include "preferences/usersettings.h"

class AccessibilitySettings {
  public:
    explicit AccessibilitySettings(UserSettingsPointer pConfig)
            : m_pConfig(std::move(pConfig)) {
    }

    DEFINE_PREFERENCE_HELPERS(AnnounceTrackSelection,
            bool,
            "[Accessibility]",
            "AnnounceTrackSelection",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceTrackLoad,
            bool,
            "[Accessibility]",
            "AnnounceTrackLoad",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnouncePlay,
            bool,
            "[Accessibility]",
            "AnnouncePlay",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceStop,
            bool,
            "[Accessibility]",
            "AnnounceStop",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceEndOfTrack,
            bool,
            "[Accessibility]",
            "AnnounceEndOfTrack",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceLibraryFocus,
            bool,
            "[Accessibility]",
            "AnnounceLibraryFocus",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceStartup,
            bool,
            "[Accessibility]",
            "AnnounceStartup",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceSearch,
            bool,
            "[Accessibility]",
            "AnnounceSearch",
            true);

    // Empty string means "use system default voice".
    DEFINE_PREFERENCE_HELPERS(TtsVoice,
            QString,
            "[Accessibility]",
            "TtsVoice",
            QString());

    // Rate in the range [-10, 10]; 0 = normal speed.
    DEFINE_PREFERENCE_HELPERS(TtsRate, int, "[Accessibility]", "TtsRate", 0);

    // Which engine output bus speech is mixed into and ducks:
    // 0 = headphone/cue (DJ-only, default), 1 = main (audience hears it).
    // Matches EngineTts::Route.
    DEFINE_PREFERENCE_HELPERS(TtsRoute, int, "[Accessibility]", "TtsRoute", 0);

  private:
    UserSettingsPointer m_pConfig;
};

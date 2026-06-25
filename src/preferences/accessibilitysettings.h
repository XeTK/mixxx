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

    DEFINE_PREFERENCE_HELPERS(AnnounceCue,
            bool,
            "[Accessibility]",
            "AnnounceCue",
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

    DEFINE_PREFERENCE_HELPERS(AnnounceEq,
            bool,
            "[Accessibility]",
            "AnnounceEq",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceFilter,
            bool,
            "[Accessibility]",
            "AnnounceFilter",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceTrim,
            bool,
            "[Accessibility]",
            "AnnounceTrim",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceMaster,
            bool,
            "[Accessibility]",
            "AnnounceMaster",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceMix,
            bool,
            "[Accessibility]",
            "AnnounceMix",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceEffect,
            bool,
            "[Accessibility]",
            "AnnounceEffect",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceSync,
            bool,
            "[Accessibility]",
            "AnnounceSync",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceTempo,
            bool,
            "[Accessibility]",
            "AnnounceTempo",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceCrossFader,
            bool,
            "[Accessibility]",
            "AnnounceCrossFader",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceFaderChange,
            bool,
            "[Accessibility]",
            "AnnounceFaderChange",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnouncePreventJogging,
            bool,
            "[Accessibility]",
            "AnnouncePreventJogging",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceTouchSurface,
            bool,
            "[Accessibility]",
            "AnnounceTouchSurface",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceTtsToggle,
            bool,
            "[Accessibility]",
            "AnnounceTtsToggle",
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

    // Deck naming convention: "Deck1" for "Deck 1" or "DeckA" for "Deck A"
    DEFINE_PREFERENCE_HELPERS(DeckNamingConvention,
            QString,
            "[Accessibility]",
            "DeckNamingConvention",
            "Deck1");

    // Enable/disable TTS by default
    DEFINE_PREFERENCE_HELPERS(EnableTtsByDefault,
            bool,
            "[Accessibility]",
            "EnableTtsByDefault",
            true);

  private:
    UserSettingsPointer m_pConfig;
};

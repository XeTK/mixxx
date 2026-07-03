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

    DEFINE_PREFERENCE_HELPERS(AnnounceLoop,
            bool,
            "[Accessibility]",
            "AnnounceLoop",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceHotcue,
            bool,
            "[Accessibility]",
            "AnnounceHotcue",
            true);

    DEFINE_PREFERENCE_HELPERS(AnnounceRecording,
            bool,
            "[Accessibility]",
            "AnnounceRecording",
            true);

    // Spoken readout of volume faders, EQ knobs, and the crossfader while
    // they move. Off by default: during a live mix these move constantly.
    DEFINE_PREFERENCE_HELPERS(AnnounceMixer,
            bool,
            "[Accessibility]",
            "AnnounceMixer",
            false);

    // Speak continuous controls while they move (throttled) instead of only
    // once they come to rest.
    DEFINE_PREFERENCE_HELPERS(AnnounceWhileMoving,
            bool,
            "[Accessibility]",
            "AnnounceWhileMoving",
            false);

    // Speak deck names as numbers ("Deck 1") instead of letters ("Deck A").
    DEFINE_PREFERENCE_HELPERS(DeckNamesAsNumbers,
            bool,
            "[Accessibility]",
            "DeckNamesAsNumbers",
            false);

    // Shorter phrasing: single-fact hotkeys speak just the value, and mixer
    // readouts drop filler words like "Deck" and "E Q".
    DEFINE_PREFERENCE_HELPERS(ConciseAnnouncements,
            bool,
            "[Accessibility]",
            "ConciseAnnouncements",
            false);

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

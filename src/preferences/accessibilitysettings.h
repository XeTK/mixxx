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

    // Spoken feedback for the track-list sort column/order when it changes
    // (via the sort_column_toggle keyboard binding or a column-header click).
    DEFINE_PREFERENCE_HELPERS(AnnounceSort,
            bool,
            "[Accessibility]",
            "AnnounceSort",
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

    // Spoken feedback for the effects section: unit routing toggles,
    // per-effect enables, which effect is loaded, and the deck filter
    // (QuickEffect) preset.
    DEFINE_PREFERENCE_HELPERS(AnnounceEffects,
            bool,
            "[Accessibility]",
            "AnnounceEffects",
            true);

    // Spoken readout of volume faders, EQ knobs, and the crossfader while
    // they move. On by default for the accessibility build: a blind DJ needs
    // to hear these controls. Readouts are debounced (spoken at rest) and
    // "announce while moving" is a separate opt-in, so this is not chatty
    // mid-mix.
    DEFINE_PREFERENCE_HELPERS(AnnounceMixer,
            bool,
            "[Accessibility]",
            "AnnounceMixer",
            true);

    // Per-event feedback for the earcon-capable transport events:
    // 0 = speech, 1 = sounds, 2 = sounds and speech. Only consulted when the
    // matching announcement is enabled above; other announcements are always
    // speech.
    DEFINE_PREFERENCE_HELPERS(FeedbackModePlay,
            int,
            "[Accessibility]",
            "FeedbackModePlay",
            2);
    DEFINE_PREFERENCE_HELPERS(FeedbackModeStop,
            int,
            "[Accessibility]",
            "FeedbackModeStop",
            2);
    DEFINE_PREFERENCE_HELPERS(FeedbackModeEndOfTrack,
            int,
            "[Accessibility]",
            "FeedbackModeEndOfTrack",
            2);
    DEFINE_PREFERENCE_HELPERS(FeedbackModeCue,
            int,
            "[Accessibility]",
            "FeedbackModeCue",
            2);
    DEFINE_PREFERENCE_HELPERS(FeedbackModeRestart,
            int,
            "[Accessibility]",
            "FeedbackModeRestart",
            2);
    DEFINE_PREFERENCE_HELPERS(FeedbackModeLoop,
            int,
            "[Accessibility]",
            "FeedbackModeLoop",
            2);
    DEFINE_PREFERENCE_HELPERS(FeedbackModeClipping,
            int,
            "[Accessibility]",
            "FeedbackModeClipping",
            2);

    // Warn when the main output clips. On by default: this is a safety/audio
    // quality signal, not a stylistic preference.
    DEFINE_PREFERENCE_HELPERS(AnnounceClipping,
            bool,
            "[Accessibility]",
            "AnnounceClipping",
            true);

    // Spoken confirmation when tracks are added to or removed from
    // playlists and crates.
    DEFINE_PREFERENCE_HELPERS(AnnouncePlaylist,
            bool,
            "[Accessibility]",
            "AnnouncePlaylist",
            true);

    // Smart cue lives in DlgPrefDeck / [Controls] (see dlgprefdeck.h) — it
    // is a general deck-loading behavior, not accessibility-specific.

    // How fader/knob positions are spoken: 0 = fractions ("three quarters"),
    // 1 = percentages ("75 percent").
    DEFINE_PREFERENCE_HELPERS(MixerReadoutStyle,
            int,
            "[Accessibility]",
            "MixerReadoutStyle",
            0);

    // How finely fraction readouts resolve: 0 = quarters, 1 = eighths,
    // 2 = sixteenths. Eighths by default — sixteenths proved too fine to be
    // useful by ear in tester feedback. Percentages are always exact.
    DEFINE_PREFERENCE_HELPERS(MixerFractionDetail,
            int,
            "[Accessibility]",
            "MixerFractionDetail",
            1);

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

    // macOS only: filters the voice picker by AVSpeechSynthesisVoice quality
    // tier. 0 = show all, 1 = Default, 2 = Enhanced, 3 = Premium (matches
    // TtsEngine::VoiceQuality + 1). Purely a browsing aid for the dropdown;
    // does not affect which voice is actually selected.
    DEFINE_PREFERENCE_HELPERS(TtsVoiceQualityFilter,
            int,
            "[Accessibility]",
            "TtsVoiceQualityFilter",
            0);

  private:
    UserSettingsPointer m_pConfig;
};

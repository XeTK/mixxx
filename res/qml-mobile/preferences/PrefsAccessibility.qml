import "../" as Skin
import QtQuick 2.12
import QtQuick.Controls 2.12
import "../Theme"

// Mirrors preferences/accessibilitysettings.h (the same ConfigObject keys
// as the desktop DlgPrefAccessibility page), minus the two entries that
// don't make sense here: TtsVoiceQualityFilter (macOS-only voice-picker
// filter) and TtsVoice (would need a platform voice-enumeration bridge,
// same class of problem as Sound Hardware's device list - out of scope for
// the plain-ConfigObject pass this page is part of; system default voice
// is used instead). OrientationPlayed is a one-shot internal flag, not a
// user preference, same as on desktop.
Item {
    id: root

    // 0 = speech, 1 = sounds, 2 = sounds and speech - see the
    // FeedbackMode* comment in accessibilitysettings.h.
    readonly property var feedbackOptions: [
        {
            text: "Speech",
            value: 0
        },
        {
            text: "Sounds",
            value: 1
        },
        {
            text: "Sounds + speech",
            value: 2
        }
    ]

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        Column {
            width: parent.width
            spacing: 10

            Skin.SectionText {
                width: parent.width
                text: "Text-to-Speech"
            }

            PrefsDropdownRow {
                width: parent.width
                label: "Route"
                group: "[Accessibility]"
                key: "TtsRoute"
                defaultValue: 0
                options: [
                    {
                        text: "Headphones / cue",
                        value: 0
                    },
                    {
                        text: "Main output",
                        value: 1
                    }
                ]
            }

            PrefsSliderRow {
                width: parent.width
                label: "Rate"
                group: "[Accessibility]"
                key: "TtsRate"
                defaultValue: 0
                from: -10
                to: 10
                stepSize: 1
                valueFormatter: (v) => v.toFixed(0)
            }

            PrefsSliderRow {
                width: parent.width
                label: "Ducking strength"
                useControl: true
                group: "[Tts]"
                key: "duckStrength"
                from: 0
                to: 1
                stepSize: 0.01
                valueFormatter: (v) => Math.round(v * 100) + "%"
            }

            PrefsSliderRow {
                width: parent.width
                label: "Beat click volume"
                useControl: true
                group: "[BeatClick]"
                key: "volume"
                from: 0
                to: 1
                stepSize: 0.01
                valueFormatter: (v) => Math.round(v * 100) + "%"
            }

            Skin.SectionText {
                width: parent.width
                text: "Mixer Readout"
            }

            PrefsDropdownRow {
                width: parent.width
                label: "Style"
                group: "[Accessibility]"
                key: "MixerReadoutStyle"
                defaultValue: 0
                options: [
                    {
                        text: "Fractions",
                        value: 0
                    },
                    {
                        text: "Percent",
                        value: 1
                    }
                ]
            }

            PrefsDropdownRow {
                width: parent.width
                label: "Fraction detail"
                group: "[Accessibility]"
                key: "MixerFractionDetail"
                defaultValue: 1
                options: [
                    {
                        text: "Quarters",
                        value: 0
                    },
                    {
                        text: "Eighths",
                        value: 1
                    },
                    {
                        text: "Sixteenths",
                        value: 2
                    }
                ]
            }

            PrefsToggleRow {
                width: parent.width
                label: "Announce mixer while moving"
                group: "[Accessibility]"
                key: "AnnounceWhileMoving"
                defaultValue: false
            }

            Skin.SectionText {
                width: parent.width
                text: "Feedback Mode"
            }

            PrefsDropdownRow {
                width: parent.width
                label: "Play/Pause"
                group: "[Accessibility]"
                key: "FeedbackModePlay"
                defaultValue: 2
                options: root.feedbackOptions
            }

            PrefsDropdownRow {
                width: parent.width
                label: "Stop"
                group: "[Accessibility]"
                key: "FeedbackModeStop"
                defaultValue: 2
                options: root.feedbackOptions
            }

            PrefsDropdownRow {
                width: parent.width
                label: "End of track"
                group: "[Accessibility]"
                key: "FeedbackModeEndOfTrack"
                defaultValue: 2
                options: root.feedbackOptions
            }

            PrefsDropdownRow {
                width: parent.width
                label: "Cue"
                group: "[Accessibility]"
                key: "FeedbackModeCue"
                defaultValue: 2
                options: root.feedbackOptions
            }

            PrefsDropdownRow {
                width: parent.width
                label: "Restart"
                group: "[Accessibility]"
                key: "FeedbackModeRestart"
                defaultValue: 2
                options: root.feedbackOptions
            }

            PrefsDropdownRow {
                width: parent.width
                label: "Loop"
                group: "[Accessibility]"
                key: "FeedbackModeLoop"
                defaultValue: 2
                options: root.feedbackOptions
            }

            PrefsDropdownRow {
                width: parent.width
                label: "Clipping"
                group: "[Accessibility]"
                key: "FeedbackModeClipping"
                defaultValue: 2
                options: root.feedbackOptions
            }

            Skin.SectionText {
                width: parent.width
                text: "Announcements"
            }

            PrefsToggleRow {
                width: parent.width
                label: "Startup"
                group: "[Accessibility]"
                key: "AnnounceStartup"
                defaultValue: true
            }

            PrefsToggleRow {
                width: parent.width
                label: "Track selection"
                group: "[Accessibility]"
                key: "AnnounceTrackSelection"
                defaultValue: true
            }

            PrefsToggleRow {
                width: parent.width
                label: "Track load"
                group: "[Accessibility]"
                key: "AnnounceTrackLoad"
                defaultValue: true
            }

            PrefsToggleRow {
                width: parent.width
                label: "Play"
                group: "[Accessibility]"
                key: "AnnouncePlay"
                defaultValue: true
            }

            PrefsToggleRow {
                width: parent.width
                label: "Cue"
                group: "[Accessibility]"
                key: "AnnounceCue"
                defaultValue: true
            }

            PrefsToggleRow {
                width: parent.width
                label: "Stop"
                group: "[Accessibility]"
                key: "AnnounceStop"
                defaultValue: true
            }

            PrefsToggleRow {
                width: parent.width
                label: "End of track"
                group: "[Accessibility]"
                key: "AnnounceEndOfTrack"
                defaultValue: true
            }

            PrefsToggleRow {
                width: parent.width
                label: "Library focus"
                group: "[Accessibility]"
                key: "AnnounceLibraryFocus"
                defaultValue: true
            }

            PrefsToggleRow {
                width: parent.width
                label: "Search"
                group: "[Accessibility]"
                key: "AnnounceSearch"
                defaultValue: true
            }

            PrefsToggleRow {
                width: parent.width
                label: "Sort"
                group: "[Accessibility]"
                key: "AnnounceSort"
                defaultValue: true
            }

            PrefsToggleRow {
                width: parent.width
                label: "Sync"
                group: "[Accessibility]"
                key: "AnnounceSync"
                defaultValue: true
            }

            PrefsToggleRow {
                width: parent.width
                label: "Tempo"
                group: "[Accessibility]"
                key: "AnnounceTempo"
                defaultValue: true
            }

            PrefsToggleRow {
                width: parent.width
                label: "Loop"
                group: "[Accessibility]"
                key: "AnnounceLoop"
                defaultValue: true
            }

            PrefsToggleRow {
                width: parent.width
                label: "Hotcue"
                group: "[Accessibility]"
                key: "AnnounceHotcue"
                defaultValue: true
            }

            PrefsToggleRow {
                width: parent.width
                label: "Recording"
                group: "[Accessibility]"
                key: "AnnounceRecording"
                defaultValue: true
            }

            PrefsToggleRow {
                width: parent.width
                label: "Effects"
                group: "[Accessibility]"
                key: "AnnounceEffects"
                defaultValue: true
            }

            PrefsToggleRow {
                width: parent.width
                label: "Clipping"
                group: "[Accessibility]"
                key: "AnnounceClipping"
                defaultValue: true
            }

            PrefsToggleRow {
                width: parent.width
                label: "Playlist/crate changes"
                group: "[Accessibility]"
                key: "AnnouncePlaylist"
                defaultValue: true
            }

            Skin.SectionText {
                width: parent.width
                text: "General"
            }

            PrefsToggleRow {
                width: parent.width
                label: "Omit deck name in headphone cue announcements"
                group: "[Accessibility]"
                key: "SplitCueOmitDeckName"
                defaultValue: false
            }

            PrefsToggleRow {
                width: parent.width
                label: "Speak deck names as numbers"
                group: "[Accessibility]"
                key: "DeckNamesAsNumbers"
                defaultValue: false
            }

            PrefsToggleRow {
                width: parent.width
                label: "Concise announcements"
                group: "[Accessibility]"
                key: "ConciseAnnouncements"
                defaultValue: false
            }

            PrefsToggleRow {
                width: parent.width
                label: "Controller navigation without window focus"
                group: "[Accessibility]"
                key: "ControllerNavigationWithoutFocus"
                defaultValue: false
            }
        }
    }
}

# Mixxx Accessibility Guide for Visually Impaired Users

## Introduction

This guide provides comprehensive instructions for using Mixxx with accessibility features designed for visually impaired users. Mixxx now includes enhanced audio announcements, keyboard navigation improvements, and configurable deck naming to improve accessibility.

## Key Accessibility Features

### 1. Enhanced Audio Announcements

- **Deck-specific cues**: Hear "Deck 1 cue on" or "Deck 2 cue off" instead of generic announcements
- **Track loading**: "Deck 1 loaded: Track Name by Artist"
- **Playback control**: "Deck 1 playing" and "Deck 1 stopped"
- **End of track**: "Deck 1 end of track"

### 2. Configurable Deck Naming

You can choose between:

- "Deck 1" format (e.g., "Deck 1 cue on")
- "Deck A" format (e.g., "Deck A cue on")

### 3. Default TTS Enablement

Text-to-speech is now enabled by default for immediate accessibility.

## How to Access Accessibility Settings

1. Open Mixxx
2. Go to **Preferences** → **Accessibility**
3. Configure these options:

### Deck Naming Convention

- Select **"Deck 1"** for "Deck 1 cue on" format
- Select **"Deck A"** for "Deck A cue on" format

### TTS Settings

- Ensure **"Announce Cue Button"** is checked
- Ensure **"Announce Track Load"** is checked
- Configure TTS voice and rate as needed

## Keyboard Navigation Guide

Mixxx supports full keyboard navigation for visually impaired users:

### Basic Navigation

- **Tab** - Move between UI elements
- **Enter/Space** - Activate buttons and checkboxes
- **Arrow keys** - Navigate within lists and controls

### Advanced Navigation

- **F6** - Toggle accessibility features (if enabled)
- **Alt+Shift+A** - Toggle TTS on/off quickly (if configured)

### Navigation Tips

1. **Focus indicators**: Use Tab to navigate between focusable elements
2. **Announcements**: The application will announce when you move between controls
3. **Context awareness**: Focus on library items, decks, or controls will provide contextual information

## Using Enhanced Features

### 1. Track Loading

- Load tracks to decks and hear announcements including deck numbers
- Example: "Deck 1 loaded: Track Name by Artist"

### 2. Cue Mode

- Activate cue mode on decks to hear:
  - "Deck 1 cue on"
  - "Deck 1 cue off"

### 3. Playback Control

- When you play/pause:
  - "Deck 1 playing"
  - "Deck 1 stopped"
- At end of track:
  - "Deck 1 end of track"

## Troubleshooting

### TTS Not Working

1. Check that TTS is enabled in **Preferences** → **Accessibility**
2. Verify the TTS voice settings are properly configured
3. Make sure you have a TTS engine installed on your system

### Deck Names Not Working

1. Ensure **Deck Naming Convention** is set in **Preferences** → **Accessibility**
2. Restart Mixxx after making changes to settings

### Keyboard Navigation Not Working

1. Verify keyboard navigation is enabled in system accessibility settings
2. Try using Tab navigation through the interface elements
3. Check that Focus indicators are visible (they should be if keyboard navigation is working)

## Recommended Keyboard Shortcuts for Accessibility

| Function         | Keyboard Shortcut |
|------------------|-------------------|
| Toggle TTS       | Alt+Shift+A       |
| Play/Pause       | Space             |
| Cue              | C                 |
| Load Track       | L                 |
| Next Track       | N                 |
| Previous Track   | P                 |
| Volume Up        | Up Arrow          |
| Volume Down      | Down Arrow        |

## Accessibility Settings Reference

All accessibility settings can be found in:
**Preferences** → **Accessibility**

- **Announce Track Selection**: Announce when tracks are selected in library
- **Announce Track Load**: Announce when tracks are loaded to decks
- **Announce Play**: Announce when decks start playing
- **Announce Cue**: Announce when cue mode is activated/deactivated
- **Announce Stop**: Announce when decks are stopped
- **Announce End of Track**: Announce when tracks finish playing
- **Announce Library Focus**: Announce when you focus on library elements
- **Announce Startup**: Announce when Mixxx starts up
- **Announce Search**: Announce when search results are found
- **TtsVoice**: Select specific TTS voice (leave blank for system default)
- **TtsRate**: Adjust speech speed (-10 to 10, 0 = normal)
- **TtsRoute**: Select output device (0 = headphones, 1 = main output)
- **DeckNamingConvention**: Choose between "Deck1" or "DeckA" formats
- **EnableTtsByDefault**: Enable TTS by default

## Tips for Visually Impaired Users

1. **Use keyboard navigation** - Start with Tab to navigate between UI elements
2. **Listen to announcements** - The application will announce important actions and state changes
3. **Configure custom voices** - Select a clear TTS voice that's easy to understand
4. **Adjust speech rate** - Set the speech rate to a comfortable pace
5. **Use the library sidebar** - All library items are announced when selected
6. **Test deck announcements** - Make sure cue and playback announcements work as expected

## Feedback and Support

If you have issues with these accessibility features:

1. Report them through the Mixxx bug tracker
2. Include what you expect to hear vs what you're hearing
3. Mention your operating system and TTS setup

These enhancements make Mixxx more accessible for visually impaired users while maintaining all existing functionality.

## Version Information

This guide applies to Mixxx version 2.6 beta with accessibility improvements implemented.

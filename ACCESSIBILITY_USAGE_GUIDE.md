# Mixxx Accessibility Enhancements Usage Guide

This guide explains how to use the accessibility enhancements that were implemented since the Mixxx 2.6 fork.

## Key Improvements

### 1. Deck-Specific Audio Announcements

**Before:** Generic announcements like "Cue on" or "Cue off"
**After:** Specific deck announcements like "Deck 1 cue on" or "Deck 2 cue off"

### 2. Default TTS Enablement

**Before:** TTS toggle was unticked by default
**After:** TTS is now enabled by default for better accessibility

### 3. Configurable Deck Naming

Users can now choose between:

- "Deck 1" format (e.g., "Deck 1 cue on")
- "Deck A" format (e.g., "Deck A cue on")

## How to Use These Features

### Accessing Accessibility Settings

1. Open Mixxx
2. Go to **Preferences** → **Accessibility**
3. Configure the following options:

#### Deck Naming Convention

- Select **"Deck 1"** for "Deck 1 cue on" format
- Select **"Deck A"** for "Deck A cue on" format

#### TTS Settings

- Ensure **"Announce Cue Button"** is checked
- Ensure **"Announce Track Load"** is checked
- Configure TTS voice and rate as needed

### Using Enhanced Announcements

Once configured, you'll hear:

- **"Deck 1 cue on"** when activating cue mode on deck 1
- **"Deck 1 cue off"** when deactivating cue mode on deck 1
- **"Deck 1 loaded: Track Name by Artist"** when loading tracks
- **"Deck 1 playing"** when playback starts
- **"Deck 1 stopped"** when playback pauses

### Keyboard Navigation

Mixxx supports full keyboard navigation:

- **Tab** - Move between UI elements
- **Enter/Space** - Activate buttons and checkboxes
- **Arrow keys** - Navigate within lists and controls
- **F6** - Toggle accessibility features (if enabled)

### Testing Your Configuration

1. Load a track to a deck
2. Activate cue mode on that deck
3. You should hear "Deck 1 cue on" or your configured format
4. Play/pause the deck and listen for "Deck 1 playing" / "Deck 1 stopped"
5. Load a different track and verify the announcement includes the deck number

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

## Feedback

If you have any issues with these accessibility features, please:

1. Report them through the Mixxx bug tracker
2. Include what you expect to hear vs what you're hearing
3. Mention your operating system and TTS setup

These enhancements make Mixxx more accessible for visually impaired users while maintaining all existing functionality.

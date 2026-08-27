@manual @keyboard
Feature: The full workflow from the keyboard alone

  Unplug the controller before running this file. The point is to establish
  that someone with no DJ hardware at all — a laptop, headphones and a
  keyboard — can mix, EQ, apply effects, drive Auto DJ, sort the library and
  reach every application toggle, entirely by ear.

  Issue #77 added the mixer and effects bindings that made this possible at
  all. Issue #78 put the AccessMenu on the keyboard. Issue #81 did the same
  for library sorting. Issue #86 covered Auto DJ.

  A caution before starting: most of #77's chords live in the Ctrl+Alt
  namespace, which macOS VoiceOver claims for itself. If you are on macOS
  with VoiceOver running and none of the mixer chords work, that is not a
  binding bug — see features/macos_voiceover.feature, which investigates it
  properly. Run this file on Linux or Windows, or on macOS with VoiceOver
  off, to test the bindings themselves.

  Background:
    Given no DJ controller is connected
    And Mixxx is running with the en_US keyboard layout
    And speech is on
    And "Speak deck names as numbers" is enabled in Preferences, Accessibility
    And "Announce mixer controls" is enabled in Preferences, Accessibility
    And "Announce effects" is enabled in Preferences, Accessibility
    And tracks are loaded in deck 1 and deck 2

  # ---------------------------------------------------------------------
  # Mixer: volume and trim — issue #77
  #
  # The mixer readout convention: the control names itself on first
  # movement ("Deck 1 volume"), and the value follows once it settles
  # ("three quarters"). Adjusting the same control again speaks only the
  # new value. A control landing back on a value it already announced stays
  # silent entirely. None of that is new here, but it is what you will
  # hear, so it is worth knowing before deciding something is broken.
  # ---------------------------------------------------------------------

  @blocking
  Scenario: Deck volume can be set by ear
    Given deck 1 is playing at full volume
    When I press "Ctrl+Alt+B" four times
    Then I hear "Deck 1 volume" named on the first press
    And I hear a value announced once I stop pressing
    And deck 1 is audibly quieter
    When I press "Ctrl+Alt+V" four times
    Then I hear a value announced
    And deck 1 is back at roughly its original level

  Scenario: Deck 2's volume uses the Shift variant
    When I press "Ctrl+Alt+Shift+B"
    Then I hear "Deck 2 volume" named
    And deck 1's volume has not changed

  Scenario: Trim is separate from volume and says so
    When I press "Ctrl+Alt+T"
    Then I hear deck 1's trim named, not its volume
    And the announcement distinguishes trim from the channel fader

  Scenario: A control that lands back where it started stays quiet
    Given deck 1's volume has just been announced at a value
    When I press "Ctrl+Alt+B" once and then "Ctrl+Alt+V" once
    Then the final value announcement matches where I started
    And I am not told the same value twice in a row

  # ---------------------------------------------------------------------
  # EQ and filter — issue #77
  # ---------------------------------------------------------------------

  @blocking
  Scenario Outline: Every EQ band moves in both directions on deck 1
    When I press "<chord>"
    Then I hear deck 1's "<band>" named
    And the sound of deck 1 changes as described by "<effect>"

    Examples:
      | chord        | band    | effect                        |
      | Ctrl+Alt+E   | EQ low  | less bass                     |
      | Ctrl+Alt+F11 | EQ low  | more bass                     |
      | Ctrl+Alt+W   | EQ mid  | less midrange                 |
      | Ctrl+Alt+O   | EQ mid  | more midrange                 |
      | Ctrl+Alt+F   | EQ high | less treble                   |
      | Ctrl+Alt+F10 | EQ high | more treble                   |

  Scenario Outline: Every EQ band moves in both directions on deck 2
    When I press "<chord>"
    Then I hear deck 2's "<band>" named
    And deck 1's EQ is unchanged

    Examples:
      | chord              | band    |
      | Ctrl+Alt+Shift+E   | EQ low  |
      | Ctrl+Alt+Shift+F11 | EQ low  |
      | Ctrl+Alt+Shift+W   | EQ mid  |
      | Ctrl+Alt+Shift+O   | EQ mid  |
      | Ctrl+Alt+Shift+F   | EQ high |
      | Ctrl+Alt+Shift+F10 | EQ high |

  @blocking
  Scenario: A bass swap can be performed entirely by keyboard
    # The single most common blind-DJ mix move. If this works, the EQ
    # bindings are fit for purpose.
    Given both decks are playing and beatmatched
    When I press "Ctrl+Alt+E" repeatedly until deck 1's low EQ is fully down
    Then I hear the value reach its minimum
    And deck 1's bass is gone from the mix
    When I press "Ctrl+Alt+Shift+F11" until deck 2's low EQ is back at centre
    Then deck 2's bass carries the mix on its own

  Scenario: The filter sweeps in both directions
    When I press "Ctrl+Alt+N" several times
    Then I hear deck 1's filter named and its value announced
    And the sound is audibly filtered
    When I press "Ctrl+Alt+S" several times
    Then the filter returns towards centre
    And I hear the value announced as it goes

  # ---------------------------------------------------------------------
  # Effects — issue #77
  #
  # Both "enabled" controls here use PowerWindow button mode: a short press
  # latches the state, a long press is momentary and reverts on release.
  # If holding Ctrl+Alt+A appears to switch the unit off again as soon as
  # you let go, that is PowerWindow working, not a bug.
  # ---------------------------------------------------------------------

  @blocking
  Scenario: Effect unit 1 can be switched on and off
    When I press "Ctrl+Alt+A" briefly
    Then I hear effect unit 1 announced as on
    And I can hear the effect in the mix
    When I press "Ctrl+Alt+A" briefly again
    Then I hear effect unit 1 announced as off
    And the effect is gone from the mix

  Scenario: Holding the effect unit chord is momentary, not latching
    When I press and hold "Ctrl+Alt+A" for two seconds
    Then the effect is audible while I hold it
    When I release the keys
    Then the effect stops
    # Expected: PowerWindow behaviour. Record it, do not file it.

  Scenario: Chain presets cycle and are announced
    When I press "Ctrl+Alt+F9"
    Then I hear effect unit 1's new chain preset announced by name
    When I press "Ctrl+Alt+F9" three more times
    Then I hear a different preset name each time

  Scenario: Effect focus cycles through the slots and names each one
    When I press "Ctrl+Alt+L"
    Then I hear "Unit 1: " followed by an effect name and " focused"
    When I press "Ctrl+Alt+L" repeatedly
    Then I hear each slot in unit 1 announced in turn
    And an empty slot is announced as "Unit 1: effect " followed by its slot number and " focused"

  Scenario: An individual effect slot can be enabled and its effect changed
    When I press "Ctrl+Alt+I" briefly
    Then I hear effect slot 1 announced as on
    When I press "Ctrl+Alt+J"
    Then I hear the name of the next effect loaded into slot 1
    When I press "Ctrl+Alt+X"
    Then I hear the name of the previous effect, which is where I started

  Scenario: Effect unit 2 responds on the Shift variants
    When I press "Ctrl+Alt+Shift+A" briefly
    Then I hear effect unit 2 announced as on
    And effect unit 1 is unchanged

  Scenario: Effect announcements can be turned off
    Given "Announce effects" is disabled in Preferences, Accessibility
    When I press "Ctrl+Alt+L"
    Then I hear nothing
    And the focus has still moved, confirmed by re-enabling the setting and pressing it again

  # ---------------------------------------------------------------------
  # The AccessMenu on the keyboard — issue #78
  #
  # These six chords are registered as application-wide Qt shortcuts on the
  # main window, deliberately NOT through the kbd.cfg control path. That is
  # what lets them keep working after you disable keyboard shortcuts from
  # inside the menu itself — otherwise disabling shortcuts would lock a
  # keyboard-only DJ out of the only way to re-enable them.
  #
  # Consequence worth knowing: the key sequences are read from the keyboard
  # config once, at startup. Rebinding them in a .kbd.cfg needs a restart.
  # ---------------------------------------------------------------------

  @blocking
  Scenario: The menu opens, navigates and closes from the keyboard
    When I press "Alt+Shift+M"
    Then I hear "Main menu." followed by the current item
    When I press "Alt+Shift+Down"
    Then I hear the next item announced
    When I press "Alt+Shift+Up"
    Then I hear the previous item announced
    When I press "Alt+Shift+M"
    Then I hear "Menu closed"

  Scenario: I can walk the whole root menu and hear every item
    When I press "Alt+Shift+M"
    And I press "Alt+Shift+Down" repeatedly until I return to where I started
    Then I hear each of these items announced in order
      | Back                |
      | Preferences         |
      | Values              |
      | Recording           |
      | Broadcasting        |
      | Speech on/off       |
      | Fullscreen          |
      | Keyboard shortcuts  |
      | Reload skin         |
      | Rescan library      |
      | About               |
      | Quit                |

  @blocking
  Scenario Outline: Toggle items announce their current state
    # The state vocabulary is exactly "on" and "off" — never "enabled" or
    # "disabled". Wording drift here is a real finding.
    Given "<item>" is currently off
    When I press "Alt+Shift+M"
    And I navigate to "<item>"
    Then I hear "<item>, off"
    When I press "Alt+Shift+Return"
    And I navigate back to "<item>"
    Then I hear "<item>, on"

    Examples:
      | item          |
      | Recording     |
      | Speech on/off |
      | Fullscreen    |

  Scenario: Fullscreen state stays correct when changed from outside the menu
    # The menu has no control object for fullscreen; the main window pushes
    # the state to it. If that push is missed, the menu will lie.
    Given Mixxx is not fullscreen
    When I toggle fullscreen using the View menu rather than the AccessMenu
    And I press "Alt+Shift+M"
    And I navigate to "Fullscreen"
    Then I hear "Fullscreen, on"

  @blocking
  Scenario: Disabling keyboard shortcuts warns me first and leaves me a way back
    # The whole reason the AccessMenu chords bypass the kbd.cfg path.
    Given keyboard shortcuts are enabled
    When I press "Alt+Shift+M"
    And I navigate to "Keyboard shortcuts"
    Then I hear "Keyboard shortcuts, on"
    When I press "Alt+Shift+Return"
    Then I hear "Keyboard shortcuts now off. Use this menu or the mouse to re-enable them."
    And I hear the warning before anything else
    When I press "Alt+1"
    Then I hear nothing, because ordinary shortcuts are now off
    When I press "Alt+Shift+M"
    Then I hear "Main menu." followed by the current item
    When I navigate to "Keyboard shortcuts" and press "Alt+Shift+Return"
    Then I hear "Keyboard shortcuts, off" announced when I navigate back to it
    When I press "Alt+1"
    Then I hear deck 1's status again

  Scenario: Enabling keyboard shortcuts does not produce the warning
    Given keyboard shortcuts are disabled
    When I enable them from the AccessMenu
    Then I do not hear "Keyboard shortcuts now off. Use this menu or the mouse to re-enable them."

  @regression
  Scenario: The AccessMenu chords do not fire twice
    # Three of the six chord names — activate, back, confirm — match real
    # control objects as well as the Qt shortcuts, so with keyboard
    # shortcuts enabled there are two paths that could both fire. Qt is
    # expected to consume the press first. A doubled announcement means it
    # did not.
    Given keyboard shortcuts are enabled
    And the AccessMenu is open on a submenu item
    When I press "Alt+Shift+Return"
    Then I hear one announcement
    And I do not hear the same announcement twice
    When I press "Alt+Shift+Backspace"
    Then I hear one announcement
    When I press "Alt+Shift+Space"
    Then I hear one announcement

  Scenario: Value editing from the keyboard
    When I press "Alt+Shift+M"
    And I navigate to "Values" and press "Alt+Shift+Return"
    And I navigate to "Speech rate" and press "Alt+Shift+Return"
    Then I hear "Speech rate. Turn to change, confirm to set, back to cancel." followed by the current value
    When I press "Alt+Shift+Down"
    Then I hear the new value announced
    When I press "Alt+Shift+Space"
    Then I hear "Set." followed by "Speech rate, " and the new value
    And subsequent speech is at the new rate

  Scenario: Cancelling a value edit leaves the value alone
    Given I am editing "Ducking strength" in the AccessMenu
    And I have changed the value without confirming
    When I press "Alt+Shift+Backspace"
    Then I hear "Cancelled." followed by "Ducking strength, " and the original value
    And the ducking behaviour is unchanged

  Scenario: The Recording item reports ready as on
    # KNOWN QUIRK: the recording status control has three states, and both
    # "ready" and "recording" read as "on". Confirm rather than file.
    Given recording is armed but not yet writing
    When I navigate to "Recording" in the AccessMenu
    Then I hear "Recording, on"

  # ---------------------------------------------------------------------
  # Library sorting from the keyboard — issue #81
  # ---------------------------------------------------------------------

  @blocking
  Scenario: Cycling the sort column announces each one
    Given the library track table has focus and contains tracks
    When I press "Alt+Shift+S"
    Then I hear "Sorting by " followed by a column name and " ascending"
    When I press "Alt+Shift+S" three more times
    Then I hear a different column name each time
    And each is announced as ascending, because moving to a new column resets the order

  Scenario: Cycling backwards through the sort columns
    Given the library track table has focus
    When I press "Alt+Shift+S" twice and note the column
    And I press "Ctrl+Alt+Shift+S"
    Then I hear the previous column announced

  Scenario: Sorting the same column again flips the order
    Given the library is sorted by title ascending
    When I press "Alt+Shift+S" until it lands on title again
    Then I hear "Sorting by title descending"

  @blocking
  Scenario: Reversing the sort order without changing column
    # LIKELY BUG — read this before recording a result.
    # The sort_order control is a plain push button whose VALUE is the sort
    # direction, with nothing connected to interpret a press as a toggle.
    # Key-down sends 1 (descending) and key-up sends 0 (ascending), so the
    # documented behaviour ("reverse the current sort order") is probably
    # not what happens: expect the list to flip to descending while the key
    # is held and snap back to ascending on release, with TWO sort
    # announcements per press.
    Given the library is sorted by title ascending
    When I press and release "Alt+Shift+O"
    Then I hear how many sort announcements occurred and what each said
    And I record the sort order the list is left in
    # Pass = one announcement, "Sorting by title descending", and the list
    # stays descending. Anything else, log exactly what you heard.

  Scenario: The column visibility menu opens and can be worked by ear
    Given the library track table has focus
    When I press "Alt+Shift+V"
    Then my screen reader announces a menu titled "Show or hide columns."
    And I can arrow through the column names hearing each one and whether it is ticked
    When I press "Space" on one of them
    Then that column's tick state changes
    When I press "Escape"
    Then the menu closes
    # NOTE: Mixxx's own speech says nothing here — the menu items are read
    # by the platform screen reader. Without one running this scenario
    # cannot be completed, which is itself worth recording.

  Scenario: Sorting by a column with no spoken name stays silent
    # Some columns have no entry in the spoken vocabulary and are skipped
    # entirely rather than announced blankly. Expected behaviour.
    When I cycle the sort column onto a column with no spoken name
    Then I hear nothing for that step
    And pressing "Alt+Shift+S" again moves on and announces normally

  # ---------------------------------------------------------------------
  # Sort feedback also fires from outside the keyboard — issue #17
  #
  # slotAnnounceSort() is wired to [Library],sort_column and sort_order
  # changing value, not to the sort_column_toggle keyboard binding
  # specifically. Issue #17 (the announcement itself) predates issue
  # #59's keyboard chords by a separate PR and never required a keyboard
  # trigger to fire — every scenario above just happens to exercise it
  # via the keyboard because that is this file's whole premise. This one
  # is out of place in a "keyboard only" file on purpose: it exists to
  # confirm the OTHER, pre-existing, non-keyboard trigger — clicking a
  # column header, which #59's own scenarios never touched — also
  # announces. A sighted assistant can do the clicking; only the
  # listening needs to be done blind.
  # ---------------------------------------------------------------------

  Scenario: Clicking a column header also announces the new sort
    Given the library is sorted by artist ascending
    When a column header other than the current sort column is clicked
    Then I hear "Sorting by " followed by that column's name and " ascending"
    When the same column header is clicked again
    Then I hear "Sorting by " followed by the column's name and " descending"

  # ---------------------------------------------------------------------
  # Auto DJ from the keyboard — issue #86
  # ---------------------------------------------------------------------

  @blocking
  Scenario: Enabling and disabling Auto DJ is announced
    Given the Auto DJ queue contains several tracks
    When I press "Shift+F12"
    Then I hear "Auto DJ on. Next: " followed by the next track's artist and title
    When I press "Shift+F12"
    Then I hear "Auto DJ off"

  Scenario: Enabling Auto DJ with an empty queue
    Given the Auto DJ queue is empty
    When I press "Shift+F12"
    Then I hear "Auto DJ on"
    And I do not hear a track name

  Scenario Outline: Auto DJ transport actions are announced
    Given Auto DJ is on with tracks queued
    When I press "<chord>"
    Then I hear "<heard>"

    Examples:
      | chord     | heard      |
      | Shift+F11 | Fading now |
      | Shift+F10 | Skipped    |

  @blocking
  Scenario: The next-track readout tells me everything I need mid-set
    # Note the wording difference from the toggle announcements: the
    # on-demand readout says "Auto DJ is on", the toggle says "Auto DJ on".
    # That asymmetry is deliberate. Transcribe carefully.
    Given Auto DJ is on
    And the queue has at least one track
    And deck 1 is playing
    When I press "Alt+Shift+N"
    Then I hear "Auto DJ is on."
    And I hear "Next: " followed by the queued track's artist and title
    And I hear "About " followed by a time remaining and " on Deck 1."

  Scenario: The next-track readout with Auto DJ off and nothing queued
    Given Auto DJ is off
    And the queue is empty
    When I press "Alt+Shift+N"
    Then I hear "Auto DJ is off. Queue is empty."

  Scenario: The readout omits the time clause when nothing is playing
    Given Auto DJ is on with a track queued
    And neither deck is playing
    When I press "Alt+Shift+N"
    Then I hear "Auto DJ is on." and the next track
    And I do not hear a time remaining

  Scenario: The time remaining is time to end of track, not time to the mix
    # DOCUMENTED CAVEAT, not a defect. The real transition normally starts
    # earlier, at the outro point or per the fade mode. Confirm the number
    # matches the end of the track so the caveat can stay accurate.
    Given Auto DJ is on and deck 1 is playing a track with 90 seconds left
    When I press "Alt+Shift+N"
    Then I hear approximately "1 minute 30 seconds remaining"
    And the actual crossfade begins earlier than that

  Scenario: With both decks playing only the first is reported
    # The readout breaks on the first playing deck. Expected.
    Given Auto DJ is on and both decks are playing
    When I press "Alt+Shift+N"
    Then I hear a time remaining for deck 1 only
    And I do not hear anything about deck 2

  Scenario: Adding a random track is silent but effective
    # There is deliberately no spoken confirmation. The way to verify is
    # the readout. Do not file the silence as a bug.
    Given Auto DJ is on and the queue is empty
    When I press "Ctrl+Shift+F9"
    Then I hear nothing
    When I press "Alt+Shift+N"
    Then I hear "Next: " followed by a track's artist and title

  Scenario: Shuffling the queue is also silent
    Given the Auto DJ queue contains several tracks
    When I press "Shift+F9"
    Then I hear nothing
    When I press "Alt+Shift+N"
    Then I hear the next track, which may now be a different one

  @blocking
  Scenario: The Auto DJ panel can be tabbed through and escaped
    # All eight controls in this panel used to be mouse-only. The trap risk
    # is the transition spinbox — check you can get back out.
    Given the Auto DJ view is showing
    And focus is on the Auto DJ queue track table
    When I press "Tab" repeatedly
    Then I hear each of these announced in this order
      | Auto DJ button          |
      | Fade now button         |
      | Skip next button        |
      | Transition mode combo   |
      | Transition time spinbox |
      | Shuffle button          |
      | Add random track button |
      | Repeat playlist button  |
    When I press "Tab" until focus is on the transition time spinbox
    And I press "Escape"
    Then focus returns to the library widget I came from
    And I am not stuck inside the panel

  Scenario: Enter also releases focus from the transition controls
    Given focus is on the transition mode combo box
    When I press "Return"
    Then focus returns to the previously focused library widget

  # ---------------------------------------------------------------------
  # Controller navigation without focus — issue #83
  # ---------------------------------------------------------------------

  Scenario: The controller navigation checkbox is reachable and named
    Given a screen reader is running
    When I open Preferences, Accessibility and tab through the General group
    Then my screen reader announces "Allow controller navigation when Mixxx isn't focused"
    And it announces whether it is ticked

  Scenario: The setting takes effect without a restart
    Given a controller with a browse encoder is connected
    And "Allow controller navigation when Mixxx isn't focused" is unticked
    When I move focus to another application
    And I turn the browse encoder
    Then the library selection does not move
    When I return to Mixxx, tick the setting and press OK
    And I move focus to another application again
    And I turn the browse encoder
    Then I hear the library selection moving
    And I did not have to restart Mixxx

  @blocking
  Scenario: Navigating with no focused window does not crash Mixxx
    # This path previously dereferenced a null focus window. It is now
    # guarded, but a crash here loses a whole set, so check it deliberately.
    Given "Allow controller navigation when Mixxx isn't focused" is ticked
    And a controller with a browse encoder is connected
    When I switch to another application so Mixxx has no focus at all
    And I turn the browse encoder several times
    Then Mixxx is still running
    And I hear the library selection moving

  Scenario: The toggle itself says nothing
    # EXPECTED: this is a silent preference with no spoken confirmation.
    When I tick or untick the controller navigation checkbox and press OK
    Then Mixxx does not speak a confirmation

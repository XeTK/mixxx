@manual @windows
Feature: Reading Mixxx with JAWS and NVDA

  Mixxx's own speech covers the performance surfaces — decks, mixer,
  effects — because a screen reader cannot see inside a skin. Everything
  else is the screen reader's job: menus, preferences, dialogs, the search
  box, the sidebar, the track table. That division only works if the
  widgets carry accessible names, and for a long stretch of the preferences
  they did not.

  Issue #84 added label buddies and accessible names across ten preferences
  pages. Issue #88 did the same for the track info dialog and gave the star
  rating a keyboard. Issue #72 exposed the track table's rows and cells as
  accessible text.

  A note on what a failure looks like here: a screen reader announcing
  "edit", "combo box", "blank" or a raw symbol instead of a name means the
  control is anonymous. That is the defect. Announcing the WRONG name is
  worse and rarer — two of those existed in Live Broadcasting before #84
  and are specifically checked below.

  The maintainer uses NVDA and VoiceOver, not JAWS. JAWS scenarios are
  included for completeness but are lower priority; if you only have time
  for one, run NVDA.

  Background:
    Given Windows with a build of this fork
    And a screen reader is running
    And the screen reader's speech is routed to a device I can hear
    And Mixxx is running with tracks in the library

  # ---------------------------------------------------------------------
  # Preferences pages — issue #84
  #
  # None of this is C++; it is all declarative metadata in the .ui files.
  # So there is nothing to trigger — just tab through and listen. A page
  # passes when every control you land on announces something that tells
  # you what it does.
  # ---------------------------------------------------------------------

  @nvda @blocking
  Scenario Outline: Every control on a preferences page announces a useful name
    When I open Preferences and select "<page>"
    And I press "Tab" through every control on the page until focus wraps
    Then each control is announced with a name that describes what it does
    And no control is announced as only its type, such as "edit" or "combo box"
    And no control is announced as "blank"
    And I could configure this page without sighted help

    Examples:
      | page               |
      | Library            |
      | Vinyl Control      |
      | Interface          |
      | Waveforms          |
      | Colors             |
      | Decks              |
      | Effects            |
      | Live Broadcasting  |
      | Key Detection      |
      | Modplug Decoder    |

  @nvda @blocking @regression
  Scenario: Live Broadcasting no longer announces the wrong field names
    # Two buddies pointed at the wrong widgets before #84: the "Stream
    # name" label named the server-type combo, and "Website" named the
    # mount point. A screen reader user filling this in was told they were
    # editing a different field from the one they were in.
    When I open Preferences, Live Broadcasting
    And I tab to the stream name field
    Then my screen reader announces it as the stream name
    And it does not announce it as a server type
    When I tab to the website field
    Then my screen reader announces it as the website
    And it does not announce it as a mount point

  @nvda @regression
  Scenario: Decks no longer announces the wrong field for intro start
    # The "Intro start" label previously named the cue mode combo.
    When I open Preferences, Decks
    And I tab to the intro start control
    Then my screen reader announces it as the intro start setting
    And it does not announce it as a cue mode

  @nvda @blocking
  Scenario: Controls that were unreachable by Tab are now in the chain
    # Several of these were mouse-only, which for a blind user means they
    # did not exist. Confirm each is now reachable.
    When I tab through the pages listed below
    Then I reach every one of these controls
      | page              | control                                     |
      | Live Broadcasting | the keychain password storage radio button  |
      | Decks             | the Smart cue checkbox                      |
      | Decks             | the BPM lock radio button                   |
      | Interface         | the screen saver combo box                  |
      | Colors            | the loop default colour combo box           |
      | Colors            | the key palette combo box                   |
      | Vinyl Control     | the deck 1 pitch estimator combo box        |
      | Vinyl Control     | the deck 2 pitch estimator combo box        |
      | Vinyl Control     | the deck 3 pitch estimator combo box        |
      | Vinyl Control     | the deck 4 pitch estimator combo box        |
      | Vinyl Control     | the signal quality checkbox                 |
      | Waveforms         | the stem opacity spin box                   |
      | Waveforms         | the stem outline opacity spin box           |

  @nvda
  Scenario: The Effects page glyph buttons say what they do
    # Their on-screen text is a single arrow character, which a screen
    # reader reads as nothing useful or as the codepoint.
    When I open Preferences, Effects
    And I tab to the button that hides the selected effect
    Then my screen reader announces "Hide effect"
    When I tab to the button that unhides the selected effect
    Then my screen reader announces "Unhide effect"
    And neither is announced as an arrow symbol or as unlabelled

  @nvda
  Scenario: The Effects page tables are named
    When I open Preferences, Effects
    Then my screen reader announces the visible effects table as "Visible effects"
    And it announces the hidden effects table as "Hidden effects"
    And it announces the quick effect chain presets list as "Quick effect chain presets"
    And it announces the effect chain presets list as "Effect chain presets"

  @nvda
  Scenario: Vinyl Control names each deck's controls by deck number
    # Four decks' worth of identical-looking sliders and combos. Without
    # the deck number in the name they are indistinguishable.
    When I open Preferences, Vinyl Control
    And I tab through the page
    Then I hear "Deck 1 vinyl speed", "Deck 2 vinyl speed", "Deck 3 vinyl speed" and "Deck 4 vinyl speed"
    And I hear "Deck 1 lead-in", "Deck 2 lead-in", "Deck 3 lead-in" and "Deck 4 lead-in"
    And I hear "Deck 1 pitch estimator", "Deck 2 pitch estimator", "Deck 3 pitch estimator" and "Deck 4 pitch estimator"
    And I always know which deck I am configuring

  @nvda @blocking
  Scenario: Key Detection has a tab order at all
    # This page had no tab order defined whatsoever before #84 — focus
    # moved in whatever order the widgets happened to be created in.
    When I open Preferences, Key Detection
    And I press "Tab" repeatedly from the top of the page
    Then focus moves in a sensible order down the page
    And I reach the analyser combo box, announced as "Key detection analyzer"
    And I reach all six notation radio buttons
    And I then reach all 24 custom key notation edit boxes
    And each key edit box is announced with the key it belongs to, such as A major or F sharp minor

  @nvda
  Scenario: Unlabelled sliders and spin boxes are named
    When I open Preferences, Modplug Decoder
    Then my screen reader announces controls named "Bass expansion depth", "Reverb depth" and "Surround depth"
    And it distinguishes "Memory limit for single track" from "Memory limit for single track value"
    When I open Preferences, Waveforms
    Then my screen reader announces "Frame rate value", "End of track warning value" and "Beat grid opacity value"

  @nvda
  Scenario: The Library page names its list and its font controls
    When I open Preferences, Library
    Then my screen reader announces the music directories list as "Music directories"
    And it announces the font field as "Library font"
    And it announces the browse button as "Choose library font", not as three dots

  @jaws
  Scenario: The same preferences pages read correctly under JAWS
    # Lower priority — the maintainer does not use JAWS. Run it if JAWS is
    # available, and record any place JAWS differs from NVDA rather than
    # treating a JAWS-only oddity as a Mixxx bug without checking NVDA too.
    When I repeat the preferences page walkthrough with JAWS running
    Then every control announces a useful name
    And I note any control where JAWS and NVDA disagree

  # ---------------------------------------------------------------------
  # The track info dialog — issue #88
  # ---------------------------------------------------------------------

  @nvda @blocking
  Scenario: Every field in the track info dialog announces its label
    Given a track is selected in the library
    When I open the track info dialog
    And I tab through every field
    Then each field is announced with its label
    And I hear title, artist, album, album artist, composer, year, genre, key, grouping, track number and comment each named
    And I hear duration, type, BPM, bitrate, date added, sample rate, replay gain and location each named
    And no field is announced as only "edit"

  @nvda
  Scenario: The colour picker is named
    Given the track info dialog is open
    When I tab to the colour button
    Then my screen reader announces it with a name describing colour
    And I can operate it with the keyboard

  @nvda @blocking
  Scenario: The star rating can be set from the keyboard
    Given the track info dialog is open for a track with no rating
    When I tab to the star rating
    Then my screen reader announces "Star rating"
    When I press "Right" three times
    Then the track's rating becomes 3 stars
    When I press "Left" once
    Then the rating becomes 2 stars
    When I press "Home"
    Then the rating becomes unrated
    When I press "End"
    Then the rating becomes 5 stars
    When I press "3"
    Then the rating becomes 3 stars
    When I press "7"
    Then the rating becomes 5 stars, because it clamps to the maximum

  @nvda
  Scenario: Up and Down also change the star rating
    Given focus is on the star rating with a rating of 2
    When I press "Up"
    Then the rating becomes 3
    When I press "Down"
    Then the rating becomes 2

  @nvda
  Scenario: The star rating does not announce its new value
    # KNOWN GAP, not a defect to file blind. WStarRating exposes an
    # accessible NAME but no accessible VALUE, and Mixxx's own speech is
    # not wired to it, so changing the rating with the keyboard is silent.
    # The value can only be confirmed by closing the dialog and hearing the
    # row readout. Confirm the gap is real so it can be fixed deliberately.
    Given focus is on the star rating
    When I press "Right"
    Then I record whether anything at all was announced
    When I close the dialog and select the track in the library
    Then I hear the new rating in the row readout

  @nvda
  Scenario: The cover art is reachable and operable
    Given the track info dialog is open
    When I tab to the cover art
    Then my screen reader announces "Cover art"
    When I press "Return"
    Then the full size cover opens
    When I press "Return" again
    Then it closes
    When I press "Space" on the cover art
    Then it behaves the same as Return

  @nvda
  Scenario: The multi-track info dialog is also labelled
    Given 3 tracks are selected in the library
    When I open the track info dialog for all of them
    And I tab through every field
    Then each field is announced with its label
    And the star rating is announced as "Star rating"

  # ---------------------------------------------------------------------
  # The key wheel dialog — issue #88
  # ---------------------------------------------------------------------

  @nvda @blocking @regression
  Scenario: Tab is no longer trapped in the key wheel
    # Tab used to be swallowed by the dialog's own key handling, so once
    # you were in there the only way out was the mouse.
    When I open the key wheel dialog
    And I press "Tab"
    Then focus moves to a control in the dialog
    And I can reach the close button using Tab alone
    When I press "Return" on the close button
    Then the dialog closes

  @nvda @blocking
  Scenario Outline: Up and Down cycle the notation and speak it
    Given the key wheel dialog is open
    When I press "<key>" until the notation reaches "<notation>"
    Then I hear "<spoken>"

    Examples:
      | key  | notation                   | spoken                                |
      | Down | Custom                     | Custom notation                       |
      | Down | OpenKey                    | OpenKey notation                      |
      | Up   | Lancelot                   | Lancelot notation                     |
      | Up   | Traditional                | Traditional notation                  |
      | Up   | OpenKey and Traditional    | OpenKey and Traditional notation      |
      | Up   | Lancelot and Traditional   | Lancelot and Traditional notation     |
      | Up   | ID3v2                      | ID3v2 notation                        |

  @nvda
  Scenario: The notation choice sticks after closing the dialog
    Given the key wheel dialog is open
    When I cycle the notation to Lancelot and hear "Lancelot notation"
    And I close the dialog
    And I press "Alt+7"
    Then deck 1's key is read in Lancelot notation

  # ---------------------------------------------------------------------
  # The track table — issue #72
  #
  # Two separate surfaces: the composed row readout that Mixxx speaks on
  # selection, and the per-cell accessible text a screen reader reads when
  # you navigate cell by cell.
  #
  # Row readout order is fixed: artist, title, "BPM locked" if locked,
  # rating, colour, times played, then ", N of M".
  # ---------------------------------------------------------------------

  @nvda @blocking
  Scenario: Selecting a track speaks its full state, not just its name
    Given a track with 3 stars, a colour, marked as played, and a locked BPM
    When I arrow onto it in the library
    Then I hear its artist, then its title
    And I hear "BPM locked"
    And I hear "3 stars"
    And I hear its colour
    And I hear that it has been played
    And I hear its position, such as "7 of 40"

  @nvda
  Scenario Outline: Rating is spoken correctly at every value
    Given a track rated "<rating>"
    When I arrow onto it in the library
    Then I hear "<spoken>" in the row readout

    Examples:
      | rating | spoken  |
      | 0      | Unrated |
      | 1      | 1 star  |
      | 2      | 2 stars |
      | 5      | 5 stars |

  @nvda
  Scenario Outline: Played state is spoken correctly
    Given a track "<state>"
    When I arrow onto it in the library
    Then I hear "<spoken>" in the row readout

    Examples:
      | state                     | spoken            |
      | never played              | Not played        |
      | played once               | Played, 1 time    |
      | played four times         | Played, 4 times   |

  @nvda
  Scenario: Track colour is read as a hex code, not a colour name
    # KNOWN ROUGH EDGE, not a defect to file blind. The colour is spoken as
    # its hex value, so a red track reads as "Color #ff0000" — the screen
    # reader will spell out the hash and the digits. Confirm what it
    # actually sounds like so a decision can be made about naming colours.
    Given a track coloured red
    When I arrow onto it in the library
    Then I hear the colour announced
    And I record exactly how it was spoken
    Given a track with no colour set
    When I arrow onto it
    Then I hear "No color"

  @nvda
  Scenario: A single-row table omits the position
    Given a search that returns exactly one track
    When I arrow onto it
    Then I hear its artist and title
    And I do not hear "1 of 1"

  @nvda
  Scenario: The row readout only fires when the track table has focus
    Given focus is in the library sidebar
    When the selected track in the table changes
    Then I do not hear a row readout
    When I move focus to the track table
    And I arrow to another track
    Then I hear the full row readout

  @nvda @blocking
  Scenario: Navigating cell by cell reads each cell's accessible text
    Given the library track table has focus
    When I navigate across the columns of a single row with my screen reader
    Then the rating cell is read as its star count or "Unrated"
    And the colour cell is read as its colour or "No color"
    And the times-played cell is read as "Not played" or "Played, N times"
    And the BPM cell of a BPM-locked track is read as the BPM followed by "BPM locked"
    And every other cell is read as the text that is displayed

  @jaws
  Scenario: The track table reads under JAWS as well
    When I repeat the track table scenarios with JAWS running
    Then rows and cells are announced with the same information
    And I note any place JAWS differs from NVDA

  # ---------------------------------------------------------------------
  # Live-performance safety
  # ---------------------------------------------------------------------

  @nvda @audio @twodevices
  Scenario: The screen reader does not speak to the audience
    Given a separate main output feeding an audience and a headphone output
    And the screen reader's output device is set to something other than the performance interface
    And the Windows default audio device is not the performance interface
    When I navigate the library and the preferences with the screen reader
    Then nothing the screen reader says is audible on the main output
    And the mix is unaffected

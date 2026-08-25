@manual @macos @voiceover
Feature: Keyboard chords surviving macOS VoiceOver

  VoiceOver claims Ctrl+Option — which Qt reports as Ctrl+Alt — as its own
  command modifier. Every chord in that namespace was being eaten before
  Mixxx ever saw the key press, which meant a blind macOS user running the
  screen reader they need in order to use the rest of the system could not
  use the fork's own shortcuts. Issue #58 moved five deck-1 chords and five
  deck-2 chords out of that namespace.

  None of this is testable without VoiceOver actually running. "VoiceOver is
  installed" is not the same thing: the modifier is only claimed while VO is
  active. Turn it on with Cmd+F5 and leave it on for every scenario here.

  Three things need checking, and only the first is obvious:
    1. the new chords work,
    2. the old chords are really gone (a chord that still works is a
       half-applied config, and the docs now lie),
    3. VoiceOver's own commands still work — moving Mixxx off Ctrl+Option
       is pointless if Mixxx has broken VO navigation in exchange.

  Background:
    Given macOS with VoiceOver running
    And Mixxx is running with the en_US keyboard layout
    And speech is on
    And "Speak deck names as numbers" is enabled in Preferences, Accessibility
    And tracks are loaded in deck 1 and deck 2
    And the Mixxx window has keyboard focus

  # ---------------------------------------------------------------------
  # The new chords — issue #58
  # ---------------------------------------------------------------------

  @blocking
  Scenario Outline: Deck 1 chords reach Mixxx with VoiceOver running
    When I press "<chord>"
    Then I hear "<heard>"
    And VoiceOver does not announce a VoiceOver command instead

    Examples:
      | chord | heard                |
      | Alt+H | Deck 1 B P M halved  |
      | Alt+D | Deck 1 B P M doubled |

  @blocking
  Scenario Outline: Deck 2 chords reach Mixxx with VoiceOver running
    When I press "<chord>"
    Then I hear "<heard>"

    Examples:
      | chord       | heard                |
      | Alt+Shift+H | Deck 2 B P M halved  |
      | Alt+Shift+D | Deck 2 B P M doubled |

  @blocking
  Scenario: Quantize toggles for both decks
    Given deck 1 quantize is on
    When I press "Alt+Q"
    Then I hear "Deck 1 quantize off"
    When I press "Alt+Q"
    Then I hear "Deck 1 quantize on"
    When I press "Alt+Shift+Q"
    Then I hear a spoken confirmation of deck 2's quantize state

  @blocking
  Scenario: The deck-1 filing pickers open on the new chords
    Given at least one crate and one playlist exist
    And a track is loaded in deck 1
    When I press "Alt+C"
    Then I hear the first crate name announced with its position
    When I press "Escape"
    When I press "Alt+P"
    Then I hear the first playlist name announced with its position
    When I press "Escape"

  Scenario: Deck 2's filing pickers use Ctrl+Shift, not Alt+Shift
    # Alt+Shift+P and Alt+Shift+C were already taken by the Library's own
    # Add to Playlist / Add to Crate, so deck 2 got Ctrl+Shift instead.
    # That asymmetry is deliberate and worth confirming both halves of.
    Given at least one crate and one playlist exist
    And a track is loaded in deck 2
    And a track is selected in the library
    When I press "Ctrl+Shift+C"
    Then I hear a crate picker open for deck 2's loaded track
    When I press "Escape"
    When I press "Alt+Shift+C"
    Then I hear a crate picker open for the library selection, not for deck 2

  # ---------------------------------------------------------------------
  # The reverse check: the old chords must be gone
  # ---------------------------------------------------------------------

  @blocking @regression
  Scenario Outline: Old Ctrl+Alt chords no longer do anything in Mixxx
    # A chord that still fires means a stale kbd.cfg somewhere — most likely
    # a user-modified copy in the settings directory shadowing the shipped
    # one. Check ~/Library/Application Support/Mixxx before filing.
    When I press "<chord>"
    Then Mixxx does not speak
    And nothing about deck 1 or deck 2 has changed

    Examples: deck 1
      | chord      |
      | Ctrl+Alt+H |
      | Ctrl+Alt+D |
      | Ctrl+Alt+P |
      | Ctrl+Alt+C |
      | Ctrl+Alt+Q |

    Examples: deck 2
      | chord            |
      | Ctrl+Alt+Shift+H |
      | Ctrl+Alt+Shift+D |
      | Ctrl+Alt+Shift+P |
      | Ctrl+Alt+Shift+C |
      | Ctrl+Alt+Shift+Q |

  Scenario: The old chords are also gone with VoiceOver switched off
    # With VO off, Ctrl+Alt reaches Mixxx normally. This distinguishes
    # "the binding was removed" from "VoiceOver is still eating it".
    Given VoiceOver is turned off
    When I press "Ctrl+Alt+H"
    Then Mixxx does not speak
    And deck 1's BPM is unchanged
    When I press "Alt+H"
    Then I hear "Deck 1 B P M halved"

  # ---------------------------------------------------------------------
  # VoiceOver's own commands must still work
  # ---------------------------------------------------------------------

  @blocking @regression
  Scenario: VoiceOver navigation still works inside Mixxx
    When I press "Ctrl+Option+Right Arrow"
    Then VoiceOver moves to the next element and announces it
    When I press "Ctrl+Option+Left Arrow"
    Then VoiceOver moves back and announces the previous element
    When I press "Ctrl+Option+Space"
    Then VoiceOver activates the focused element

  Scenario: The VoiceOver rotor opens over the Mixxx window
    When I press "Ctrl+Option+U"
    Then VoiceOver announces the rotor
    When I press "Escape"
    Then VoiceOver closes the rotor
    And Mixxx has not reacted to any of those presses

  Scenario: VoiceOver reads the Mixxx menu bar
    When I press "Ctrl+Option+M"
    Then VoiceOver announces the Mixxx menu bar
    And I can arrow through the top-level menus hearing each one

  # ---------------------------------------------------------------------
  # The unresolved collision between #58 and #56
  #
  # #58 moved ten chords OUT of the Ctrl+Alt namespace because VoiceOver
  # eats it. #56 then added roughly two dozen NEW mixer and effects chords
  # INTO that same namespace: Ctrl+Alt+B/V/G/T/E/W/O/F/N/S/A/I/J/X/L,
  # Ctrl+Alt+F9/F10/F11, and their Ctrl+Alt+Shift deck-2 twins.
  #
  # If VoiceOver eats Ctrl+Option the way #58 says it does, then the entire
  # keyboard-only mixing and effects workflow #56 was written to enable is
  # unusable on macOS with a screen reader running — the exact user and the
  # exact platform this fork exists for.
  #
  # Neither commit mentions the other. These scenarios exist to establish
  # the fact one way or the other, with evidence, before anyone argues
  # about it. Run them early.
  # ---------------------------------------------------------------------

  @blocking @inference
  Scenario Outline: Do the new mixer chords survive VoiceOver at all
    # VALIDATING AN ASSUMPTION — and the assumption may well be wrong.
    # Record the result for every row even when they all behave the same.
    When I press "<chord>"
    Then I hear "<expected>"
    # If instead VoiceOver speaks one of its own commands, or nothing
    # happens, log it as "eaten by VoiceOver" against this row.

    Examples: deck 1 mixer
      | chord      | expected                            |
      | Ctrl+Alt+V | deck 1's volume announced going up   |
      | Ctrl+Alt+B | deck 1's volume announced going down |
      | Ctrl+Alt+T | deck 1's trim announced going up     |
      | Ctrl+Alt+G | deck 1's trim announced going down   |

    Examples: deck 1 EQ and filter
      | chord        | expected                        |
      | Ctrl+Alt+E   | deck 1's EQ low announced down   |
      | Ctrl+Alt+F11 | deck 1's EQ low announced up     |
      | Ctrl+Alt+W   | deck 1's EQ mid announced down   |
      | Ctrl+Alt+O   | deck 1's EQ mid announced up     |
      | Ctrl+Alt+F   | deck 1's EQ high announced down  |
      | Ctrl+Alt+F10 | deck 1's EQ high announced up    |
      | Ctrl+Alt+N   | deck 1's filter announced down    |
      | Ctrl+Alt+S   | deck 1's filter announced up      |

    Examples: effects
      | chord       | expected                                |
      | Ctrl+Alt+A  | effect unit 1 announced switching on     |
      | Ctrl+Alt+F9 | effect unit 1's next chain preset spoken |
      | Ctrl+Alt+L  | effect unit 1's focused effect spoken    |
      | Ctrl+Alt+I  | effect slot 1 announced switching on     |
      | Ctrl+Alt+J  | the next effect in slot 1 spoken         |
      | Ctrl+Alt+X  | the previous effect in slot 1 spoken     |

  @blocking
  Scenario: The same mixer chords with VoiceOver off, as a control
    # If these all work with VO off and none work with VO on, the finding
    # is unambiguous and #56 needs the same treatment #58 got.
    Given VoiceOver is turned off
    When I press "Ctrl+Alt+V"
    Then I hear deck 1's volume announced going up
    When I press "Ctrl+Alt+A"
    Then I hear effect unit 1 announced switching on

  # ---------------------------------------------------------------------
  # Other chords worth checking against VoiceOver, not covered by #58
  # ---------------------------------------------------------------------

  Scenario Outline: The AccessMenu chords are not claimed by VoiceOver
    # #57's chords are in the Alt+Shift namespace, which should be clear,
    # but they are registered as application-wide Qt shortcuts and have
    # never been checked against VO.
    When I press "<chord>"
    Then Mixxx responds as documented for that chord
    And VoiceOver does not announce a VoiceOver command instead

    Examples:
      | chord               |
      | Alt+Shift+M         |
      | Alt+Shift+Up        |
      | Alt+Shift+Down      |
      | Alt+Shift+Return    |
      | Alt+Shift+Backspace |
      | Alt+Shift+Space     |

  Scenario Outline: The library sort chords are not claimed by VoiceOver
    Given the library track table has focus
    When I press "<chord>"
    Then Mixxx responds as documented for that chord

    Examples:
      | chord            |
      | Alt+Shift+S      |
      | Ctrl+Alt+Shift+S |
      | Alt+Shift+O      |
      | Alt+Shift+V      |

  Scenario: Keylock and pitch chords from issue #50 are in the eaten namespace too
    # #50 put keylock on Ctrl+Alt+K and pitch on Ctrl+Alt+Up/Down — the same
    # namespace #58 was fixing. Same question as the #56 block above.
    When I press "Ctrl+Alt+K"
    Then I hear "Deck 1 key lock on"
    # If VoiceOver takes it instead, log it alongside the #56 findings —
    # it is the same problem and wants the same fix.

  # ---------------------------------------------------------------------
  # macOS speech quality, since you are here
  # ---------------------------------------------------------------------

  @audio
  Scenario: Mixxx speech and VoiceOver speech do not collide
    Given VoiceOver is set to output to the built-in speakers
    And "Speech output" in Preferences, Accessibility is set to headphones
    When I press "Alt+1"
    Then I hear deck 1's status in the headphones
    And VoiceOver's own speech stays on the built-in speakers
    And neither one cuts the other off mid-word

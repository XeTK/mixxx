@manual @e2e @audio @blocking
Feature: A complete mix, performed without sight

  This is the acceptance test. Everything else in the plan checks a
  behaviour; this checks whether the behaviours add up to something a
  person can actually DJ with.

  Rules for running it:

    - Turn the monitor off, or close your eyes and keep them closed, or
      have someone else confirm you did not look. If you glance at the
      screen even once to recover from being lost, the run has failed —
      note where you looked and why, because that is the finding.
    - Do not use the mouse.
    - Do it at a realistic pace. Taking four minutes to find a track is a
      fail even if every announcement was correct, and that will not show
      up in any per-behaviour scenario.
    - Keep a stopwatch. Record how long the whole run took and where the
      time went.

  Run it twice: once keyboard-only, once with the DDJ-400. They exercise
  genuinely different paths and different failure modes.

  Key chords used below are the en_US defaults. Deck 1 is the left hand
  side of the keyboard, deck 2 the right.

  Background:
    Given a library containing at least 200 analysed tracks
    And headphones on a separate output from the main output
    And "Speech output" in Preferences, Accessibility is set to headphones
    And "Speak deck names as numbers" is enabled
    And "Smart cue" is enabled in Preferences, Decks
    And speech is on
    And I cannot see the screen

  # ---------------------------------------------------------------------

  @keyboard
  Scenario: A full mix from launch to filed track, keyboard only
    # ---- Getting in ----
    Given no DJ controller is connected
    When I launch Mixxx
    Then I hear "Mixxx ready"
    And I know from that alone that the audio device opened

    # ---- Finding the first track ----
    When I press "Tab" until I reach the search box
    And I type an artist name I know is in the library
    Then I hear how many tracks matched
    When I press "Escape" to move to the track list
    And I arrow through the results
    Then I hear each track's artist, title and position
    And I can tell the tracks apart well enough to choose one

    # ---- Loading and previewing ----
    When I press "Shift+Left" to load the selected track into deck 1
    Then I hear "Loaded deck, Alpha" followed by the artist, title, BPM and key
    And I hear the headphone cue move to deck 1 in the same announcement
    And I hear deck 1 in my headphones and nothing on the main output
    When I press "D" to start deck 1
    Then I hear it start playing
    When I press "Alt+1"
    Then I hear deck 1's full status: playing, time remaining, BPM and pitch

    # ---- Out to the room ----
    When I move the crossfader fully towards deck 1 using "H"
    Then deck 1 is audible on the main output
    When I press "T" to take deck 1 out of the headphones
    Then I hear "Deck 1 headphone cue off"

    # ---- Finding the next track by key and tempo ----
    When I press "Alt+5"
    Then I hear deck 1's BPM
    When I press "Alt+7"
    Then I hear deck 1's musical key
    When I search the library for a track and check its BPM and key from the row readout
    Then I can choose a track that will mix with deck 1 without seeing the screen

    # ---- Loading and cueing the second deck ----
    When I press "Shift+Right" to load it into deck 2
    Then I hear "Loaded deck, Bravo" with the artist, title, BPM and key
    And I hear the headphone cue move to deck 2
    And I hear deck 2 in my headphones while deck 1 continues on the main output

    # ---- Beatmatching by ear ----
    When I press "Alt+B" to turn the beat click on
    Then I hear "Beat click on"
    And I hear deck 1's click in my left ear
    When I press "Alt+S"
    Then I hear "Split cue on. Deck 1 left, deck 2 right"
    When I press "L" to start deck 2
    And I adjust deck 2's pitch with "F7" and "F8" until the two decks are in time
    Then I can hear the drift closing
    And I can hold them in time by ear alone
    When I press "Alt+6"
    Then I hear deck 2's BPM, close to deck 1's

    # ---- Setting up a mix point ----
    When I press "Shift+L" to set deck 2's cue point where I want the mix to start
    Then I hear the cue point confirmed
    When I press "Z" to jump to a hotcue on deck 1
    Then I hear deck 1 respond
    When I press "U" to set a beat loop on deck 2
    Then I hear the loop announced with its length
    When I press "I" to halve it
    Then I hear the new loop size
    When I press "O" to double it back
    Then I hear the loop size return to where it was
    When I press "9" to release the loop
    Then I hear the loop turn off

    # ---- The mix itself ----
    When I bring the crossfader across towards deck 2 using "G"
    Then I hear both tracks blending on the main output
    When I press "Ctrl+Alt+E" repeatedly to pull deck 1's bass out
    Then I hear the announcement name deck 1's EQ low and give me a value
    And deck 1's bass leaves the mix
    When I press "Ctrl+Alt+Shift+F11" to bring deck 2's bass up
    Then deck 2 carries the low end
    When I press "Ctrl+Alt+A" to switch effect unit 1 on
    Then I hear the unit announced as on
    And I hear the effect in the mix
    When I press "Ctrl+Alt+A" again
    Then the effect leaves the mix
    When I complete the crossfade to deck 2 using "G"
    And I press "Alt+3"
    Then I hear deck 1's time remaining, confirming it is safe to stop
    When I press "D" to stop deck 1
    Then I hear it stop

    # ---- Filing the track that worked ----
    When I press "Ctrl+Shift+C" to add deck 2's loaded track to a crate
    Then I hear the first crate name with its position
    When I arrow to the crate I want
    Then I hear each crate name and position as I go
    When I press "Return"
    Then I hear the track confirmed as added to that crate

    # ---- Handing over to Auto DJ ----
    When I add several tracks to the Auto DJ queue
    And I press "Shift+F12"
    Then I hear "Auto DJ on. Next: " followed by a track's artist and title
    When I press "Alt+Shift+N"
    Then I hear the Auto DJ state, the next track and the time remaining on the playing deck
    When I press "Shift+F11"
    Then I hear "Fading now"
    And the mix moves on without me touching anything

    # ---- Getting out ----
    When I press "Shift+F12"
    Then I hear "Auto DJ off"
    When I press "Alt+Shift+Left" while deck 1 is stopped
    Then I hear "Deck 1 track ejected"
    When I press "Ctrl+Q"
    Then Mixxx exits cleanly

    # ---- The verdict ----
    Then I performed the entire mix without looking at the screen
    And I never had to guess at the state of a deck
    And I was never left in silence wondering whether a key press registered
    And I record how long the run took and where I lost time

  # ---------------------------------------------------------------------

  @hardware @ddj400
  Scenario: The same mix on the DDJ-400
    # The controller run exists to catch a different class of problem: not
    # "is the announcement right" but "can I find the control at all, and
    # does the hardware agree with the mapping".
    Given the DDJ-400 is connected and its mapping is enabled
    And "Use the Hot Cue pads as accessibility pads" is enabled
    And no keyboard is used except where noted

    When I launch Mixxx
    Then I hear "Mixxx ready"
    When I press "Alt+2" on the keyboard
    Then I hear deck 2's status, confirming both decks were configured

    When I turn the browse knob to move through the library
    Then I hear one track announced per detent
    And turning it the other way takes me back the way I came
    When I press LOAD on the left deck
    Then I hear the track loaded into deck 1 with its artist, title, BPM and key
    And I hear the headphone cue move to deck 1

    When I press the left deck's PLAY button
    Then I hear it start
    When I press Pad 1 on the left deck
    Then I hear deck 1's full status
    When I press Pad 3 on the left deck
    Then I hear deck 1's BPM
    When I press Pad 4 on the left deck
    Then I hear deck 1's musical key

    When I load a second track into deck 2 the same way
    Then I hear it announced with its own details
    When I press Shift + Pad 8 on the left deck
    Then speech toggles, and I toggle it straight back on
    When I press Shift + Pad 7 on the left deck
    Then I hear the split cue state announced

    When I move the left deck's tempo fader
    Then deck 1's speed changes smoothly and predictably
    When I press Shift + SYNC on the left deck
    Then I hear deck 1's tempo range announced
    And I know how much the fader will now move the pitch

    When I press Shift + Pad 3 on the left deck
    Then I hear "Deck 1 key lock on"
    When I press Shift + Pad 4 twice
    Then deck 1 sounds two semitones lower
    When I press Shift + Pad 6
    Then deck 1 returns to its original key

    When I press the BEAT LOOP pad mode button
    Then I hear "Pads, beat loop"
    When I set a loop from the pads
    Then I hear the loop announced
    When I press CUE/LOOP CALL left
    Then I hear the halved loop size
    When I press CUE/LOOP CALL right
    Then I hear it doubled back

    When I press Shift + HOT CUE
    Then I hear "Pads, keyboard (not yet supported)"
    And pressing the pads does nothing, exactly as I was told

    When I press the HOT CUE pad mode button
    Then I hear "Pads, hot cues"
    When I move the channel faders and the crossfader to bring deck 2 in
    Then I hear the mixer positions announced as I move them
    When I use the EQ knobs to swap the bass between decks
    Then I hear each EQ band named and its value announced
    When I press the BEAT FX paddle to change the focused effect
    Then I hear "Unit 1: " followed by the effect name and " focused"

    When I press Shift + CUE on the left deck
    Then I hear "Deck 1 back to start"
    When I stop deck 1
    Then I hear it stop

    When I press Ctrl+Shift+C on the keyboard to file deck 2's track
    Then I hear the crate picker and can complete the add using the browse knob
    Then I hear the track confirmed as added

    Then I performed the mix using the controller and my ears alone
    And every control I reached for told me what it did
    And I record any control I could not find or could not identify by sound

  # ---------------------------------------------------------------------

  @keyboard @timing
  Scenario: Recovering from getting lost
    # The realistic failure in a live set is not a wrong announcement, it
    # is losing track of state. Every one of these must be answerable in
    # one key press.
    Given both decks are loaded and one is playing
    And I have deliberately lost track of what is where
    When I press "Alt+1"
    Then I know whether deck 1 is playing, how long is left, its BPM and its pitch
    When I press "Alt+2"
    Then I know the same about deck 2
    When I press "Alt+Shift+T"
    Then I hear deck 1's artist and title
    When I press "Alt+Shift+Y"
    Then I hear deck 2's artist and title
    When I press "Alt+Shift+R"
    Then I hear the last announcement repeated
    Then I have recovered full awareness of the session in under 10 seconds

  @keyboard
  Scenario: Nothing destroys anything by accident during a set
    # Run this at the end of a mix run, while tired and moving fast — that
    # is when the accident happens.
    Given a mix is in progress with both decks loaded
    When I press "Return" repeatedly while focus is in the library
    Then no track is purged, hidden, removed or deleted
    When I open a track context menu and press "Escape"
    Then nothing has changed
    When I press "Alt+Shift+Left" while deck 1 is playing
    Then I hear "Deck 1 is playing, eject blocked. Stop the deck first."
    And deck 1 keeps playing
    And the mix is undisturbed throughout

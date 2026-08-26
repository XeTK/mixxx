@manual @hardware @ddj400
Feature: Pioneer DDJ-400 on real hardware

  Everything in this file needs the physical controller. The emulator in
  tools/ddj400-emulator/ and the unit tests both speak whatever byte
  convention we told them to speak, so they can only prove the mapping is
  self-consistent — they cannot prove it matches the hardware. That is what
  these scenarios are for.

  The highest-risk items are tagged @inference. Those were written from a
  sibling controller's mapping or from an unverified reading of the
  hardware, and nobody has ever watched them work. Treat a failure there as
  information, not as a regression.

  Background:
    Given the DDJ-400 is connected by USB and powered on
    And Mixxx is running with the shipped "Pioneer DDJ-400" mapping enabled
    And speech is on
    And "Speak deck names as numbers" is enabled in Preferences, Accessibility
    And "Concise announcements" is disabled
    And a track is loaded in deck 1 and a track is loaded in deck 2

  # ---------------------------------------------------------------------
  # Browse knob — issue #47
  #
  # THE SINGLE HIGHEST-RISK UNVERIFIED ITEM IN THE WHOLE FORK.
  #
  # The DDJ-400's browse knob is bound through a Script-Binding (needed so
  # the knob can drive the spoken AccessMenu when it is open) rather than
  # through Mixxx's <SelectKnob/> MIDI option. That means the mapping has to
  # decode the encoder's bytes itself, and the convention it decodes was
  # INFERRED, not measured: we assumed 7-bit two's complement, because that
  # is what <SelectKnob/> decodes and because the DDJ-FLX4 — a sibling
  # controller with an identical 0xB6/0x40 browse binding — uses it.
  #
  # If the DDJ-400 actually uses the offset-64 convention (0x41 = up,
  # 0x3F = down) that this same mapping uses for its absolute knobs and
  # faders, then 0x41 decodes as +65 and 0x3F as +63 — both clamp to +1, and
  # the knob scrolls DOWN THE LIST IN BOTH DIRECTIONS while appearing to
  # "work". That failure is quiet and easy to mistake for a stuck encoder,
  # which is exactly why scenario "Browse knob byte convention" checks the
  # raw bytes rather than the observed behaviour.
  #
  # The pre-fork bug this replaced also matters for the regression check:
  # the old code passed the raw MIDI value straight through, so one detent
  # reported as 0x7F became 127 rows of movement.
  # ---------------------------------------------------------------------

  @inference @blocking @midimonitor
  Scenario: Browse knob byte convention matches what the mapping assumes
    # VALIDATING AN ASSUMPTION. Read the block comment above first.
    # This is the one scenario in the plan whose result cannot be guessed.
    Given Mixxx is closed
    And a MIDI monitor is running and receiving from the DDJ-400
    When I turn the browse knob one detent clockwise
    Then the monitor logs exactly one Control Change message
    And its status byte is "0xB6"
    And its controller number is "0x40"
    And I write down its data byte verbatim in the run log
    When I turn the browse knob one detent counter-clockwise
    Then the monitor logs exactly one Control Change message with status "0xB6" and controller "0x40"
    And I write down its data byte verbatim in the run log
    # Pass condition, stated explicitly so there is no room to interpret:
    And the clockwise data byte is "0x01" and the counter-clockwise data byte is "0x7F"
    # If instead you logged 0x41 clockwise and 0x3F counter-clockwise, the
    # controller uses offset-64 and the decode in
    # res/controllers/Pioneer-DDJ-400-script.js is wrong. Record both bytes
    # and stop — the rest of the browse scenarios will produce misleading
    # results until this is settled.

  @inference @blocking
  Scenario: One detent moves the library selection exactly one row
    Given the library is focused on a playlist with at least 20 tracks
    And the track at the top of the visible list is selected
    When I turn the browse knob one detent clockwise
    Then I hear the next track down the list announced
    And I hear exactly one track announced, not a burst of them
    When I turn the browse knob one detent counter-clockwise
    Then I hear the track I started on announced again

  @inference @blocking
  Scenario: The browse knob is not inverted
    # Guards against the silent failure mode where both directions decode
    # positive and the list only ever moves one way.
    Given the library is focused on a playlist with at least 20 tracks
    And I note the selected track's title
    When I turn the browse knob 5 detents clockwise
    And I turn the browse knob 5 detents counter-clockwise
    Then I hear the title I noted announced again

  @regression
  Scenario: A fast browse-knob spin does not run away
    # The clamp exists because the pre-fork code sent the raw value through:
    # a single detent reporting 0x7F moved the selection 127 rows.
    Given the library is focused on a playlist with at least 60 tracks
    And the first track in the list is selected
    When I spin the browse knob quickly through about 10 detents clockwise
    Then the selection has moved by roughly 10 tracks, not by dozens
    And I can still hear individual track announcements rather than one jump to the end of the list

  @inference
  Scenario: The browse knob drives the spoken AccessMenu when it is open
    Given the AccessMenu is open
    When I turn the browse knob one detent clockwise
    Then I hear the next menu item announced
    And the library selection behind the menu has not moved
    When I close the AccessMenu
    And I turn the browse knob one detent clockwise
    Then I hear a library track announced, not a menu item

  # ---------------------------------------------------------------------
  # Pad-mode MIDI note numbers — long-standing issue #18
  #
  # The eight pad-mode note numbers in PioneerDDJ400.padModePressed were
  # never confirmed against hardware. The hardware switches the pads' notes
  # internally when a mode button is pressed, so a wrong number here means
  # a mode announces as the wrong layer — actively misleading, worse than
  # silence.
  #
  # The map under test (deck 1 status 0x90, deck 2 status 0x91):
  #   0x1B hot cues   0x6D beat loop   0x20 beat jump   0x22 sampler
  #   0x69 keyboard   0x1E pad fx 1    0x6B pad fx 2    0x6F key shift
  # ---------------------------------------------------------------------

  @inference @blocking @midimonitor
  Scenario Outline: Pad-mode button note numbers match the mapping
    # VALIDATING AN ASSUMPTION (issue #18). Log every byte even when it matches.
    Given Mixxx is closed
    And a MIDI monitor is running and receiving from the DDJ-400
    When I press the "<button>" pad mode button on the left deck
    Then the monitor logs a Note On with status "0x90" and note number "<note>"
    And I write the observed note number in the run log

    Examples: unshifted modes
      | button     | note |
      | HOT CUE    | 0x1B |
      | BEAT LOOP  | 0x6D |
      | BEAT JUMP  | 0x20 |
      | SAMPLER    | 0x22 |

    Examples: shifted modes
      | button              | note |
      | Shift + HOT CUE     | 0x69 |
      | Shift + BEAT LOOP   | 0x1E |
      | Shift + BEAT JUMP   | 0x6B |
      | Shift + SAMPLER     | 0x6F |

  @inference @midimonitor
  Scenario: Right-deck pad mode buttons use status 0x91
    Given Mixxx is closed
    And a MIDI monitor is running and receiving from the DDJ-400
    When I press the "HOT CUE" pad mode button on the right deck
    Then the monitor logs a Note On with status "0x91" and note number "0x1B"

  # ---------------------------------------------------------------------
  # Pad-mode announcements and honesty about dead layers — issue #65
  # ---------------------------------------------------------------------

  @blocking
  Scenario Outline: Each pad mode announces the layer it selected
    When I press the "<button>" pad mode button
    Then I hear "<spoken>"

    Examples: layers that actually do something
      | button              | spoken               |
      | HOT CUE             | Pads, hot cues       |
      | BEAT LOOP           | Pads, beat loop      |
      | BEAT JUMP           | Pads, beat jump      |
      | SAMPLER             | Pads, sampler        |

    Examples: layers with no working pads behind them
      | button              | spoken                                  |
      | Shift + HOT CUE     | Pads, keyboard (not yet supported)       |
      | Shift + BEAT LOOP   | Pads, pad effects 1 (not yet supported) |
      | Shift + BEAT JUMP   | Pads, pad effects 2 (not yet supported) |
      | Shift + SAMPLER     | Pads, key shift (not yet supported)     |

  Scenario: A dead pad layer really is dead, exactly as announced
    # The point of the suffix is that it tells the truth. Check that it does.
    When I press "Shift + HOT CUE"
    Then I hear "Pads, keyboard (not yet supported)"
    When I press each of the 8 pads in turn
    Then nothing happens to either deck
    And I hear nothing from any of the pad presses

  @regression
  Scenario: Re-pressing the mode you are already in announces again
    # Changed by #65. It used to stay silent, which left you unsure whether
    # the press registered at all. The mapping now bounces the control
    # through 0 first; 0 is outside the spoken vocabulary so the bounce
    # itself must stay silent.
    When I press "BEAT LOOP"
    Then I hear "Pads, beat loop"
    When I press "BEAT LOOP" again
    Then I hear "Pads, beat loop"
    And I hear it exactly once, with no extra word before or after it

  @regression
  Scenario: The Numark Scratch mode button still announces once, not twice
    # The pad_mode control keeps its no-op-ignoring default because the
    # Numark Scratch mapping fires the mode button on both decks at once and
    # relies on the duplicate being dropped. #65's bounce could have broken
    # that. Needs the Numark Scratch, not the DDJ-400.
    Given the Numark Scratch is connected instead of the DDJ-400
    And the shipped "Numark Scratch" mapping is enabled
    When I press the pad mode selector button once
    Then I hear one pad mode announcement
    And I do not hear the same announcement twice in a row

  # ---------------------------------------------------------------------
  # Keylock and pitch on Shift + pads 3-6 — issue #50
  #
  # These pads used to deliberately do nothing, so that a stray press could
  # not clear a stored hotcue. #50 gave them keylock and pitch instead.
  # The choice of Shift+pads over the Key Shift pad mode is itself an
  # inference: the Key Shift layer has no confirmed hardware note layout in
  # this codebase, so it was avoided.
  #
  # Shift-pad MIDI: deck 1 status 0x98, deck 2 status 0x9A, data byte =
  # pad number minus 1. So deck 1 Shift+Pad 3 is (0x98, 0x02, 0x7F).
  # ---------------------------------------------------------------------

  @blocking
  Scenario: Shift + Pad 3 toggles keylock on the pad's own deck
    Given deck 1 keylock is off
    When I press Shift + Pad 3 on the left deck
    Then I hear "Deck 1 key lock on"
    When I press Shift + Pad 3 on the left deck again
    Then I hear "Deck 1 key lock off"
    And deck 2's keylock state has not changed

  Scenario: Shift + Pad 3 on the right deck affects deck 2 only
    Given deck 2 keylock is off
    When I press Shift + Pad 3 on the right deck
    Then I hear "Deck 2 key lock on"

  Scenario: Shift + Pads 4 and 5 shift the pitch by ear
    # NOTE: pitch_up / pitch_down / reset_key have NO spoken confirmation of
    # their own — there is no observer for them in AnnouncementManager. That
    # is current intended behaviour, not a bug to file. Verify by ear, and
    # use Alt+7 to read the key back.
    Given deck 1 is playing a track with a known musical key
    And deck 1 keylock is on
    When I press Shift + Pad 4 on the left deck
    Then the track sounds one semitone lower
    And I hear no announcement from the press itself
    When I press Alt+7
    Then I hear the key read back one semitone lower than it started
    When I press Shift + Pad 5 on the left deck
    Then the track sounds back at its original pitch

  Scenario: Shift + Pad 6 resets the key to the track's original
    Given deck 1 is playing
    And I have pressed Shift + Pad 4 three times so the track is three semitones down
    When I press Shift + Pad 6 on the left deck
    And I press Alt+7
    Then I hear the track's original key read back

  @regression @blocking
  Scenario: Shift + pads 3-6 no longer clear hotcues
    # This is the safety property the pads used to have by being dead.
    # Confirm the new bindings did not reopen the hole.
    Given the accessibility pad layer is off in Preferences, Controllers, DDJ-400
    And deck 1 has hotcues stored in slots 3, 4, 5 and 6
    When I press Shift + Pad 3, Shift + Pad 4, Shift + Pad 5 and Shift + Pad 6 on the left deck
    And I press Pad 3, Pad 4, Pad 5 and Pad 6 on the left deck in HOT CUE mode
    Then each pad still jumps to its stored hotcue
    And no hotcue has been cleared

  Scenario: The documented pad layer still works alongside the new bindings
    Given "Use the Hot Cue pads as accessibility pads" is enabled in Preferences, Controllers, DDJ-400
    And the pads are in HOT CUE mode
    When I press Pad 1 on the left deck
    Then I hear deck 1's full status: playing or stopped, time remaining, BPM and pitch
    When I press Shift + Pad 1 on the left deck
    Then I hear "Deck 1 B P M halved"
    When I press Shift + Pad 2 on the left deck
    Then I hear "Deck 1 B P M doubled"

  # ---------------------------------------------------------------------
  # The four newly-announced controls — issue #54
  #
  # These are controls the DDJ-400 can change from the front panel that
  # previously gave a blind DJ no feedback at all.
  #
  # Two of them depend on a mapping claim that is stated in a code comment
  # but was not verified: that this fork remaps Shift+CUE to start_stop and
  # Shift+SYNC to rateRange. If those presses do nothing, check the mapping
  # before filing an announcement bug.
  # ---------------------------------------------------------------------

  @blocking
  Scenario: Cycling the tempo range is spoken immediately
    Given deck 1's tempo range is plus or minus 8 percent
    When I press Shift + SYNC on the left deck
    Then I hear "Deck 1 tempo range plus or minus 16 percent"
    And I hear it straight away, not after a pause
    When I press Shift + SYNC on the left deck again
    Then I hear the next range in the cycle announced the same way

  Scenario: The tempo range announcement lands before the next pitch readout
    # The reason this one is not debounced: the same fader position now
    # means a different BPM delta, so a stale pitch reading would mislead.
    Given deck 1's tempo range is plus or minus 8 percent
    And the pitch fader is off centre
    When I press Shift + SYNC on the left deck
    And I then nudge the pitch fader
    Then I hear the tempo range announcement before the pitch percentage

  @inference
  Scenario: Shift + CUE announces the jump back to the start
    # Depends on the unverified Shift+CUE -> start_stop remap.
    Given deck 1 is playing from somewhere in the middle of the track
    When I press Shift + CUE on the left deck
    Then I hear "Deck 1 back to start"
    And deck 1 is at the start of the track

  @blocking
  Scenario: Halving and doubling an active loop announces the new size
    # loop_scale changes the loop length without touching beatloop_size, so
    # the announcement tracks the size itself. Repeated presses must
    # compound correctly rather than repeating one number.
    Given deck 1 is playing with a 4-beat loop active
    When I press CUE/LOOP CALL left on the left deck
    Then I hear "Deck 1 loop size 2"
    When I press CUE/LOOP CALL left on the left deck again
    Then I hear "Deck 1 loop size 1"
    When I press CUE/LOOP CALL right on the left deck
    Then I hear "Deck 1 loop size 2"

  Scenario: Loop size announcements survive a fresh loop
    Given deck 1 is playing with no loop active
    When I set an 8-beat loop
    And I press CUE/LOOP CALL left on the left deck
    Then I hear "Deck 1 loop size 4"
    # If you hear a size that has nothing to do with 8, the seed from
    # loop_enabled/beatloop_size is not being picked up.

  Scenario: Moving effect focus announces the effect by name
    Given effect unit 1 has an effect named "Echo" in slot 1
    And "Announce effects" is enabled in Preferences, Accessibility
    When I press the BEAT FX right paddle to move focus onto that effect
    Then I hear "Unit 1: Echo focused"
    When I press the BEAT FX right paddle again
    Then I hear "Unit 1: " followed by the next effect's name and " focused"

  Scenario: Effect focus falls back to a slot number when the name is unknown
    Given effect unit 2 has an empty slot 3
    When I move the focus onto slot 3 of effect unit 2
    Then I hear "Unit 2: effect 3 focused"

  Scenario: Sweeping effect focus quickly does not flood the speech channel
    # focused_effect is debounced, unlike rateRange.
    When I sweep the BEAT FX paddle through four effects as fast as I can
    Then I hear one announcement naming the effect I landed on
    And I do not hear all four announced in sequence

  # ---------------------------------------------------------------------
  # Tempo fader high-resolution pair — issue #47's second half
  # ---------------------------------------------------------------------

  @inference @midimonitor
  Scenario: The tempo fader sends an MSB and an LSB
    # The mapping only applies a new rate when the LSB arrives; an MSB alone
    # is a silent no-op. The emulator was corrected to match this reading of
    # the hardware, which is itself unverified.
    Given Mixxx is closed
    And a MIDI monitor is running and receiving from the DDJ-400
    When I move the left deck's tempo fader slowly from one end to the other
    Then the monitor logs Control Change messages on controller "0x00"
    And it logs Control Change messages on controller "0x20"
    And each "0x00" message is followed by a "0x20" message
    And I write down whether the LSB really is sent in the run log

  Scenario: The tempo fader changes the deck's pitch smoothly
    Given deck 1 is playing
    When I move the left deck's tempo fader slowly from centre to one end
    Then the deck's speed changes smoothly as I move it
    And it does not jump in large steps or stick
    When I press Alt+1
    Then I hear a pitch value consistent with where I left the fader

  # ---------------------------------------------------------------------
  # Two decks on connect — carried over from issue #33
  # ---------------------------------------------------------------------

  @regression
  Scenario: Connecting the controller gives me two usable decks
    Given Mixxx is closed and the DDJ-400 is disconnected
    When I connect the DDJ-400 and start Mixxx
    And I press Alt+2
    Then I hear deck 2's status announced rather than silence

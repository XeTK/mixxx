@manual @audio
Feature: The audio path — things only a listener can confirm

  Unit tests can assert that a string was produced. They cannot assert that
  a human heard it. Everything in this file is about the difference: speech
  that was generated but flushed before it reached the speakers, earcons
  that are technically distinct but sound the same at gig volume, ducking
  that either does not duck or ducks so hard the mix disappears.

  All earcons in this fork are synthesised at runtime from sine grains, not
  loaded from sound files. There is nothing to install. The frequencies and
  durations are given below so a tester can describe what they actually
  heard rather than guessing at a name.

  Run these somewhere quiet, on headphones, with the mix at a realistic
  level rather than at zero. An earcon that is only distinguishable in
  silence has not passed.

  Background:
    Given Mixxx is running with a working audio device
    And speech is on
    And "Speak deck names as numbers" is enabled in Preferences, Accessibility
    And I am listening on headphones
    And the earcon volume is at its default

  # ---------------------------------------------------------------------
  # Speech batching — issue #48
  #
  # This is the highest-value scenario in the file. The bug it fixes was
  # invisible to every test that checked "was the right string generated",
  # because the string WAS generated — and then thrown away.
  #
  # The mechanism: TtsEngine::say() barges in on whatever is currently
  # speaking. It bumps a generation counter and flushes the audio sink, and
  # the synthesiser worker abandons any render whose generation is stale.
  # Loading a track spoke the load announcement, then — synchronously, in
  # the same call stack — Smart Cue moved the headphone cue, whose observer
  # called say() again. The load announcement had not been synthesised yet,
  # so it was discarded outright and the DJ heard only the cue message.
  #
  # The fix wraps the whole of slotNewTrackLoaded() in a speech batch: every
  # speak() inside appends to a list instead of dispatching, and the batch
  # end joins them with ". " and says the lot in one utterance. One
  # generation bump, nothing to barge in on.
  #
  # So the observable pass condition is not just "I heard both" — it is
  # "I heard both AS ONE CONTINUOUS UTTERANCE with no gap or cut".
  # ---------------------------------------------------------------------

  @blocking @timing
  Scenario: Loading a track with Smart Cue speaks the load AND the cue move
    Given "Smart cue" is enabled in Preferences, Decks
    And deck 2 is stopped and empty
    And deck 1 currently has the headphone cue
    And a track with a known artist, title, BPM and key is selected in the library
    When I press "Shift+Right" to load it into deck 2
    Then I hear the announcement begin with "Loaded deck, Bravo"
    And I hear the artist, then the title, then the BPM, then the key
    And I hear "headphone cue on" in the same breath, without a pause or a cut
    And the load announcement is not cut off part way through

  @blocking @timing
  Scenario: The load announcement is not lost when the cue moves away from a deck
    Given "Smart cue" is enabled in Preferences, Decks
    And deck 1 has the headphone cue and is stopped
    And deck 2 is stopped
    When I load a track into deck 2
    Then I hear the full load announcement for deck 2
    And I hear the cue moving to deck 2 announced
    And I hear a single utterance, not two utterances where the first is truncated

  @timing
  Scenario: Loading into a playing deck does not disturb the cue or the speech
    Given deck 1 is playing
    And "Loading a track, when deck is playing" is not set to Reject
    When I load a track into deck 1
    Then I hear the full load announcement
    And the headphone cue has not moved

  @timing
  Scenario: Load announcements survive back-to-back loads
    # Two loads in quick succession is a normal thing to do while digging.
    # The second load legitimately barges in on the first; what must not
    # happen is the second load's own announcement being eaten.
    When I load a track into deck 1
    And I immediately load a different track into deck 2
    Then I hear the deck 2 load announcement in full
    # The deck 1 announcement being cut short here is expected behaviour.

  @timing
  Scenario: The AccessMenu speaks one utterance per action, not two
    # #48 also merged the menu's two-utterance patterns into single strings,
    # for the same barge-in reason.
    When I open the AccessMenu
    Then I hear "Main menu." followed immediately by the current item, as one utterance
    When I navigate to a value item and enter value-edit mode
    Then I hear the item label, then "Turn to change, confirm to set, back to cancel.", then the current value
    And I hear all of it, with no part cut off
    When I change the value and confirm it
    Then I hear "Set." followed by the item and its new value, as one utterance
    When I enter value-edit mode again and cancel
    Then I hear "Cancelled." followed by the item and its unchanged value

  # ---------------------------------------------------------------------
  # Mixer readouts on by default — issue #36
  #
  # This was the single BLOCKING finding from the original accessibility
  # audit: a blind DJ moving a channel fader or an EQ knob heard nothing
  # at all unless they had already found and ticked "Announce mixer
  # controls" in Preferences, Accessibility — a setting they could not
  # discover without already being able to see the dialog.
  #
  # AnnounceMixer's default flipped from false to true; nothing else
  # about the readout changed. It covers volume, trim, the three EQ
  # bands, the filter (QuickEffect super knob) and the crossfader — every
  # one of those observers in AnnouncementManager checks the same
  # AnnounceMixer setting. It still names the control on first touch and
  # speaks the settled value once you stop moving it (debounced), and
  # "announce while moving" remains a separate, still-opt-in setting, so
  # this is not chatty mid-mix. Every scenario elsewhere in this plan that
  # exercises these controls (e.g. keyboard_only.feature's EQ/volume/
  # filter/crossfader scenarios) explicitly enables AnnounceMixer in its
  # own Background, so none of them actually prove the DEFAULT — this is
  # the only scenario in the plan that does.
  #
  # AnnounceTempo (the pitch/tempo fader) is a SEPARATE preference and was
  # already on by default before this PR; it is included below only as a
  # sanity check that raising AnnounceMixer's default did not disturb it,
  # not because #36 touched it.
  # ---------------------------------------------------------------------

  @blocking @firstrun
  Scenario: A fresh profile speaks EQ, filter, volume and the crossfader with nothing configured
    Given the Mixxx settings directory has been moved aside so this is a fresh profile
    And Mixxx is running with a working audio device
    And I have not opened Preferences, Accessibility at all
    And deck 1 is playing
    When I move deck 1's low EQ knob and let it settle
    Then I hear "Deck 1 E Q low" named on the first move
    And I hear a value announced once it settles
    When I move deck 1's filter knob and let it settle
    Then I hear "Deck 1 filter" named
    And I hear a value announced once it settles
    When I move deck 1's volume fader and let it settle
    Then I hear "Deck 1 volume" named
    And I hear a value announced once it settles
    When I move the crossfader and let it settle
    Then I hear "Crossfader" named
    And I hear a value announced once it settles
    When I move deck 1's pitch/tempo fader and let it settle
    Then I hear a pitch value announced
    # The tempo fader was already spoken by default before #36 (a
    # different setting, AnnounceTempo); included here only to confirm
    # the mixer change did not silence it as a side effect.

  Scenario: Mixer readouts can still be turned off by someone who wants quiet
    Given "Announce mixer controls" is disabled in Preferences, Accessibility
    And deck 1 is playing
    When I move deck 1's volume fader and let it settle
    Then I hear nothing

  # ---------------------------------------------------------------------
  # Musical key names respect the locale — issue #14
  #
  # keyForSpeech() in AnnouncementManager now wraps its 24 fully-spelled
  # key names ("C Major", "F Sharp Major", ... "B Minor") in tr(), so a
  # translated build speaks the key in the user's language instead of
  # always English. This only affects Traditional notation (and the
  # "...and Traditional" variants); Open Key and Lancelot/Camelot codes
  # are spoken as a digit plus a phonetic letter and were not touched.
  #
  # CORRECTION AT TIME OF WRITING: none of the shipped translation files
  # (res/translations/mixxx_*.ts) contain a translated msgid for any of
  # these 24 strings — they are new to tr() as of this commit and have
  # not yet been through a Transifex sync. So switching the locale today
  # will NOT produce a translated key name; Qt's tr() falls back to the
  # English source string when no translation exists. What these
  # scenarios can actually prove until a translation lands is that
  # switching locale does not break the announcement — not that a
  # translated word comes out. Re-run them and update the expected
  # string once a translation is available.
  #
  # The locale is set from Preferences, Interface ("Locale" combo box,
  # default "System") and is only read at startup, so it needs a
  # restart to take effect.
  # ---------------------------------------------------------------------

  @locale
  Scenario: The on-demand key readout follows the app locale
    Given "Locale" in Preferences, Interface is set to a non-English language with a Mixxx translation installed
    And I have restarted Mixxx so the new locale is loaded
    And "Key Notation" in Preferences, Key Detection is set to Traditional
    And a track with a known musical key is loaded in deck 1
    When I press "Alt+7"
    Then I hear the key name in that language, not in English
    # If no translation exists yet for the key names (see the correction
    # above), you will hear the English name instead — that is the
    # current expected state, not a fail. Record which one you heard.

  @locale
  Scenario: The load announcement's key name also follows the locale
    Given "Locale" in Preferences, Interface is set to a non-English language with a Mixxx translation installed
    And I have restarted Mixxx so the new locale is loaded
    And "Key Notation" in Preferences, Key Detection is set to Traditional
    When I load a track with a known musical key into deck 1
    Then I hear the key name spoken in the same language as the rest of the load announcement
    # Same correction as above applies.

  @locale @regression
  Scenario: A locale with no Mixxx translation at all still falls back safely
    Given "Locale" in Preferences, Interface is set to a language that has no Mixxx translation installed
    And I have restarted Mixxx
    And a track with a known musical key is loaded in deck 1
    When I press "Alt+7"
    Then I still hear a recognisable key name, spoken in English
    And Mixxx does not crash and does not announce an empty string

  # ---------------------------------------------------------------------
  # Earcons — the full set, including the new xrun tone from issue #87
  #
  # Reference for describing what you heard:
  #   Play        587 Hz then 880 Hz     rising pair
  #   Stop        880 Hz then 587 Hz     falling pair
  #   EndOfTrack  1175 Hz x3             three high pips, ~185 ms total
  #   CueOn       784 Hz                 one tone
  #   CueOff      523 Hz                 one lower tone
  #   Restart     659 Hz x2, 45 ms apart double tap
  #   LoopOn      698 -> 1047 Hz         rising pair
  #   LoopOff     1047 -> 698 Hz         falling pair
  #   Clipping    350 Hz x2, 60 ms each  low double buzz
  #   CuePreview  988 Hz, 40 ms          one very short high tick
  #   Xrun        196 Hz x3, 90 ms each  low harsh triple buzz, ~310 ms  [NEW]
  # ---------------------------------------------------------------------

  @blocking
  Scenario: The xrun earcon cannot be mistaken for the clipping earcon
    # This distinction carries real meaning mid-set: clipping means "turn
    # the gain down", an xrun means "the engine glitched and there is
    # nothing you can do about it from the mixer". Confusing them makes a
    # DJ pull gain down for no reason.
    Given "Announce audio clipping" is enabled with feedback style "Sounds and speech"
    When I deliberately clip deck 1 by pushing its gain well past full
    Then I hear a low double buzz
    And I hear "Deck 1 clipping"
    When I force an audio dropout by setting the audio buffer very small and loading the CPU
    Then I hear a lower, longer, three-part buzz that is clearly not the clipping sound
    And I hear "Audio dropout"
    And I could tell the two apart without being told which was which

  Scenario: The xrun announcement obeys the clipping preference
    # There is deliberately no separate xrun setting; it rides on clipping's
    # checkbox and feedback style. Confirm that, so nobody files it as a
    # missing preference.
    Given "Announce audio clipping" is disabled
    When I force an audio dropout
    Then I hear nothing at all

  Scenario Outline: The clipping feedback style applies to the xrun tone too
    Given "Announce audio clipping" is enabled with feedback style "<style>"
    When I force an audio dropout
    Then I hear "<result>"

    Examples:
      | style            | result                                        |
      | Speech           | only the words "Audio dropout", with no tone  |
      | Sounds           | only the low triple buzz, with no words       |
      | Sounds and speech| both the low triple buzz and "Audio dropout"  |

  @timing
  Scenario: Repeated dropouts do not turn into a continuous alarm
    # Throttled to one announcement per 5 seconds.
    When I force a sustained run of audio dropouts lasting 20 seconds
    Then I hear the dropout announcement roughly every 5 seconds
    And I do not hear it repeating continuously

  # ---------------------------------------------------------------------
  # Per-channel clipping — issue #87
  # ---------------------------------------------------------------------

  @blocking
  Scenario: Channel clipping is panned to the deck that is clipping
    # The main-bus warning is centred and unnamed; a channel warning names
    # its deck and is panned to it. Both cues carry the same information,
    # which is the point: you can act on the pan before the words arrive.
    Given both decks are playing
    When I clip deck 1 only
    Then I hear the clipping buzz in my left ear
    And I hear "Deck 1 clipping"
    When I clip deck 2 only
    Then I hear the clipping buzz in my right ear
    And I hear "Deck 2 clipping"

  Scenario: Main output clipping is still centred and unnamed
    When I clip the main output without clipping either channel
    Then I hear the clipping buzz centred in both ears
    And I hear "Clipping" with no deck name

  @timing
  Scenario: One deck clipping does not silence the other deck's warning
    # The throttle is per-deck by design, precisely so a channel that is
    # constantly clipping cannot mask the one that just started.
    Given deck 1 has just been announced as clipping
    When I clip deck 2 within the next 2 seconds
    Then I hear "Deck 2 clipping"
    And it is not suppressed by deck 1's recent warning

  # ---------------------------------------------------------------------
  # Speech routing, ducking and the beat click
  # ---------------------------------------------------------------------

  @blocking @twodevices
  Scenario: Speech goes to the headphones and not to the audience
    Given a separate main output and headphone output are configured
    And "Speech output" in Preferences, Accessibility is set to headphones
    And both decks are playing into the main output
    When I press "Alt+1"
    Then I hear deck 1's status in the headphones
    And nothing is audible on the main output but the music

  @twodevices
  Scenario: Speech can be routed to the main output when that is what I want
    Given "Speech output" in Preferences, Accessibility is set to main output
    When I press "Alt+1"
    Then I hear deck 1's status on the main output

  @twodevices
  Scenario: The beat click follows the speech route
    Given "Speech output" is set to headphones
    When I press "Alt+B"
    Then I hear "Beat click on" in the headphones
    And I hear the click in the headphones only
    And the click is not audible on the main output

  Scenario: The beat click pans by deck and marks the bar
    Given both decks are playing tracks with correct beat grids
    When I press "Alt+B"
    Then I hear "Beat click on"
    And I hear deck 1's click in my left ear
    And I hear deck 2's click in my right ear
    And every fourth click on each deck is noticeably higher pitched
    When I press "Alt+B"
    Then I hear "Beat click off"
    And the clicks stop

  Scenario: The beat click volume slider actually changes the click level
    Given the beat click is on and the mix is at gig level
    When I raise "Beat click volume" in Preferences, Accessibility
    Then the clicks get louder relative to the music
    And I can still pick them out over a loud mix

  Scenario Outline: Ducking lowers the music enough to hear speech, but not too far
    Given both decks are playing at a realistic level
    And "Music ducking during announcements" is set to "<setting>"
    When I press "Alt+1"
    Then the music drops "<amount>" while the announcement plays
    And the music returns to its previous level when the announcement finishes
    And I can make out every word of the announcement

    Examples:
      | setting | amount               |
      | off     | not at all           |
      | light   | a little             |
      | heavy   | a lot but not to silence |

  Scenario: Ducking recovers cleanly after a barged-in announcement
    # If a second announcement interrupts the first, the duck envelope must
    # still release. A stuck duck means a quiet mix for the rest of the set.
    When I press "Alt+1" and then immediately press "Alt+2" before the first finishes
    Then I hear deck 2's status
    And the music returns to full level once it finishes
    And the music is not left quiet

  # ---------------------------------------------------------------------
  # Split cue
  # ---------------------------------------------------------------------

  Scenario: Split cue puts one deck in each ear
    Given both decks are playing and both have the headphone cue on
    When I press "Alt+S"
    Then I hear "Split cue on. Deck 1 left, deck 2 right"
    And I hear only deck 1 in my left ear
    And I hear only deck 2 in my right ear
    When I press "Alt+S"
    Then I hear "Split cue off"
    And both decks are back in both ears

  Scenario: The headphone mix knob still works in split mode
    Given split cue is on
    When I turn the headphone mix knob towards main
    Then I hear the main mix blended into both ears
    And each deck is still dominant in its own ear

  Scenario: Beat click and split cue together give one deck and one grid per ear
    Given both decks are playing
    When I press "Alt+S"
    And I press "Alt+B"
    Then my left ear carries deck 1's audio and deck 1's click
    And my right ear carries deck 2's audio and deck 2's click
    And I can beatmatch by ear from that alone

  # ---------------------------------------------------------------------
  # Talkover and eject — issue #87
  # ---------------------------------------------------------------------

  Scenario: The microphone toggle is always spoken
    # No settings gate and no throttle on this one, deliberately: it is a
    # direct user action with live consequences.
    When I press "`"
    Then I hear "Microphone on"
    When I press "`"
    Then I hear "Microphone off"

  Scenario: The microphone toggle is spoken even with announcements turned down
    Given every announcement category checkbox in Preferences, Accessibility is unticked
    When I press "`"
    Then I still hear "Microphone on"

  @blocking
  Scenario: Ejecting a playing deck is refused out loud
    # BaseTrackPlayerImpl silently no-ops an eject on a playing deck, so
    # before this the key press did nothing and said nothing.
    Given deck 1 is playing
    When I press "Alt+Shift+Left"
    Then I hear "Deck 1 is playing, eject blocked. Stop the deck first."
    And deck 1 is still playing the same track

  Scenario: The eject refusal is spoken even with track-load announcements off
    Given "Announce track loads" is disabled in Preferences, Accessibility
    And deck 1 is playing
    When I press "Alt+Shift+Left"
    Then I hear "Deck 1 is playing, eject blocked. Stop the deck first."

  Scenario: Ejecting a stopped deck is confirmed
    Given deck 1 has a track loaded and is stopped
    When I press "Alt+Shift+Left"
    Then I hear "Deck 1 track ejected"
    When I press "Alt+1"
    Then I hear that deck 1 has no track loaded

  Scenario: Ejecting an empty deck says nothing
    Given deck 1 has no track loaded
    When I press "Alt+Shift+Left"
    Then I hear nothing

  Scenario: Double-pressing eject reloads and announces confusingly
    # KNOWN EDGE CASE, documented in the source, not a defect to file.
    # A rapid second press within the double-click window restores the
    # previous track instead of ejecting, but the announcement still says
    # "ejected" before the load announcement arrives. Confirm it behaves
    # as described so the wording can be improved deliberately later.
    Given deck 1 has a track loaded and is stopped
    When I press "Alt+Shift+Left" twice in quick succession
    Then I hear "Deck 1 track ejected"
    And I then hear a load announcement for the same track

  # ---------------------------------------------------------------------
  # Sampler speech — issue #75
  # ---------------------------------------------------------------------

  Scenario: Loading a sampler is announced with its number and the track
    When I load a track with a known artist, title, BPM and key into sampler 3
    Then I hear "Sampler 3 loaded" followed by the artist, the title, the BPM and the key

  Scenario: A sampler with no metadata announces only what it has
    When I load a track with no artist, title, BPM or key into sampler 1
    Then I hear "Sampler 1 loaded"
    And I do not hear "B P M" or "Key"

  Scenario Outline: Sampler transport is announced
    Given sampler 2 has a track loaded
    When I "<action>" sampler 2
    Then I hear "<heard>"

    Examples:
      | action | heard             |
      | play   | Sampler 2 playing |
      | stop   | Sampler 2 stopped |
      | eject  | Sampler 2 ejected |

  Scenario: Ejecting a playing sampler stays silent
    # Eject only speaks when the sampler has a track and is not playing,
    # because that is the only case the engine actually acts on.
    Given sampler 2 is playing
    When I eject sampler 2
    Then I hear nothing about sampler 2 being ejected

  Scenario: Samplers do not steal the headphone cue
    Given "Smart cue" is enabled in Preferences, Decks
    And deck 1 has the headphone cue
    When I load a track into sampler 4
    Then I hear "Sampler 4 loaded" and the track details
    And I do not hear any headphone cue announcement
    And deck 1 still has the headphone cue

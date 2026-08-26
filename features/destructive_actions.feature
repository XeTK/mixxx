@manual @destructive
Feature: Confirmations before anything is destroyed

  USE A THROWAWAY LIBRARY. Several scenarios here will move real files to
  the trash if they pass, and will delete them irrecoverably if they fail in
  the wrong direction. Copy a dozen tracks into a scratch folder, point a
  fresh Mixxx profile at it, and run these there.

  Issue #74 is the work under test. Purge previously had no confirmation at
  all — a keystroke removed tracks from the library with nothing said and
  nothing asked. Hide, Remove and Delete had confirmations but a blind user
  had no way to know which button was focused, so pressing Enter on reflex
  was a coin flip.

  Two properties matter and both need checking separately:

    1. I am TOLD what is about to happen, in enough detail to decide.
    2. The SAFE option is what happens if I do the reflexive thing —
       press Enter, or press Escape, without moving focus.

  Property 2 is the one that actually protects data. Test it deliberately
  and honestly: press Enter without moving, and see what happens.

  Background:
    Given Mixxx is running against a throwaway library
    And speech is on
    And a screen reader is running
    And the library track table has focus

  # ---------------------------------------------------------------------
  # Purge — the one that had no confirmation before
  # ---------------------------------------------------------------------

  @blocking
  Scenario: Purging a single track asks first and says what it means
    Given exactly one track is selected in the library
    When I choose Purge from the track context menu
    Then I hear "Purge tracks dialog. Permanently remove 1 track from the library? This only removes the library entry, it does not delete the file from disk. No is selected by default; press Escape or Enter for no, or move to Yes and press Enter to purge."
    And my screen reader announces a dialog titled "Confirm Purge"
    And my screen reader announces that the focused button is "No"

  @blocking
  Scenario: Pressing Enter on the purge dialog does not purge
    # The single most important scenario in this file.
    Given several tracks are selected in the library
    And the purge confirmation dialog has just opened
    When I press "Return" without moving focus
    Then the dialog closes
    And all the tracks are still in the library
    And no files have been removed from disk

  @blocking
  Scenario: Pressing Escape on the purge dialog does not purge
    Given several tracks are selected in the library
    And the purge confirmation dialog has just opened
    When I press "Escape"
    Then the dialog closes
    And all the tracks are still in the library

  Scenario: Deliberately confirming a purge does purge
    Given exactly one track is selected in the library
    And I know the file's path on disk
    When I open the purge confirmation dialog
    And I move focus to "Yes" and press "Return"
    Then the track is no longer in the library
    And the file is still present on disk

  Scenario: Purging several tracks says how many
    Given 5 tracks are selected in the library
    When I choose Purge from the track context menu
    Then I hear the announcement say "Permanently remove 5 tracks from the library?"

  Scenario: The purge wording distinguishes library removal from file deletion
    # A blind user cannot skim the dialog for reassurance. The distinction
    # has to be in the spoken sentence, and it is the whole reason purge is
    # less frightening than delete.
    Given a track is selected in the library
    When I open the purge confirmation dialog
    Then the spoken text says the file is not deleted from disk
    And I could tell purge apart from delete on the announcement alone

  Scenario: Purging from the library table and from the track menu behave the same
    Given a track is selected in the library
    When I purge it using the track context menu
    Then I hear the purge confirmation announcement
    When I select another track and purge it using the library table's own purge path
    Then I hear the same purge confirmation announcement

  # ---------------------------------------------------------------------
  # Hide
  # ---------------------------------------------------------------------

  @blocking
  Scenario: Hiding tracks is narrated before the dialog appears
    Given 3 tracks are selected in the library
    When I choose Hide from Library from the track context menu
    Then I hear "Confirm track hide dialog. Are you sure you want to hide the selected 3 tracks? No is selected by default; press Escape or Enter for no, or move to Yes and press Enter to confirm."

  @blocking
  Scenario: Pressing Enter on the hide dialog does not hide
    Given the hide confirmation dialog has just opened for 3 tracks
    When I press "Return" without moving focus
    Then all 3 tracks are still visible in the library

  Scenario: The spoken button names do not match the on-screen buttons
    # KNOWN MISMATCH, and worth an explicit result rather than a shrug.
    # The narration says "Yes" and "No"; the dialog that actually appears
    # uses "Ok" and "Cancel", with Cancel focused. The safe-default
    # behaviour is correct either way, but a screen-reader user is told to
    # look for a button that is not there.
    Given the hide confirmation dialog has just opened
    When my screen reader announces the focused button
    Then I record the button name it announces
    And I compare it with the "No" the spoken narration told me to expect
    # Expected today: the screen reader says "Cancel". Log it so the
    # wording can be reconciled in one direction or the other.

  Scenario: Deliberately confirming a hide does hide
    Given 2 tracks are selected in the library
    When I open the hide confirmation dialog
    And I move focus to the accept button and press "Return"
    Then those tracks are no longer listed in the library
    And I can find them again in Hidden Tracks

  Scenario: The session suppression checkbox is reachable and announced
    Given the hide confirmation dialog has just opened
    When I tab to the checkbox
    Then my screen reader announces "Don't ask again during this session"
    And I can tick it with "Space"

  Scenario: Hiding tracks that are in playlists warns separately
    # This second dialog is NOT narrated by Mixxx — the screen reader is
    # the only thing that will read it. Its default button was changed to
    # Cancel as part of #74, which is the property to check.
    Given a track that belongs to at least one playlist is selected
    When I confirm the hide
    Then my screen reader announces a dialog titled "Hiding tracks"
    And my screen reader announces that the focused button is "Cancel"
    When I press "Return" without moving focus
    Then the track is still in its playlists
    And the track is still visible in the library

  # ---------------------------------------------------------------------
  # Remove — from AutoDJ, a crate, or a playlist
  # ---------------------------------------------------------------------

  @blocking
  Scenario Outline: Removing tracks names the place they are being removed from
    # The spoken text is the only way to tell these three apart. Getting
    # the wrong one means removing a track from the wrong container.
    Given the "<context>" view is showing with 2 tracks selected
    When I choose Remove from the track context menu
    Then I hear "Confirm track removal dialog. Are you sure you want to remove the selected 2 tracks <from>? No is selected by default; press Escape or Enter for no, or move to Yes and press Enter to confirm."

    Examples:
      | context  | from                     |
      | AutoDJ   | from the AutoDJ queue    |
      | crate    | from this crate          |
      | playlist | from this playlist       |

  @blocking
  Scenario: Pressing Enter on a remove dialog does not remove
    Given a playlist is showing with 2 tracks selected
    And the remove confirmation dialog has just opened
    When I press "Return" without moving focus
    Then both tracks are still in the playlist

  Scenario: Removing from a crate does not touch the library
    Given a crate is showing with 1 track selected
    When I open the remove confirmation dialog and confirm it deliberately
    Then the track is no longer in that crate
    And the track is still in the main library
    And the file is still on disk

  Scenario: Singular and plural are both correct
    Given a playlist is showing with exactly 1 track selected
    When I choose Remove
    Then I hear "remove the selected 1 track from this playlist"
    And I do not hear "1 tracks"

  # ---------------------------------------------------------------------
  # Delete from disk — the irreversible one
  # ---------------------------------------------------------------------

  @blocking
  Scenario: Deleting track files is narrated with the word trash
    Given 2 tracks are selected in the library
    When I choose Delete Track Files from the track context menu
    Then I hear "Move track files to trash dialog. Move 2 track files to the trash bin? Cancel is selected by default; press Escape or Enter to cancel, or move to Okay and press Enter to continue."
    # On a build against Qt older than 5.15 the wording is different and
    # the operation is a permanent delete rather than a move to trash:
    # "Delete track files dialog. Permanently delete 2 track files from
    # disk? This can not be undone. Cancel is selected by default; press
    # Escape or Enter to cancel, or move to Delete Files and press Enter
    # to delete." Record which wording you heard.

  @blocking
  Scenario: Pressing Enter on the delete dialog does not delete
    Given 2 tracks are selected in the library
    And the delete confirmation dialog has just opened
    When I press "Return" without moving focus
    Then both files are still on disk
    And both tracks are still in the library

  @blocking
  Scenario: Pressing Escape on the delete dialog does not delete
    Given 2 tracks are selected in the library
    And the delete confirmation dialog has just opened
    When I press "Escape"
    Then both files are still on disk

  Scenario: The screen reader agrees with the narration about the safe button
    Given the delete confirmation dialog has just opened
    Then my screen reader announces that the focused button is "Cancel"
    And that matches the "Cancel" the spoken narration told me to expect

  Scenario: Deliberately confirming a delete moves the files to the trash
    Given 1 track is selected and I know its path on disk
    When I open the delete confirmation dialog
    And I move focus to the accept button and press "Return"
    Then the file is no longer at that path
    And the file is in the system trash, recoverable
    And the track is no longer in the library

  Scenario: Deleting a track that is loaded in a deck stops the deck first
    Given a track is loaded and playing in deck 1
    And that same track is selected in the library
    When I delete it and confirm deliberately
    Then deck 1 stops
    And the track is ejected from deck 1
    And I hear the deck stopping

  Scenario: The same file selected twice is only counted once
    Given a selection that includes the same file through two different playlist entries
    When I choose Delete Track Files
    Then I hear a count of 1 track file, not 2

  # ---------------------------------------------------------------------
  # Cross-cutting: nothing destructive should be reachable by accident
  # ---------------------------------------------------------------------

  @blocking
  Scenario: No destructive action fires without a confirmation
    Given a track is selected in the library
    When I trigger each of Purge, Hide, Remove and Delete Track Files in turn
    Then each one opens a confirmation I can hear before anything changes
    And none of them acts immediately

  Scenario: A confirmation I cancel leaves the selection intact
    Given 4 tracks are selected in the library
    When I open the purge confirmation and cancel it
    Then the same 4 tracks are still selected
    And I can immediately do something else with them

  Scenario: Repeated Enter presses on a stack of dialogs stay safe
    # The realistic accident: a key press queued while a dialog was
    # opening, or a stuck key. Every layer must default safe.
    Given a track that belongs to a playlist is selected
    When I trigger a hide and then press "Return" three times in quick succession
    Then the track is still visible in the library
    And it is still in its playlist

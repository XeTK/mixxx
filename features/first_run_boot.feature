@manual @firstrun
Feature: Cold start, first run, and device errors

  Startup is where a blind user is most exposed. There is no speech until
  the audio engine exists, the screen reader is the only thing talking, and
  if the sound device does not open there is no obvious way to find that out
  without sight.

  Two changes are under test here. Issue #69 stops "Mixxx ready" being
  spoken into an audio engine that cannot render it — it is now held until
  SoundManager confirms the devices actually opened. Issue #70 makes error
  dialogs speak their own title and message instead of relying entirely on
  a screen reader that may not be running.

  Every scenario tagged @firstrun needs a clean settings directory. Move
  your real one aside first — see features/README.md. Testing "first run"
  against an existing profile proves nothing, because the thing being
  tested is what happens when none of the config exists yet.

  Background:
    Given a build of this fork installed and launchable

  # ---------------------------------------------------------------------
  # "Mixxx ready" timing — issue #69
  #
  # The ordering that made this necessary: loadConfiguredSkin() runs, and
  # with it the skin-loaded announcement, BEFORE the setupDevices() retry
  # loop in MixxxMainWindow::initialize(). Speech in this fork is mixed
  # into Mixxx's own engine output, so a "Mixxx ready" spoken at skin-load
  # time had nothing to render into and was simply never heard.
  #
  # It is now queued and flushed on SoundManager::devicesSetup, which only
  # fires on the success path.
  # ---------------------------------------------------------------------

  @blocking
  Scenario: Mixxx says it is ready, once, and I can hear it
    Given a working sound device is configured
    When I launch Mixxx
    Then I hear "Mixxx ready"
    And I hear it clearly and from the beginning, not clipped at the front
    And I hear it exactly once

  @blocking @timing
  Scenario: Mixxx ready arrives after the audio device is open, not before
    # The observable consequence: the announcement should come late enough
    # that other speech works immediately afterwards. If "Mixxx ready" is
    # audible but the next announcement is silent, the gating is wrong in
    # the other direction.
    Given a working sound device is configured
    When I launch Mixxx
    And I hear "Mixxx ready"
    And I immediately press "Alt+1"
    Then I hear deck 1's status announced
    And that announcement is also audible from the beginning

  Scenario: Mixxx ready is not spoken when the setting is off
    Given "Announce Mixxx ready at startup" is disabled in Preferences, Accessibility
    When I restart Mixxx
    Then I do not hear "Mixxx ready" at any point
    When I press "Alt+1"
    Then I hear deck 1's status, so speech is working

  Scenario: Mixxx ready is not repeated when sound hardware is reconfigured
    Given Mixxx is running and has already said "Mixxx ready"
    When I open Preferences, Sound Hardware and press Apply without changing anything
    Then I do not hear "Mixxx ready" again

  Scenario: Reloading the skin while running says ready again
    # Once the engine is up, the announcement fires immediately rather than
    # being queued, so a skin reload genuinely re-announces. Confirm that is
    # what happens rather than silence.
    Given Mixxx is running with a working sound device
    When I reload the skin
    Then I hear "Mixxx ready"

  @blocking
  Scenario: A sound device that never opens does not falsely claim readiness
    # KNOWN AND INTENDED: with no device, "Mixxx ready" is never spoken —
    # there is no timeout and no fallback, because there is no audio path
    # to speak it through. The failure must be discoverable another way.
    Given the configured sound device is unavailable, for example unplugged or claimed by another application
    When I launch Mixxx
    Then I do not hear "Mixxx ready"
    And my screen reader announces the sound device error dialog
    And I can reach and operate that dialog's buttons with the keyboard alone

  Scenario: Fixing the device afterwards releases the queued announcement
    Given Mixxx launched with no working sound device and did not say it was ready
    When I open Preferences, Sound Hardware, select a working device and press OK
    Then I hear "Mixxx ready"
    And speech works from that point on

  # ---------------------------------------------------------------------
  # The genuinely fresh install
  # ---------------------------------------------------------------------

  @blocking
  Scenario: First launch with no configuration at all can be completed without sight
    # KNOWN LIMITATION, documented in ACCESSIBILITY_GUIDE.md: the first-run
    # sound hardware dialog appears before Mixxx's speech can produce audio,
    # so the screen reader is the only thing talking here. What is being
    # tested is whether that is SUFFICIENT — whether a blind user can get
    # from a fresh install to a working setup unaided.
    Given the Mixxx settings directory has been moved aside
    And a screen reader is running
    When I launch Mixxx
    Then my screen reader announces the first-run dialog
    And I can choose a music directory using only the keyboard
    And I can choose an output device using only the keyboard
    And every control I need is announced with a name that tells me what it does
    And I reach a running Mixxx without needing anyone to read the screen to me
    And I then hear "Mixxx ready"

  Scenario: The library scan on first run announces itself
    Given a fresh profile and a music folder containing at least 200 tracks
    When I complete first-run setup and the library scan begins
    Then I hear "Scanning library"
    And I hear it once, not repeatedly as the scan progresses

  Scenario: A later rescan announces again
    Given Mixxx has already completed one library scan this session
    When I trigger a rescan from the Library menu
    Then I hear "Scanning library"

  Scenario: Speech defaults are usable without configuration
    # A fresh install with nothing configured is the state a new blind user
    # meets first. If they have to configure speech before they can hear
    # anything, the fork has failed at step one.
    Given a fresh profile
    When I complete first-run setup
    And I arrow through the library track list
    Then I hear tracks announced without having enabled anything
    When I press "Alt+1"
    Then I hear deck 1's status
    And the default voice and rate are intelligible

  # ---------------------------------------------------------------------
  # Error dialogs speaking for themselves — issue #70
  #
  # ErrorDialogHandler now speaks "<title>. <message>" through Mixxx's own
  # speech before the dialog is shown. The message is HTML-stripped,
  # whitespace-collapsed, and truncated at 300 characters with an ellipsis.
  #
  # Two things it deliberately does NOT speak: the "Show Details" section,
  # and the informative-text block. That matters for the controller mapping
  # error in particular, which puts most of its useful content in the
  # informative text — so expect title plus one sentence and no more.
  #
  # Dialogs raised before CoreServices::initialize() wires the connection
  # are not spoken at all.
  # ---------------------------------------------------------------------

  @blocking
  Scenario: A broadcasting connection error speaks itself
    Given a Live Broadcasting profile pointing at a server that will refuse the connection
    When I enable Live Broadcasting
    Then I hear "Connection error." followed by the connection's error text
    And I hear which profile it refers to
    And I can dismiss the dialog with the keyboard alone

  Scenario: A recording disk-space warning speaks itself
    Given the recording folder is on a volume with less than 1 GiB free
    When I start recording
    Then I hear "Low Disk Space Warning. There is less than 1 GiB of usable space in the recording folder"

  Scenario: A recording file failure speaks itself
    Given the recording folder is set to a location I cannot write to
    When I start recording
    Then I hear "Recording." followed by the message about not being able to create the audio file
    And the spoken text mentions free disk space and write permission

  Scenario: A controller mapping error speaks itself
    Given a controller mapping containing a deliberate script error
    When I enable that controller in Preferences, Controllers
    Then I hear "Controller Mapping Error." followed by the name of the controller and that its mapping is not working properly
    # EXPECTED GAP: the detail of what is wrong lives in the dialog's
    # informative text, which is not spoken. You will need the screen
    # reader or the log for the actual script error.

  Scenario: A long error message is truncated rather than read forever
    Given an error whose message text is longer than 300 characters
    When the error dialog is raised
    Then I hear roughly the first 300 characters
    And I hear it end in a trailing-off rather than stopping mid-word with no signal

  Scenario: Markup in an error message is not read out as markup
    Given an error whose message contains HTML formatting
    When the error dialog is raised
    Then I hear the plain words
    And I do not hear tag names or angle brackets read aloud

  @regression
  Scenario: Error speech does not replace the screen reader's own reading
    Given a screen reader is running
    When an error dialog is raised
    Then I hear Mixxx speak the title and message
    And my screen reader also announces the dialog and its buttons
    And I can tell which button has focus before I press Enter

  Scenario: Errors during startup, before speech is wired up
    # EXPECTED BEHAVIOUR, not a defect: a dialog raised before
    # CoreServices::initialize() connects the handler will not be spoken by
    # Mixxx. The screen reader is the fallback. Confirm the fallback works.
    Given a configuration that will fail during early startup
    And a screen reader is running
    When I launch Mixxx
    Then my screen reader announces the dialog
    And I can act on it with the keyboard alone

  # ---------------------------------------------------------------------
  # Getting back out
  # ---------------------------------------------------------------------

  Scenario: Quitting works from the keyboard and does not hang
    Given Mixxx is running with both decks playing
    When I press "Ctrl+Q"
    Then Mixxx exits
    And it does not leave a dialog I cannot hear

  Scenario: Settings survive a restart
    Given I have changed the speech rate and enabled the beat click
    When I quit and relaunch Mixxx
    Then I hear "Mixxx ready" at my chosen speech rate
    When I press "Alt+1"
    Then the announcement is at the same rate

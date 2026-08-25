@manual
Feature: Library navigation, menus and the YouTube source

  Menus were the last large silent surface. A context menu is a list of
  things you cannot see, opened over a widget you cannot see, and until
  issue #80 arrowing through one told you nothing at all.

  This file also covers the YouTube library source from issue #85 — the
  only place in the fork where a long-running network operation reports
  progress by ear — and the library scanner dialog from issue #88.

  Some scenarios need a screen reader and say so. The menu-hover narration
  itself is Mixxx's own speech, so most of this file can be run without
  one.

  Background:
    Given Mixxx is running with a library containing at least 40 tracks
    And speech is on
    And "Speak deck names as numbers" is enabled in Preferences, Accessibility
    And at least two crates and two playlists exist

  # ---------------------------------------------------------------------
  # Track menu narration — issue #80
  #
  # The spoken text is the menu item's own label. Items with dynamic names
  # carry the raw name separately so that a crate called "R&B" is spoken as
  # "R and B" rather than having its ampersand mangled into a mnemonic.
  #
  # Hover does not bubble from submenus to their parent, so every submenu
  # had to be wired up individually. That is the failure mode to hunt for:
  # a submenu somewhere that stayed silent because it was missed.
  # ---------------------------------------------------------------------

  @blocking
  Scenario: Arrowing through the track context menu speaks every item
    Given a track is selected in the library
    When I open the track context menu
    And I arrow down through every item to the bottom
    Then I hear each item announced as I land on it
    And no item passes in silence

  @blocking
  Scenario Outline: Every submenu of the track menu narrates its contents
    # Walk into each one and back out. A silent submenu means a missed
    # connection, and there were many to make.
    Given a track is selected in the library
    And the track context menu is open
    When I arrow to "<submenu>" and open it
    And I arrow through its items
    Then I hear each item announced

    Examples:
      | submenu                       |
      | Load to                       |
      | Deck                          |
      | Sampler                       |
      | Add to Playlist               |
      | Crates                        |
      | Metadata                      |
      | Update external collections   |
      | Cover Art                     |
      | Adjust BPM                    |
      | Select Color                  |
      | Hotcues                       |
      | Clear                         |
      | Analyze                       |

  Scenario: The find-on-web submenu and its per-service submenus narrate
    Given the track context menu is open
    When I arrow to the find on web submenu and open it
    Then I hear each service announced
    When I open one of the service submenus
    Then I hear each item inside it announced

  Scenario: The search-related-tracks submenu narrates
    Given the track context menu is open
    When I arrow to the search related tracks submenu and open it
    Then I hear each item announced

  Scenario: Playlist and crate names are spoken as written
    # These carry their raw name separately so mnemonic markers do not
    # corrupt them. A crate named with an ampersand is the test case.
    Given a crate exists named "Drum & Bass"
    And a playlist exists named "Warm & Up"
    And a track is selected in the library
    When I open the track context menu and enter the Crates submenu
    Then I hear "Drum and Bass" spoken as a normal name
    And I do not hear a doubled ampersand or a mangled name
    When I go back and enter the Add to Playlist submenu
    Then I hear "Warm and Up" spoken as a normal name

  Scenario: The sampler submenus narrate, including the paged ones
    Given the track context menu is open
    When I arrow into the Sampler submenu
    Then I hear each sampler entry announced
    When I open one of the paged sampler submenus
    Then I hear each entry inside it announced

  Scenario: The same item hovered twice is not repeated
    # Deliberate deduplication — arrowing off an item and back onto it via
    # a mouse jiggle should not stutter.
    Given the track context menu is open
    When I land on an item and stay there
    Then I hear it announced once
    And I do not hear it repeated while focus stays put

  Scenario: Menu items are not given a position in the list
    # Unlike the spoken crate and playlist pickers, menu hover deliberately
    # does not append "N of M". Confirm, so nobody adds it by accident and
    # nobody files its absence.
    Given the track context menu is open
    When I arrow through several items
    Then I hear each item's name alone
    And I do not hear "1 of" or any position count

  @blocking
  Scenario: The sidebar playlist context menu narrates
    Given focus is on a playlist in the sidebar
    When I open its context menu
    And I arrow through the items
    Then I hear each item announced

  @blocking
  Scenario: The sidebar crate context menu narrates
    Given focus is on a crate in the sidebar
    When I open its context menu
    And I arrow through the items
    Then I hear each item announced

  Scenario: The sidebar root context menus narrate
    Given focus is on the Playlists root in the sidebar
    When I open its context menu and arrow through it
    Then I hear each item announced
    Given focus is on the Crates root in the sidebar
    When I open its context menu and arrow through it
    Then I hear each item announced

  # ---------------------------------------------------------------------
  # The library scanner dialog — issue #88
  # ---------------------------------------------------------------------

  Scenario: The scanner announces itself when it appears
    Given a music directory containing enough tracks that scanning takes a few seconds
    When a library scan starts
    Then I hear "Scanning library"

  Scenario: The scanner announcement does not repeat during one scan
    # The dialog can show and hide repeatedly within a single scan.
    Given a library scan is in progress and I have heard "Scanning library"
    When the scan continues for another 30 seconds
    Then I do not hear "Scanning library" again

  Scenario: A second scan announces again
    Given one library scan has completed
    When I start another scan
    Then I hear "Scanning library"

  Scenario: Per-file progress is not read out
    # EXPECTED: only the one announcement. Reading every filename would be
    # unusable. Confirm the silence is the intended silence.
    Given a library scan is in progress
    Then I do not hear individual file names announced

  # ---------------------------------------------------------------------
  # The track row readout in ordinary use — issue #72
  # ---------------------------------------------------------------------

  @blocking
  Scenario: Arrowing through a playlist tells me where I am
    Given a playlist containing 40 tracks is showing
    And the track table has focus
    When I arrow down through several tracks
    Then I hear each track's artist and title
    And I hear its position, counting up as I go
    And the total matches the number of tracks in the playlist

  Scenario: A track with no artist announces what it has
    Given a track with a title but no artist
    When I arrow onto it
    Then I hear the title
    And I do not hear an empty pause where the artist would be

  Scenario: Holding the arrow key does not produce a burst of speech
    # Selection is debounced. Scrolling fast should land on one readout,
    # not queue forty.
    Given the track table has focus at the top of a long list
    When I hold the down arrow for two seconds and release
    Then I hear one readout, for the track I landed on
    And I do not hear a backlog of earlier tracks

  Scenario: Row readouts can be turned off
    Given "Announce track selection" is disabled in Preferences, Accessibility
    When I arrow through the track table
    Then I hear nothing
    And re-enabling the setting restores the readouts

  # ---------------------------------------------------------------------
  # YouTube source — issue #85
  #
  # This is the only place in the fork where a network operation of unknown
  # duration reports back by ear. The progress design is deliberately
  # sparse: exactly three utterances, "25%", "50%" and "75%", each the bare
  # number and a percent sign. 0% and 100% are never spoken.
  #
  # It shells out to yt-dlp, so a machine without yt-dlp installed will
  # exercise the failure paths instead — which are worth testing too.
  # ---------------------------------------------------------------------

  Scenario: The YouTube source is in the sidebar and explains itself
    Given "ShowYouTubeLibrary" is enabled
    When I arrow to "YouTube" in the sidebar and select it
    Then I hear "YouTube search. Type in the search box to find tracks."
    When I expand it
    Then I hear a child item called "Downloaded"

  @blocking
  Scenario: A search reports that it started, and what it found
    Given yt-dlp is installed and on the PATH
    And the YouTube source is selected
    When I type "aphex twin" into the search box
    Then I hear "Searching YouTube for" followed by the query
    And when the results arrive I hear a count, such as "12 results"

  Scenario: A search with one result uses the singular
    When I run a YouTube search that returns exactly one result
    Then I hear "1 result"
    And I do not hear "1 results"

  Scenario: A search with no results says so
    When I run a YouTube search for a nonsense string that returns nothing
    Then I hear "No results found."

  Scenario: Clearing the search box says nothing
    Given a YouTube search has returned results
    When I clear the search box
    Then I hear nothing

  Scenario: Repeating the same search does not re-announce
    Given I have just searched for "aphex twin" and heard the result count
    When I search for exactly "aphex twin" again
    Then I hear nothing

  @blocking
  Scenario: Search results are readable by ear
    Given a YouTube search has returned results
    When I arrow through them
    Then I hear each result's title
    And I hear ", by " followed by the channel name
    And I hear its duration in minutes and seconds
    And I hear its position in the result list

  Scenario: An already-downloaded result says so
    Given a YouTube search whose results include a track I have already downloaded
    When I arrow onto that result
    Then I hear ", downloaded" at the end of the readout
    And I can tell it apart from a result I have not fetched

  Scenario: A short result is announced in seconds only
    Given a search result shorter than one minute
    When I arrow onto it
    Then I hear its duration in seconds
    And I do not hear "0 minutes"

  @blocking
  Scenario: Downloading a track reports start and progress but not completion
    Given yt-dlp is installed and a download directory is configured
    And a YouTube search result is selected that I have not downloaded
    When I press "Return" to load it
    Then I hear "Downloading " followed by the track title
    And as it proceeds I hear "25%"
    And then "50%"
    And then "75%"
    And I do not hear "0%"
    And I do not hear "100%"
    And when it finishes the track simply loads, with no completion announcement

  Scenario: Progress milestones do not repeat or go backwards
    Given a download is in progress and I have heard "50%"
    When yt-dlp reports a percentage that maps back below 50
    Then I do not hear "25%" or "50%" again

  Scenario: A very fast download may skip milestones
    # EXPECTED: a small file can jump straight past 25 and 50. Not a bug.
    Given a very short track is downloaded quickly
    When the download completes
    Then I may hear only some of the milestones, or none
    And the track still loads

  Scenario Outline: Download and search failures are spoken usefully
    Given "<situation>"
    When I "<action>"
    Then I hear a spoken message containing "<fragment>"
    And the message tells me enough to know what to fix

    Examples:
      | situation                              | action                       | fragment                    |
      | yt-dlp is not installed or not on PATH | run a YouTube search         | Is it installed             |
      | yt-dlp is not installed or not on PATH | download a search result     | Download failed             |
      | no download directory is configured    | download a search result     | No download directory       |
      | a download is already running          | start a second download      | already in progress         |

  Scenario: A yt-dlp error message is read rather than swallowed
    Given a search that will make yt-dlp fail with a message on its error output
    When I run the search
    Then I hear yt-dlp's own message spoken
    And I am not left with silence

  Scenario: The downloaded track carries its source in the comment
    Given a YouTube track has been downloaded successfully
    When I open its track info dialog
    Then the comment field contains the source URL, the uploader and the licence
    And my screen reader can read it

  Scenario: The on-screen download progress is not spoken
    # EXPECTED: the overview widget paints a progress readout that is
    # deliberately visual only. The spoken milestones are the accessible
    # equivalent. Confirm they do not double up.
    Given a download is in progress on deck 1
    Then I hear only the milestone announcements
    And I do not hear a continuous stream of percentages

  Scenario: The YouTube source can be hidden
    Given "ShowYouTubeLibrary" is disabled
    When I arrow through the sidebar
    Then I do not find a "YouTube" item
    And nothing else in the sidebar is disturbed

  # ---------------------------------------------------------------------
  # Regression: the spurious cue announcements fix that rode along with #85
  # ---------------------------------------------------------------------

  @regression
  Scenario: Loading a track does not announce cues for decks that are not shown
    # A bug fixed alongside the YouTube port: the headphone cue was being
    # written unconditionally on load, so decks the skin does not show
    # announced themselves. If you hear a deck number that is not on
    # screen, this has come back.
    Given a skin showing only two decks
    When I load a track into deck 1
    Then I hear at most announcements about decks 1 and 2
    And I do not hear anything about deck 3 or deck 4

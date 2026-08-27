#!/usr/bin/env python3
"""D11: "Hiding tracks that are in playlists warns separately"
(features/destructive_actions.feature)

The playlist-membership double-dialog case named explicitly in issue #133
as one of the three shapes of scenario left uncovered by #104/PR #121 (the
other two being the positive-confirm paths -- d9/d10 -- and the Remove
outline, which needs an AutoDJ/crate/playlist *view* and is left for a
follow-up; this scenario needs only playlist *membership*, reachable from
the plain Tracks view).

TrackCollection::hideTracks() (src/library/trackcollection.cpp) checks
playlist membership globally, independent of which library view is
showing -- if any selected track belongs to a non-history playlist, a
second QMessageBox ("Hiding tracks", Ok/Cancel, default Cancel) appears
after the first hide confirmation is accepted, warning that hiding will
also remove the track from those playlists. This second dialog is NOT
narrated by Mixxx's own TTS (only the first hide confirmation is wired to
announceText()) -- the screen reader is the only thing that would read it,
which is exactly the gap issue #74/PR #121's "Known mismatch" note and
COVERAGE.md's "Still untested" section flag. This scenario cannot assert
the words of a screen reader, but it can assert the one thing that
actually protects data: that the safe default (Cancel) on this un-narrated
second dialog really is safe, i.e. pressing Return without moving focus
leaves the track hidden-from-nothing -- still in its playlist, still
visible in the library.

Seeds the playlist directly via library_fixture.seed_playlist rather than
driving "Create New Playlist" / drag-and-drop, consistent with the rest of
this fixture's philosophy (see library_fixture.py's module docstring).

Run via the orchestrator::

    python3 tools/e2e/run_e2e.py --scenario d11_hide_playlist_membership_warns
"""
import os
import sys
import time

from library_actions import click_dialog_accept_button, hide_or_remove_selected, select_all_tracks
from library_fixture import (
    bootstrap_settings_dir,
    is_visible,
    make_wav_files,
    playlist_track_ids,
    seed_playlist,
    seed_tracks,
)

_SEEDED = []
_SETTINGS_DIR = None
_PLAYLIST_NAME = "E2E Fixture Playlist"


def prepare(settings_dir, mixxx_bin):
    global _SEEDED, _SETTINGS_DIR
    _SETTINGS_DIR = settings_dir
    bootstrap_settings_dir(mixxx_bin, settings_dir)
    music_dir = os.path.normpath(os.path.join(settings_dir, "..", "music"))
    _SEEDED = seed_tracks(settings_dir, make_wav_files(music_dir, count=1))
    seed_playlist(settings_dir, _PLAYLIST_NAME, _SEEDED)


def run(driver, tts):
    track = _SEEDED[0]

    select_all_tracks(driver)
    hide_or_remove_selected(driver)

    if not tts.wait_for("hide the selected 1 track", timeout=10.0):
        print("FAIL: hide confirmation was not announced")
        print("--- TTS log so far ---")
        print(tts.read())
        sys.exit(1)
    print("OK: hide confirmation announced")

    # Accept the FIRST dialog (Hide/Remove, Ok/Cancel, default Cancel) --
    # the one Property 2 elsewhere in this file is about pressing Return on
    # by reflex. Here we deliberately move past it to reach the second,
    # un-narrated playlist-membership warning, which is this scenario's
    # actual subject.
    click_dialog_accept_button(driver, expected_count=2)
    time.sleep(0.8)

    # Second dialog: "Hiding tracks" warning, Ok/Cancel, default Cancel
    # (TrackCollection::hideTracks()). Not narrated -- nothing to wait_for
    # in the TTS log here, which is itself the documented gap. Press Return
    # without moving focus, the reflexive action Property 2 is about.
    driver.press("return")
    time.sleep(1.0)

    if not is_visible(_SETTINGS_DIR, track):
        print("FAIL: track was hidden despite Return on the playlist-membership warning")
        sys.exit(1)
    remaining = playlist_track_ids(_SETTINGS_DIR, _PLAYLIST_NAME)
    if not remaining:
        print("FAIL: track was removed from its playlist despite Return on the warning")
        sys.exit(1)

    print("PASS: the un-narrated playlist-membership warning defaulted safe -- "
          "track is still visible and still in its playlist")


if __name__ == "__main__":
    from ax_driver import AxDriver, TtsLog, create_backend

    tts_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mixxx-e2e/tts.log"
    driver = AxDriver(create_backend()).connect()
    run(driver, TtsLog(tts_path))

#!/usr/bin/env python3
"""D8: "Repeated Enter presses on a stack of dialogs stay safe"
(features/destructive_actions.feature, cross-cutting)

# The realistic accident: a key press queued while a dialog was opening, or
# a stuck key. Every layer must default safe.

Simplified from the Gherkin, which uses a track that belongs to a playlist
to exercise the second, un-narrated "Hiding tracks" playlist warning dialog
stacked on top of the first: this scenario only seeds plain library tracks,
so it exercises the single hide-confirmation dialog receiving three rapid
Returns rather than two stacked dialogs each receiving one. The
playlist-membership double-dialog case needs Playlists/PlaylistTracks fixture
rows this slice does not add yet -- left manual; see COVERAGE.md.

Run via the orchestrator::

    python3 tools/e2e/run_e2e.py --scenario d8_repeated_enter_stack_safe
"""
import os
import sys
import time

from library_actions import hide_or_remove_selected, select_all_tracks
from library_fixture import bootstrap_settings_dir, is_visible, make_wav_files, seed_tracks

_SEEDED = []
_SETTINGS_DIR = None


def prepare(settings_dir, mixxx_bin):
    global _SEEDED, _SETTINGS_DIR
    _SETTINGS_DIR = settings_dir
    bootstrap_settings_dir(mixxx_bin, settings_dir)
    music_dir = os.path.normpath(os.path.join(settings_dir, "..", "music"))
    _SEEDED = seed_tracks(settings_dir, make_wav_files(music_dir, count=1))


def run(driver, tts):
    track = _SEEDED[0]

    select_all_tracks(driver)
    hide_or_remove_selected(driver)

    if not tts.wait_for("hide the selected 1 track", timeout=10.0):
        print("FAIL: hide confirmation was not announced")
        sys.exit(1)

    for _ in range(3):
        driver.press("return")
        time.sleep(0.15)
    time.sleep(1.0)

    if not is_visible(_SETTINGS_DIR, track):
        print("FAIL: track was hidden after three rapid Return presses")
        sys.exit(1)

    print("PASS: three rapid Return presses on the hide dialog stayed safe")


if __name__ == "__main__":
    from ax_driver import AxDriver, TtsLog, create_backend

    tts_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mixxx-e2e/tts.log"
    driver = AxDriver(create_backend()).connect()
    run(driver, TtsLog(tts_path))

#!/usr/bin/env python3
"""D7: "No destructive action fires without a confirmation"
(features/destructive_actions.feature, @blocking, cross-cutting)

Triggers Purge, Hide and "Delete Track Files" in turn on the same track,
Escaping each one closed, and asserts that (a) every single one produced an
audible confirmation before anything could change and (b) after all three,
the track is untouched. "Remove" is deliberately not included here -- it
needs an AutoDJ/crate/playlist view rather than the plain library table, and
is left for a follow-up (see COVERAGE.md).

Run via the orchestrator::

    python3 tools/e2e/run_e2e.py --scenario d7_no_destructive_action_fires_immediately
"""
import os
import sys
import time

from library_actions import (
    choose_context_menu_item,
    choose_context_menu_item_any,
    hide_or_remove_selected,
    select_all_tracks,
)
from library_fixture import bootstrap_settings_dir, is_visible, make_wav_files, seed_tracks

_SEEDED = []
_SETTINGS_DIR = None

_DELETE_MENU_LABELS = ["Move Track File", "Delete Files from Disk"]


def prepare(settings_dir, mixxx_bin):
    global _SEEDED, _SETTINGS_DIR
    _SETTINGS_DIR = settings_dir
    bootstrap_settings_dir(mixxx_bin, settings_dir)
    music_dir = os.path.normpath(os.path.join(settings_dir, "..", "music"))
    _SEEDED = seed_tracks(settings_dir, make_wav_files(music_dir, count=1))


def _assert_confirmed_then_cancel(tts, expect_substring, label):
    if not tts.wait_for(expect_substring, timeout=10.0):
        print(f"FAIL: {label} did not announce a confirmation before acting")
        print("--- TTS log so far ---")
        print(tts.read())
        sys.exit(1)
    print(f"OK: {label} opened a confirmation I could hear before anything changed")


def run(driver, tts):
    track = _SEEDED[0]

    select_all_tracks(driver)
    choose_context_menu_item(driver, "Purge")
    _assert_confirmed_then_cancel(tts, "Permanently remove 1 track from the library", "Purge")
    driver.press("escape")
    time.sleep(0.5)
    if not is_visible(_SETTINGS_DIR, track):
        print("FAIL: track was purged despite Escape")
        sys.exit(1)

    select_all_tracks(driver)
    hide_or_remove_selected(driver)
    _assert_confirmed_then_cancel(tts, "hide the selected 1 track", "Hide")
    driver.press("escape")
    time.sleep(0.5)
    if not is_visible(_SETTINGS_DIR, track):
        print("FAIL: track was hidden despite Escape")
        sys.exit(1)

    select_all_tracks(driver)
    choose_context_menu_item_any(driver, _DELETE_MENU_LABELS)
    _assert_confirmed_then_cancel(tts, "Cancel is selected by default", "Delete Track Files")
    driver.press("escape")
    time.sleep(0.5)
    if not os.path.exists(track):
        print("FAIL: file was deleted despite Escape")
        sys.exit(1)
    if not is_visible(_SETTINGS_DIR, track):
        print("FAIL: track was removed from the library despite Escape")
        sys.exit(1)

    print("PASS: Purge, Hide and Delete Track Files all confirmed before acting, and none of "
          "them acted immediately")


if __name__ == "__main__":
    from ax_driver import AxDriver, TtsLog, create_backend

    tts_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mixxx-e2e/tts.log"
    driver = AxDriver(create_backend()).connect()
    run(driver, TtsLog(tts_path))

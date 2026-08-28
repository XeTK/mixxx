#!/usr/bin/env python3
"""D4: "Pressing Enter on the hide dialog does not hide"
(features/destructive_actions.feature, @blocking)

Triggers Hide the same way a user would by reflex: Ctrl+Backspace on the
focused track table (util/defs.h's kHideRemoveShortcutKey /
kHideRemoveShortcutModifier), NOT via the "Hide from Library" context-menu
item -- that item calls straight through to TrackDAO with no confirmation at
all (see library_actions.hide_or_remove_selected's docstring). Only the
direct-keypress path in WTrackTableView::keyPressEvent shows the dialog this
scenario is about.

Run via the orchestrator::

    python3 tools/e2e/run_e2e.py --scenario d4_hide_enter_safe
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
    _SEEDED = seed_tracks(settings_dir, make_wav_files(music_dir, count=3))


def run(driver, tts):
    select_all_tracks(driver)
    hide_or_remove_selected(driver)

    # "track" not "tracks" -- see d1_purge_enter_safe.py's comment; this
    # build's announcement is "3 track(s)" without a loaded translation
    # catalog for %n to resolve against.
    if not tts.wait_for("hide the selected 3 track", timeout=10.0):
        print("FAIL: hide confirmation was not announced")
        print("--- TTS log so far ---")
        print(tts.read())
        sys.exit(1)
    print("OK: hide confirmation announced")

    driver.press("return")
    time.sleep(1.0)

    missing = [p for p in _SEEDED if not is_visible(_SETTINGS_DIR, p)]
    if missing:
        print(f"FAIL: {len(missing)} track(s) no longer visible after pressing Return: {missing}")
        sys.exit(1)

    print("PASS: pressing Return on the hide dialog hid nothing")


if __name__ == "__main__":
    from ax_driver import AxDriver, TtsLog, create_backend

    tts_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mixxx-e2e/tts.log"
    driver = AxDriver(create_backend()).connect()
    run(driver, TtsLog(tts_path))

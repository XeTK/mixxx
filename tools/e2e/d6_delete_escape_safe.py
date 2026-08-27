#!/usr/bin/env python3
"""D6: "Pressing Escape on the delete dialog does not delete"
(features/destructive_actions.feature, @blocking)

Same as d5_delete_enter_safe but via Escape.

Run via the orchestrator::

    python3 tools/e2e/run_e2e.py --scenario d6_delete_escape_safe
"""
import os
import sys
import time

from library_actions import choose_context_menu_item_any, select_all_tracks
from library_fixture import bootstrap_settings_dir, make_wav_files, seed_tracks

_SEEDED = []

_DELETE_MENU_LABELS = ["Move Track File", "Delete Files from Disk"]


def prepare(settings_dir, mixxx_bin):
    global _SEEDED
    bootstrap_settings_dir(mixxx_bin, settings_dir)
    music_dir = os.path.normpath(os.path.join(settings_dir, "..", "music"))
    _SEEDED = seed_tracks(settings_dir, make_wav_files(music_dir, count=2))


def run(driver, tts):
    select_all_tracks(driver)
    choose_context_menu_item_any(driver, _DELETE_MENU_LABELS)

    if not tts.wait_for("Cancel is selected by default", timeout=10.0):
        print("FAIL: delete confirmation was not announced")
        sys.exit(1)
    print("OK: delete confirmation announced")

    driver.press("escape")
    time.sleep(1.0)

    gone = [p for p in _SEEDED if not os.path.exists(p)]
    if gone:
        print(f"FAIL: {len(gone)} file(s) removed from disk after pressing Escape: {gone}")
        sys.exit(1)

    print("PASS: pressing Escape on the delete dialog deleted nothing")


if __name__ == "__main__":
    from ax_driver import AxDriver, TtsLog, create_backend

    tts_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mixxx-e2e/tts.log"
    driver = AxDriver(create_backend()).connect()
    run(driver, TtsLog(tts_path))

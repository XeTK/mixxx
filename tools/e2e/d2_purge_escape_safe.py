#!/usr/bin/env python3
"""D2: "Pressing Escape on the purge dialog does not purge"
(features/destructive_actions.feature, @blocking)

Same as d1_purge_enter_safe but via Escape rather than Return -- both must be
safe, and the feature file checks them as two separate scenarios rather than
assuming one implies the other.

Run via the orchestrator::

    python3 tools/e2e/run_e2e.py --scenario d2_purge_escape_safe
"""
import os
import sys
import time

from library_actions import choose_context_menu_item, select_all_tracks
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
    choose_context_menu_item(driver, "Purge")

    # "track" not "tracks" -- see d1_purge_enter_safe.py's comment on the
    # same check; this build's announcement is "3 track(s)", not "3 tracks",
    # without a loaded translation catalog.
    if not tts.wait_for("Permanently remove 3 track", timeout=10.0):
        print("FAIL: purge confirmation was not announced")
        sys.exit(1)
    print("OK: purge confirmation announced")

    driver.press("escape")
    time.sleep(1.0)

    missing = [p for p in _SEEDED if not is_visible(_SETTINGS_DIR, p)]
    if missing:
        print(f"FAIL: {len(missing)} track(s) no longer visible after pressing Escape: {missing}")
        sys.exit(1)

    print("PASS: pressing Escape on the purge dialog purged nothing")


if __name__ == "__main__":
    from ax_driver import AxDriver, TtsLog, create_backend

    tts_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mixxx-e2e/tts.log"
    driver = AxDriver(create_backend()).connect()
    run(driver, TtsLog(tts_path))

#!/usr/bin/env python3
"""D9: "Deliberately confirming a hide does hide"
(features/destructive_actions.feature)

The positive-confirm counterpart to d4_hide_enter_safe.py: moves focus to
the accept button and actually performs the action, rather than pressing
Return/Escape on the safe default. Selects 2 fixture tracks, triggers the
hide confirmation via the direct keypress (see hide_or_remove_selected's
docstring for why that path and not the menu), clicks the dialog's accept
button (see click_dialog_accept_button's docstring for why a click and not
Tab+Return), and asserts the tracks are marked hidden (mixxx_deleted=1) in
the library DB afterward.

Run via the orchestrator::

    python3 tools/e2e/run_e2e.py --scenario d9_hide_confirm_deliberate
"""
import os
import sys
import time

from library_actions import click_dialog_accept_button, hide_or_remove_selected, select_all_tracks
from library_fixture import bootstrap_settings_dir, make_wav_files, seed_tracks, track_row

_SEEDED = []
_SETTINGS_DIR = None


def prepare(settings_dir, mixxx_bin):
    global _SEEDED, _SETTINGS_DIR
    _SETTINGS_DIR = settings_dir
    bootstrap_settings_dir(mixxx_bin, settings_dir)
    music_dir = os.path.normpath(os.path.join(settings_dir, "..", "music"))
    _SEEDED = seed_tracks(settings_dir, make_wav_files(music_dir, count=2))


def run(driver, tts):
    select_all_tracks(driver)
    hide_or_remove_selected(driver)

    if not tts.wait_for("hide the selected 2 track", timeout=10.0):
        print("FAIL: hide confirmation was not announced")
        print("--- TTS log so far ---")
        print(tts.read())
        sys.exit(1)
    print("OK: hide confirmation announced")

    click_dialog_accept_button(driver, expected_count=2)
    time.sleep(1.0)

    still_visible = []
    missing_row = []
    for p in _SEEDED:
        row = track_row(_SETTINGS_DIR, p)
        if row is None:
            missing_row.append(p)
        elif not row["mixxx_deleted"]:
            still_visible.append(p)

    if missing_row:
        print(f"FAIL: {len(missing_row)} track(s) have no library row at all: {missing_row}")
        sys.exit(1)
    if still_visible:
        print(f"FAIL: {len(still_visible)} track(s) were not hidden: {still_visible}")
        sys.exit(1)

    print("PASS: deliberately confirming the hide dialog hid the selected tracks")


if __name__ == "__main__":
    from ax_driver import AxDriver, TtsLog, create_backend

    tts_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mixxx-e2e/tts.log"
    driver = AxDriver(create_backend()).connect()
    run(driver, TtsLog(tts_path))

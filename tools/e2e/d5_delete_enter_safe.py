#!/usr/bin/env python3
"""D5: "Pressing Enter on the delete dialog does not delete"
(features/destructive_actions.feature, @blocking)

The irreversible one. Opens "Move Track File(s) to Trash" / "Delete Files
from Disk" (label depends on the Qt version Mixxx was built against -- see
WTrackMenu::slotRemoveFromDisk) from the track context menu, then presses
Return without moving focus. Cancel is the default button
(cancelBtn->setDefault(true)), so this must be a no-op: files must still be
on disk and tracks must still be in the library.

Run via the orchestrator::

    python3 tools/e2e/run_e2e.py --scenario d5_delete_enter_safe
"""
import os
import sys
import time

from library_actions import choose_context_menu_item_any, select_all_tracks
from library_fixture import bootstrap_settings_dir, is_visible, make_wav_files, seed_tracks

_SEEDED = []
_SETTINGS_DIR = None

_DELETE_MENU_LABELS = ["Move Track File", "Delete Files from Disk"]


def prepare(settings_dir, mixxx_bin):
    global _SEEDED, _SETTINGS_DIR
    _SETTINGS_DIR = settings_dir
    bootstrap_settings_dir(mixxx_bin, settings_dir)
    music_dir = os.path.normpath(os.path.join(settings_dir, "..", "music"))
    _SEEDED = seed_tracks(settings_dir, make_wav_files(music_dir, count=2))


def run(driver, tts):
    select_all_tracks(driver)
    choose_context_menu_item_any(driver, _DELETE_MENU_LABELS)

    # "Cancel is selected by default" is the one clause shared verbatim by
    # both the pre- and post-Qt-5.15 announcement wordings (see
    # trackconfirmdialogs.cpp::deleteFromDiskAnnouncement).
    if not tts.wait_for("Cancel is selected by default", timeout=10.0):
        print("FAIL: delete confirmation was not announced")
        print("--- TTS log so far ---")
        print(tts.read())
        sys.exit(1)
    print("OK: delete confirmation announced")

    driver.press("return")
    time.sleep(1.0)

    gone = [p for p in _SEEDED if not os.path.exists(p)]
    if gone:
        print(f"FAIL: {len(gone)} file(s) removed from disk after pressing Return: {gone}")
        sys.exit(1)
    missing = [p for p in _SEEDED if not is_visible(_SETTINGS_DIR, p)]
    if missing:
        print(f"FAIL: {len(missing)} track(s) no longer in the library: {missing}")
        sys.exit(1)

    print("PASS: pressing Return on the delete dialog deleted nothing")


if __name__ == "__main__":
    from ax_driver import AxDriver, TtsLog, create_backend

    tts_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mixxx-e2e/tts.log"
    driver = AxDriver(create_backend()).connect()
    run(driver, TtsLog(tts_path))

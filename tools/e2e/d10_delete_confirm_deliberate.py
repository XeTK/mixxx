#!/usr/bin/env python3
"""D10: "Deliberately confirming a delete moves the files to the trash"
(features/destructive_actions.feature)

The positive-confirm counterpart to d5_delete_enter_safe.py/
d6_delete_escape_safe.py. Opens "Move Track File(s) to Trash" from the
track context menu (see open_track_context_menu's docstring for why this
is a real right-click rather than the Shift+F10 chord the harness
originally tried), clicks the dialog's accept button, and asserts the file
is gone from its original path and the track is gone from the library.

A second, single-OK "Track Files Moved To Trash" info popup appears after
a successful delete (WTrackMenu::slotRemoveFromDisk, gated by
s_showPurgeSuccessPopup, true by default) -- this is dismissed with its
own accept-button click too.

Does not assert the file actually landed in the macOS trash (~/.Trash):
RemoveTrackFilesFromDiskTrackPointerOperation calls Qt's
QFile::moveToTrash(), whose exact destination is platform- and
Finder-preferences-dependent, and "the file is no longer at that path" is
the one part of the Gherkin's assertion (`the file is in the system trash,
recoverable`) that is both checkable and not testing Qt/macOS's own
trash implementation.

Run via the orchestrator::

    python3 tools/e2e/run_e2e.py --scenario d10_delete_confirm_deliberate
"""
import os
import sys
import time

from library_actions import choose_context_menu_item_any, click_dialog_accept_button, select_all_tracks
from library_fixture import bootstrap_settings_dir, make_wav_files, seed_tracks, track_row

_SEEDED = []
_SETTINGS_DIR = None

_DELETE_MENU_LABELS = ["Move Track File", "Delete Files from Disk"]


def prepare(settings_dir, mixxx_bin):
    global _SEEDED, _SETTINGS_DIR
    _SETTINGS_DIR = settings_dir
    bootstrap_settings_dir(mixxx_bin, settings_dir)
    music_dir = os.path.normpath(os.path.join(settings_dir, "..", "music"))
    _SEEDED = seed_tracks(settings_dir, make_wav_files(music_dir, count=1))


def run(driver, tts):
    track = _SEEDED[0]

    select_all_tracks(driver)
    choose_context_menu_item_any(driver, _DELETE_MENU_LABELS)

    if not tts.wait_for("Cancel is selected by default", timeout=10.0):
        print("FAIL: delete confirmation was not announced")
        print("--- TTS log so far ---")
        print(tts.read())
        sys.exit(1)
    print("OK: delete confirmation announced")

    # First dialog: Cancel / Okay (or Delete Files on old Qt).
    click_dialog_accept_button(driver, expected_count=2)
    time.sleep(1.0)

    # Second dialog: the single-OK "Track Files Moved To Trash" info popup.
    click_dialog_accept_button(driver, expected_count=1)
    time.sleep(0.5)

    if os.path.exists(track):
        print(f"FAIL: file still exists at its original path: {track}")
        sys.exit(1)
    row = track_row(_SETTINGS_DIR, track)
    if row is not None:
        print(f"FAIL: track still has a library row after deliberate delete: {row}")
        sys.exit(1)

    print("PASS: deliberately confirming the delete dialog removed the file and the library entry")


if __name__ == "__main__":
    from ax_driver import AxDriver, TtsLog, create_backend

    tts_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mixxx-e2e/tts.log"
    driver = AxDriver(create_backend()).connect()
    run(driver, TtsLog(tts_path))

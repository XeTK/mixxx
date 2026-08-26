#!/usr/bin/env python3
"""D3: "Purging a single track asks first and says what it means"
(features/destructive_actions.feature)

Property 1 from the top of that file: the user must be TOLD what is about to
happen, in enough detail to decide, before the dialog needs a decision at
all. Asserts the exact announced string (mixxx::trackconfirm::purgeAnnouncement
in src/widget/trackconfirmdialogs.cpp) for a single track.

Does not attempt the screen-reader-focused-button assertions from the same
Gherkin scenario ("my screen reader announces that the focused button is
'No'") -- that is genuinely screen-reader output, not Mixxx's own TTS, and
is out of reach of the --tts-log hook. Left manual; see COVERAGE.md.

Run via the orchestrator::

    python3 tools/e2e/run_e2e.py --scenario d3_purge_narrates_before_dialog
"""
import os
import sys
import time

from library_actions import choose_context_menu_item, select_all_tracks
from library_fixture import bootstrap_settings_dir, make_wav_files, seed_tracks

_SEEDED = []
_SETTINGS_DIR = None


def prepare(settings_dir, mixxx_bin):
    global _SEEDED, _SETTINGS_DIR
    _SETTINGS_DIR = settings_dir
    bootstrap_settings_dir(mixxx_bin, settings_dir)
    music_dir = os.path.normpath(os.path.join(settings_dir, "..", "music"))
    _SEEDED = seed_tracks(settings_dir, make_wav_files(music_dir, count=1))


def run(driver, tts):
    select_all_tracks(driver)
    choose_context_menu_item(driver, "Purge")

    expected = (
        "Purge tracks dialog. Permanently remove 1 track from the library? "
        "This only removes the library entry, it does not delete the file "
        "from disk. No is selected by default; press Escape or Enter for "
        "no, or move to Yes and press Enter to purge."
    )
    if not tts.wait_for(expected, timeout=10.0):
        print("FAIL: exact purge announcement was not heard")
        print(f"  expected: {expected!r}")
        print("--- TTS log so far ---")
        print(tts.read())
        sys.exit(1)

    print("PASS: purge dialog announces itself verbatim before asking for a decision")

    # Leave the app in a clean state for teardown.
    driver.press("escape")
    time.sleep(0.3)


if __name__ == "__main__":
    from ax_driver import AxDriver, TtsLog, create_backend

    tts_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mixxx-e2e/tts.log"
    driver = AxDriver(create_backend()).connect()
    run(driver, TtsLog(tts_path))

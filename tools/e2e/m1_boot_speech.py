#!/usr/bin/env python3
"""M1 Scenario 1: boot speech.

Asserts that a freshly launched Mixxx speaks "Mixxx ready" (captured via the
``--tts-log`` hook) and that a main window is exposed in the AX tree.

Run via the orchestrator::

    python3 tools/e2e/run_e2e.py --scenario m1_boot_speech
"""
import sys


def run(driver, tts):
    # The orchestrator already waited for the window; also confirm it here so
    # this scenario is self-contained when driven directly.
    if driver.wait_for_window(timeout=30.0) is None:
        print("FAIL: no main window in the AX tree")
        sys.exit(1)

    print("Waiting for 'Mixxx ready' in the TTS log...")
    if not tts.wait_for("Mixxx ready", timeout=30.0):
        print("FAIL: 'Mixxx ready' was never spoken")
        print("--- TTS log so far ---")
        print(tts.read())
        sys.exit(1)

    print("PASS: 'Mixxx ready' spoken and main window present")


if __name__ == "__main__":
    # Allow running standalone against an already-running Mixxx + tts log.
    from ax_driver import AxDriver, TtsLog, create_backend

    tts_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mixxx-e2e/tts.log"
    driver = AxDriver(create_backend()).connect()
    run(driver, TtsLog(tts_path))

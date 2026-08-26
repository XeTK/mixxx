#!/usr/bin/env python3
"""M3 Scenario 4: keyboard readout via Alt+1 -> TTS log.

Sends the Alt+1 key combination (deck status readout) to Mixxx and asserts
the TTS log contains new spoken output.

Run via the orchestrator::

    python3 tools/e2e/run_e2e.py --scenario m4_keyboard_readout
"""
import sys

from ax_driver import AxDriver, TtsLog, create_backend

# Alt+1 = deck 1 status readout. Keycode 18 = '1'.
KEY_1 = 18


def run(driver, tts):
    print("Sending Alt+1 (deck 1 status)...")
    before = tts.snapshot()
    driver.send_key(KEY_1, modifiers=("alt",))

    if not tts.wait_for_change(before, timeout=10.0):
        print("FAIL: no speech captured after Alt+1")
        sys.exit(1)

    new = tts.new_since(before)
    print(f"\n=== New TTS output after Alt+1 ===")
    print(new)
    print("\nPASS: keyboard readout produced speech")


if __name__ == "__main__":
    tts_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mixxx-e2e/tts.log"
    driver = AxDriver(create_backend()).connect()
    run(driver, TtsLog(tts_path))

#!/usr/bin/env python3
"""M2 Scenario 2: DDJ-400 spoken menu end-to-end.

Drives the running Mixxx through the DDJ-400 emulator's virtual MIDI port and
asserts on the spoken output captured via ``--tts-log``:

  1. Hold BROWSE (>0.4s) to open the menu -> "Main menu".
  2. Rotate BROWSE down x2 -> the focused item's label is spoken.
  3. Press BROWSE (activate) -> submenu/action spoken.
  4. Press BROWSE+SHIFT (back) -> going back spoken.

This uses the emulator's *scripted* mode (``--script``), which is the documented
deterministic path for the hold-to-open gesture and the browse+shift back
gesture (the interactive line protocol cannot express browse+shift).

Requires the DDJ-400 mapping to be assigned to the emulator's virtual port in
Mixxx (see tools/ddj400-emulator/README.md). Written to run against a live app;
it cannot be validated without one.

Run via the orchestrator::

    python3 tools/e2e/run_e2e.py --scenario m2_ddj400_menu
"""
import os
import subprocess
import sys
import tempfile
import time

from ax_driver import TtsLog

HERE = os.path.dirname(os.path.abspath(__file__))
EMULATOR = os.path.join(HERE, "..", "ddj400-emulator", "ddj400_emulator.py")


def _write_sequence(path):
    """Write a scripted sequence for the emulator (see emulator README)."""
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(
            "# M2: open menu, navigate down, activate, go back\n"
            "press browse hold=0.8\n"   # hold-to-open gesture
            "sleep 1.0\n"
            "rotate down\n"
            "rotate down\n"
            "sleep 0.5\n"
            "press browse\n"            # activate focused item
            "sleep 0.5\n"
            "press browse+shift\n"      # back
            "sleep 0.5\n"
        )


def run(driver, tts):
    port = os.environ.get("DDJ400_PORT", "DDJ-400 Emulator")
    seq_path = os.path.join(tempfile.gettempdir(), "mixxx-e2e-m2-seq.txt")
    _write_sequence(seq_path)

    print(f"Starting DDJ-400 emulator on port {port!r} (scripted)...")
    emu = subprocess.Popen(
        [sys.executable, EMULATOR, "--port", port, "--script", seq_path],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        # Give the virtual port a moment to register before Mixxx polls it.
        time.sleep(1.0)

        # 1. Hold BROWSE to open the menu.
        if not tts.wait_for("Main menu", timeout=15.0):
            print("FAIL: menu did not open (no 'Main menu' spoken)")
            print("--- TTS log so far ---")
            print(tts.read())
            sys.exit(1)
        print("OK: menu opened")

        # 2. Rotate BROWSE down x2; assert navigation produced new speech.
        before = tts.snapshot()
        if not tts.wait_for_change(before, timeout=10.0):
            print("FAIL: no speech after BROWSE rotate down")
            sys.exit(1)
        print(f"OK: navigation spoke: {tts.new_since(before)!r}")

        # 3. Press BROWSE (activate); assert new speech.
        before = tts.snapshot()
        if not tts.wait_for_change(before, timeout=10.0):
            print("FAIL: no speech after BROWSE activate")
            sys.exit(1)
        print(f"OK: activate spoke: {tts.new_since(before)!r}")

        # 4. Press BROWSE+SHIFT (back); assert new speech.
        before = tts.snapshot()
        if not tts.wait_for_change(before, timeout=10.0):
            print("FAIL: no speech after back gesture")
            sys.exit(1)
        print(f"OK: back spoke: {tts.new_since(before)!r}")

        print("PASS: DDJ-400 menu open/navigate/activate/back all spoke")
    finally:
        emu.terminate()
        try:
            emu.wait(timeout=5)
        except subprocess.TimeoutExpired:
            emu.kill()


if __name__ == "__main__":
    # Standalone: attach to a running Mixxx and its tts log.
    from ax_driver import AxDriver, create_backend

    tts_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mixxx-e2e/tts.log"
    driver = AxDriver(create_backend()).connect()
    run(driver, TtsLog(tts_path))

#!/usr/bin/env python3
"""M3 Scenario 4: keyboard readout via Alt+1 -> TTS log.

Sends the Alt+1 key combination (deck status readout) to Mixxx and asserts
the TTS log contains new spoken output.
"""
import sys
import time

import Quartz
from ApplicationServices import AXUIElementCreateApplication


def find_mixxx():
    for app in Quartz.NSWorkspace.sharedWorkspace().runningApplications():
        p = app.executableURL().path() if app.executableURL() else ""
        if p.endswith("/mixxx"):
            return app
    return None


def send_key(keycode, flags):
    down = Quartz.CGEventCreateKeyboardEvent(None, keycode, True)
    Quartz.CGEventSetFlags(down, flags)
    Quartz.CGEventPost(Quartz.kCGHIDEventTap, down)
    up = Quartz.CGEventCreateKeyboardEvent(None, keycode, False)
    Quartz.CGEventSetFlags(up, flags)
    Quartz.CGEventPost(Quartz.kCGHIDEventTap, up)


def main():
    mixxx = find_mixxx()
    if mixxx is None:
        print("FAIL: Mixxx not running")
        sys.exit(1)
    pid = mixxx.processIdentifier()
    print(f"Mixxx pid={pid}")

    tts_log = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mixxx-a11y-test/tts.log"
    try:
        with open(tts_log) as f:
            before = f.read()
    except FileNotFoundError:
        before = ""

    # Alt+1 = deck 1 status readout. Keycode 18 = '1'. Alt = 0x80000.
    print("Sending Alt+1 (deck 1 status)...")
    send_key(18, Quartz.kCGEventFlagMaskAlternate)
    time.sleep(2.0)

    with open(tts_log) as f:
        after = f.read()
    new = after[len(before):].strip()
    print(f"\n=== New TTS output after Alt+1 ===")
    print(new if new else "(nothing new spoken)")

    if not new:
        print("FAIL: no speech captured after Alt+1")
        sys.exit(1)
    print("\nPASS: keyboard readout produced speech")
    sys.exit(0)


if __name__ == "__main__":
    main()

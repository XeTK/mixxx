#!/usr/bin/env python3
"""M3 Scenario 4: keyboard readout via Alt+1 -> TTS log.

Sends the Alt+1 key combination (deck 1 status readout) to Mixxx and asserts
that a specific, expected string actually reached SPOKEN (handed to the
synthesizer) or COMPLETED (fully mixed into the engine output) in the
structured --tts-log (see src/util/ttslog.h).

Earlier versions of this script only checked that the log file grew after the
keypress. That is not a meaningful assertion: a suppressed utterance, one
killed by barge-in mid-render, or a build with a severed audio path can all
still append *something* (a REQUESTED/SUPPRESSED/SUPERSEDED record) without
the deck-status text ever having been spoken. Checking for a SPOKEN/COMPLETED
record naming the exact text is the only way this scenario can tell "Mixxx
tried to say something" apart from "Mixxx actually said the deck status".
"""
import re
import sys
import time

import Quartz
from ApplicationServices import AXUIElementCreateApplication

# Alt+1 = deck 1 status readout. Keycode 18 = '1'. Alt = kCGEventFlagMaskAlternate.
KEY_1 = 18

# The deck-status announcement for an empty deck 1 always ends this way
# regardless of the "speak deck names as numbers" preference (issue: the
# preference only changes the leading "Deck, Alpha" / "Deck 1" portion). This
# assumes deck 1 has no track loaded, which is true for a freshly created
# --settings-path profile (the orchestrator's job; run this scenario against
# a throwaway settings dir, not a developer's real one).
EXPECTED_SUBSTRING = "No track loaded"

# One ttslog record, per the format documented in src/util/ttslog.h:
#   <utc-iso8601-ms> <EVENT> id=<n> [<key>=<value> ...] text="<escaped>"
LINE_RE = re.compile(
    r'^\S+ (?P<event>[A-Z]+) id=(?P<id>\d+)(?: \S+=\S+)? text="(?P<text>.*)"$'
)


def unescape(text):
    out = []
    i = 0
    while i < len(text):
        c = text[i]
        if c == "\\" and i + 1 < len(text):
            nxt = text[i + 1]
            out.append({"\\": "\\", '"': '"', "n": "\n", "r": "\r", "t": "\t"}.get(nxt, nxt))
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def parse_records(content):
    """Yield (event, id, text) for every ttslog line in `content`."""
    for line in content.splitlines():
        m = LINE_RE.match(line)
        if not m:
            continue
        yield m.group("event"), m.group("id"), unescape(m.group("text"))


def find_spoken_or_completed(content, substring):
    """Return the first (event, id, text) record whose text contains
    `substring` and whose event is SPOKEN or COMPLETED, or None."""
    for event, rec_id, text in parse_records(content):
        if event in ("SPOKEN", "COMPLETED") and substring in text:
            return event, rec_id, text
    return None


def find_mixxx():
    for app in Quartz.NSWorkspace.sharedWorkspace().runningApplications():
        p = app.executableURL().path() if app.executableURL() else ""
        if p.endswith("/mixxx"):
            return app
    return None


def focus_mixxx(app):
    """Bring Mixxx to the foreground so the synthetic keypress reaches it
    rather than whatever window happened to have focus (e.g. this script's
    own terminal)."""
    try:
        # macOS 14+.
        app.activate()
    except AttributeError:
        # Older macOS: NSApplicationActivateIgnoringOtherApps.
        app.activateWithOptions_(1 << 1)
    time.sleep(0.5)


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
    # AXUIElementCreateApplication isn't used for element lookups in this
    # scenario, but constructing it (as the other AX-driven scenarios do)
    # confirms the accessibility tree is actually available before we rely on
    # the app being keyboard-focusable.
    AXUIElementCreateApplication(pid)

    focus_mixxx(mixxx)

    tts_log = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mixxx-a11y-test/tts.log"
    try:
        with open(tts_log) as f:
            before = f.read()
    except FileNotFoundError:
        before = ""

    print("Sending Alt+1 (deck 1 status)...")
    send_key(KEY_1, Quartz.kCGEventFlagMaskAlternate)

    # Poll rather than a single fixed sleep: SPOKEN is logged as soon as the
    # text is handed to the synthesizer backend, but COMPLETED can lag behind
    # by however long the utterance takes to render and drain through
    # EngineTts's FIFO (see EngineTts::pollAudibilityEvents).
    deadline = time.time() + 10.0
    after = before
    found = None
    while time.time() < deadline:
        with open(tts_log) as f:
            after = f.read()
        new = after[len(before):]
        found = find_spoken_or_completed(new, EXPECTED_SUBSTRING)
        if found:
            break
        time.sleep(0.25)

    new = after[len(before):].strip()
    print("\n=== New TTS log records after Alt+1 ===")
    print(new if new else "(nothing new logged)")

    if not found:
        print(
            f'\nFAIL: no SPOKEN or COMPLETED record containing "{EXPECTED_SUBSTRING}" '
            "after Alt+1 (the deck status may have been requested but never "
            "actually reached the synthesizer or the engine output -- see "
            "src/util/ttslog.h)"
        )
        sys.exit(1)

    event, rec_id, text = found
    print(f'\nPASS: {event} id={rec_id} text="{text}"')
    sys.exit(0)


if __name__ == "__main__":
    main()

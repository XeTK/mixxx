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

Run via the orchestrator::

    python3 tools/e2e/run_e2e.py --scenario m4_keyboard_readout
"""
import os
import re
import sys
import time

from ax_driver import AxDriver, TtsLog, create_backend

# Alt+1 = deck 1 status readout. Keycode 18 = '1'.
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


def prepare(settings_dir, mixxx_bin):
    # See m1_boot_speech.prepare(): an empty `directories` table makes Mixxx
    # block on a native file-choose dialog before any window appears.
    from library_fixture import bootstrap_settings_dir, register_root_directory

    bootstrap_settings_dir(mixxx_bin, settings_dir)
    music_dir = os.path.normpath(os.path.join(settings_dir, "..", "music"))
    register_root_directory(settings_dir, music_dir)


def run(driver, tts):
    print("Sending Alt+1 (deck 1 status)...")
    before = tts.snapshot()
    driver.send_key(KEY_1, modifiers=("alt",))

    # Poll rather than a single fixed sleep: SPOKEN is logged as soon as the
    # text is handed to the synthesizer backend, but COMPLETED can lag behind
    # by however long the utterance takes to render and drain through
    # EngineTts's FIFO (see EngineTts::pollAudibilityEvents).
    deadline = time.time() + 10.0
    found = None
    new = ""
    while time.time() < deadline:
        new = tts.new_since(before)
        found = find_spoken_or_completed(new, EXPECTED_SUBSTRING)
        if found:
            break
        time.sleep(0.25)

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


if __name__ == "__main__":
    tts_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mixxx-e2e/tts.log"
    driver = AxDriver(create_backend()).connect()
    run(driver, TtsLog(tts_path))

#!/usr/bin/env python3
"""
DDJ-400 software emulator
=========================

A developer/test tool that presents a *virtual MIDI port* to Mixxx and sends
the exact MIDI messages a real Pioneer DDJ-400 sends, so DDJ-400-driven
accessibility features (e.g. the spoken library menu) can be developed and
tested without physical hardware.

Mixxx uses PortMidi for MIDI input. On macOS, PortMidi can open virtual MIDI
ports (CoreMIDI virtual sources), which is exactly what this emulator creates:
a virtual MIDI *output* port that Mixxx sees as an *input* device.

Two front ends are provided:

  * Interactive keyboard mode (default) -- drive the emulator by hand.
  * Scripted mode (--script FILE)      -- replay a timed event sequence for
    automated regression tests.

The MIDI numbers below are verified against
`res/controllers/Pioneer-DDJ-400.midi.xml` on the `pi-arm64-build-and-updater`
branch. Do not change them.

Dependencies (install once):

    pip install mido python-rtmidi

The dependencies are imported lazily so that the script can still be parsed,
compiled, and dry-run (--dry-run) without them installed.
"""

import argparse
import select
import sys
import termios
import time
import tty

# ---------------------------------------------------------------------------
# DDJ-400 MIDI message definitions (verified from Pioneer-DDJ-400.midi.xml)
# ---------------------------------------------------------------------------
#
# Each control is described as (status_byte, data_byte, value_semantics).
# status_byte includes the MIDI channel nibble (e.g. 0x96 = Note On, ch 7).
#
#   BROWSE knob rotate : CC 0xB6/0x40, value 0x41 = up, 0x3F = down (relative)
#   BROWSE press       : Note 0x96/0x41, 0x7F down / 0x00 up
#   BROWSE + SHIFT     : Note 0x96/0x42, 0x7F down / 0x00 up
#   LOAD Deck1         : Note 0x96/0x46, 0x7F down / 0x00 up
#   LOAD Deck2         : Note 0x96/0x47, 0x7F down / 0x00 up
#   SHIFT Deck1        : Note 0x90/0x3F, 0x7F down / 0x00 up
#   SHIFT Deck2        : Note 0x91/0x3F, 0x7F down / 0x00 up

BROWSE_UP = (0xB6, 0x40, 0x41)      # CC, rotate up
BROWSE_DOWN = (0xB6, 0x40, 0x3F)    # CC, rotate down
BROWSE_PRESS = (0x96, 0x41)         # Note, BROWSE press
BROWSE_SHIFT_PRESS = (0x96, 0x42)   # Note, BROWSE + SHIFT press
LOAD_DECK1 = (0x96, 0x46)           # Note, LOAD Deck1
LOAD_DECK2 = (0x96, 0x47)           # Note, LOAD Deck2
SHIFT_DECK1 = (0x90, 0x3F)          # Note, SHIFT Deck1
SHIFT_DECK2 = (0x91, 0x3F)          # Note, SHIFT Deck2

NOTE_ON = 0x7F                      # value for "pressed"
NOTE_OFF = 0x00                     # value for "released"

# How long (seconds) a BROWSE press must be held before it is treated as a
# "hold to open the menu" gesture. Shorter presses are plain clicks.
HOLD_THRESHOLD = 0.4

# Default name of the virtual MIDI port. Mixxx lists it as an input device.
DEFAULT_PORT_NAME = "DDJ-400 Emulator"


# ---------------------------------------------------------------------------
# MIDI backend (lazy import so the module works without mido/rtmidi)
# ---------------------------------------------------------------------------
class MidiBackend:
    """Wraps mido + python-rtmidi and exposes a tiny send API.

    The real MIDI libraries are imported only when an instance is created, so
    `--dry-run` and the sequence parser work without them installed.
    """

    def __init__(self, port_name, dry_run=False, virtual=True):
        self.dry_run = dry_run
        self.port = None
        if dry_run:
            return
        try:
            import mido
        except ImportError as exc:  # pragma: no cover - depends on env
            raise SystemExit(
                "mido is not installed. Run:  pip install mido python-rtmidi\n"
                f"(original error: {exc})"
            ) from exc
        self._mido = mido
        try:
            # Force the rtmidi backend so we can create a *virtual* port.
            mido.set_backend("mido.backends.rtmidi")
            if virtual:
                self.port = mido.open_output(port_name, virtual=True)
            else:
                # Open an existing destination port (e.g. the IAC Driver bus)
                # instead of creating a new virtual one.
                self.port = mido.open_output(port_name)
        except Exception as exc:  # pragma: no cover - depends on env
            raise SystemExit(
                "Could not open a MIDI output port. On macOS make sure "
                "the IAC driver / CoreMIDI virtual ports are available.\n"
                f"(original error: {exc})"
            ) from exc

    def send(self, status, data, value):
        """Send a single 3-byte MIDI message."""
        if self.dry_run:
            print(f"  [dry-run] send  {status:02X} {data:02X} {value:02X}")
            return
        # Build a mido.Message from the raw status/data/value bytes. The status
        # byte's high nibble is the message type (0x9=note on, 0xB=control
        # change, ...) and the low nibble is the MIDI channel (0-based).
        msg_type = (status >> 4) & 0x0F
        channel = status & 0x0F
        mido = self._mido
        if msg_type == 0x9:
            msg = mido.Message("note_on", channel=channel, note=data, velocity=value)
        elif msg_type == 0x8:
            msg = mido.Message("note_off", channel=channel, note=data, velocity=value)
        elif msg_type == 0xB:
            msg = mido.Message("control_change", channel=channel, control=data, value=value)
        elif msg_type == 0xC:
            msg = mido.Message("program_change", channel=channel, program=data)
        else:
            # Fall back to a raw sysex-style message for anything else.
            msg = mido.Message.from_bytes([status, data, value])
        self.port.send(msg)

    def close(self):
        if self.port is not None:
            self.port.close()


# ---------------------------------------------------------------------------
# Emulator: turns high-level DDJ-400 actions into MIDI messages
# ---------------------------------------------------------------------------
class DDJ400Emulator:
    def __init__(self, backend):
        self.backend = backend

    # -- low-level helpers --------------------------------------------------
    def _note(self, note, value):
        """Send a Note On/Off. `note` is a (status, data) tuple."""
        status, data = note
        self.backend.send(status, data, value)

    def _note_down(self, note):
        self._note(note, NOTE_ON)

    def _note_up(self, note):
        self._note(note, NOTE_OFF)

    def _cc(self, status, data, value):
        self.backend.send(status, data, value)

    # -- public actions -----------------------------------------------------
    def browse_rotate(self, direction):
        """direction: +1 = up, -1 = down."""
        if direction > 0:
            self._cc(*BROWSE_UP)
        else:
            self._cc(*BROWSE_DOWN)

    def browse_press(self, hold_seconds=0.0):
        """Press BROWSE. hold_seconds is the time between down and up.

        A momentary press (hold_seconds == 0) sends down then immediately up.
        A held press keeps the note down for hold_seconds before releasing.
        """
        self._note_down(BROWSE_PRESS)
        if hold_seconds > 0:
            time.sleep(hold_seconds)
        self._note_up(BROWSE_PRESS)

    def browse_press_down(self):
        self._note_down(BROWSE_PRESS)

    def browse_press_up(self):
        self._note_up(BROWSE_PRESS)

    def browse_shift_press(self, hold_seconds=0.0):
        """Press BROWSE + SHIFT (the 'back' gesture)."""
        self._note_down(BROWSE_SHIFT_PRESS)
        if hold_seconds > 0:
            time.sleep(hold_seconds)
        self._note_up(BROWSE_SHIFT_PRESS)

    def load_deck1(self):
        self._note_down(LOAD_DECK1)
        self._note_up(LOAD_DECK1)

    def load_deck2(self):
        self._note_down(LOAD_DECK2)
        self._note_up(LOAD_DECK2)

    def shift_down(self):
        """SHIFT is a modifier: hold while other keys are pressed."""
        self._note_down(SHIFT_DECK1)
        self._note_down(SHIFT_DECK2)

    def shift_up(self):
        self._note_up(SHIFT_DECK1)
        self._note_up(SHIFT_DECK2)


# ---------------------------------------------------------------------------
# Interactive keyboard front end
# ---------------------------------------------------------------------------
HELP_TEXT = """
DDJ-400 Emulator -- interactive mode
====================================
Keys (press directly, no Enter needed):
  Up / Down        BROWSE rotate (up/down)
  Enter            BROWSE press (short click)
  L                LOAD Deck1
  R                LOAD Deck2
  S                SHIFT (momentary)
  H                show this help
  Q                quit

Hold-to-open: in a real terminal, hold Enter for > %(hold).2fs to send the
BROWSE press-down, then release to send press-up (the menu gesture). If your
terminal doesn't support raw key mode (e.g. piped input), fall back to the
line protocol: type a token (up/down/enter/l/r/s) and press Enter; a trailing
'+' means hold (e.g. 'enter+' then 'enter' to release).
""" % {"hold": HOLD_THRESHOLD}


def run_interactive(emu):
    """Read raw keypresses from the terminal and drive the emulator.

    Uses termios raw mode so the actual keys (arrow keys, Enter, L/R/S) work
    as described in HELP_TEXT, rather than requiring typed text tokens.
    """
    print(HELP_TEXT)
    held = {}  # token -> (kind, payload, down_time)

    # Map a raw key token to a DDJ-400 action.
    def action_for(token):
        if token == "up":
            return ("rotate", +1)
        if token == "down":
            return ("rotate", -1)
        if token == "enter":
            return ("note", BROWSE_PRESS)
        if token == "shift+enter":
            return ("note", BROWSE_SHIFT_PRESS)
        if token == "l":
            return ("load1", None)
        if token == "r":
            return ("load2", None)
        if token == "s":
            return ("shift", None)
        return None

    def read_key():
        """Read a single keypress from stdin, decoding arrow keys / Enter."""
        ch = sys.stdin.read(1)
        if not ch:
            return None
        if ch == "\x1b":  # ESC sequence (arrow keys)
            seq = sys.stdin.read(2)
            if seq == "[A":
                return "up"
            if seq == "[B":
                return "down"
            if seq == "[C":
                return "right"
            if seq == "[D":
                return "left"
            return None
        if ch == "\r" or ch == "\n":
            return "enter"
        return ch.lower()

    old = None
    try:
        # Put the terminal in raw mode so we get keypresses without Enter.
        old = termios.tcgetattr(sys.stdin.fileno())
        tty.setraw(sys.stdin.fileno())
    except (termios.error, AttributeError):
        # Not a TTY (e.g. piped input); fall back to line-based reading.
        old = None

    try:
        while True:
            if old is None:
                # Line-based fallback: read a whole line.
                line = sys.stdin.readline()
                if not line:
                    break
                token = line.strip().lower()
                if token in ("q", "quit", "exit"):
                    print("Quitting.")
                    break
                is_hold = token.endswith("+")
                token = token[:-1] if is_hold else token
                action = action_for(token)
                if action is None:
                    print(f"Unknown key: {token!r}  (H for help)")
                    continue
                kind, payload = action
                if is_hold:
                    _key_down(emu, held, token, kind, payload)
                else:
                    _key_up(emu, held, token, kind, payload)
                continue

            # Raw mode: read keypresses as they come.
            key = read_key()
            if key is None:
                continue
            if key == "q":
                print("Quitting.")
                break
            if key == "h":
                print(HELP_TEXT)
                continue
            action = action_for(key)
            if action is None:
                continue
            kind, payload = action
            if kind == "note":
                # Momentary press: down then immediately up (short click).
                emu.browse_press()
            elif kind == "rotate":
                emu.browse_rotate(payload)
            elif kind == "load1":
                emu.load_deck1()
            elif kind == "load2":
                emu.load_deck2()
            elif kind == "shift":
                # Shift is momentary in raw mode: press and release.
                emu.shift_down()
                emu.shift_up()
    finally:
        if old is not None:
            termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, old)


def _key_down(emu, held, token, kind, payload):
    """Handle a key-down event (line-based fallback)."""
    if kind == "rotate":
        emu.browse_rotate(payload)
    elif kind == "note":
        emu.browse_press_down()
        held[token] = (kind, payload, time.time())
    elif kind == "load1":
        emu.load_deck1()
    elif kind == "load2":
        emu.load_deck2()
    elif kind == "shift":
        emu.shift_down()
        held[token] = (kind, payload, time.time())


def _key_up(emu, held, token, kind, payload):
    """Handle a key-up event (line-based fallback)."""
    if kind == "rotate":
        emu.browse_rotate(payload)
    elif kind == "note":
        if token in held:
            _, note, down = held.pop(token)
            held_for = time.time() - down
            emu.browse_press_up()
            if held_for >= HOLD_THRESHOLD:
                print(f"  (BROWSE held {held_for:.2f}s -> menu gesture)")
        else:
            emu.browse_press()
    elif kind == "load1":
        emu.load_deck1()
    elif kind == "load2":
        emu.load_deck2()
    elif kind == "shift":
        if token in held:
            held.pop(token)
        emu.shift_up()


# ---------------------------------------------------------------------------
# Scripted mode
# ---------------------------------------------------------------------------
# Sequence file format: one command per line, blank lines and '#' comments
# ignored. Durations are in seconds.
#
#   rotate up
#   rotate down
#   press browse [hold=SECONDS]      # BROWSE press; optional hold duration
#   press browse+shift [hold=SECONDS]
#   press load1
#   press load2
#   shift down
#   shift up
#   sleep SECONDS                    # pause between events
#
# Example (hold-to-open the menu, scroll, activate):
#   press browse hold=0.8     # hold BROWSE to open the spoken menu
#   sleep 0.5
#   rotate down               # scroll down one item
#   sleep 0.2
#   press browse              # activate the focused item
#   shift up                  # release shift if it was held

COMMANDS = {
    "rotate": lambda emu, args: emu.browse_rotate(1 if args[0] == "up" else -1),
    "press": None,  # handled specially (needs hold parsing)
    "shift": None,  # handled specially (down/up)
    "sleep": lambda emu, args: time.sleep(float(args[0])),
}


def parse_sequence(text):
    """Parse sequence text into a list of (command, args) tuples.

    Raises ValueError on malformed lines. This is pure logic (no MIDI), so it
    can be unit-tested / dry-run without a MIDI device.
    """
    events = []
    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        cmd = parts[0].lower()
        args = parts[1:]

        if cmd == "rotate":
            if not args or args[0] not in ("up", "down"):
                raise ValueError(
                    f"line {lineno}: rotate needs 'up' or 'down'"
                )
            events.append(("rotate", args[0]))
        elif cmd == "press":
            if not args:
                raise ValueError(f"line {lineno}: press needs a target")
            target = args[0].lower()
            if target not in ("browse", "browse+shift", "load1", "load2"):
                raise ValueError(
                    f"line {lineno}: unknown press target {target!r}"
                )
            hold = 0.0
            for a in args[1:]:
                if a.startswith("hold="):
                    hold = float(a.split("=", 1)[1])
            events.append(("press", (target, hold)))
        elif cmd == "shift":
            if not args or args[0] not in ("down", "up"):
                raise ValueError(
                    f"line {lineno}: shift needs 'down' or 'up'"
                )
            events.append(("shift", args[0]))
        elif cmd == "sleep":
            try:
                events.append(("sleep", float(args[0])))
            except (IndexError, ValueError):
                raise ValueError(
                    f"line {lineno}: sleep needs a duration in seconds"
                ) from None
        else:
            raise ValueError(f"line {lineno}: unknown command {cmd!r}")
    return events


def run_script(emu, path):
    """Replay a sequence file through the emulator."""
    with open(path, "r", encoding="utf-8") as fh:
        events = parse_sequence(fh.read())

    print(f"Replaying {len(events)} events from {path}")
    for cmd, args in events:
        if cmd == "rotate":
            print(f"  rotate {args}")
            emu.browse_rotate(1 if args == "up" else -1)
        elif cmd == "press":
            target, hold = args
            print(f"  press {target} hold={hold:.2f}")
            if target == "browse":
                emu.browse_press(hold)
            elif target == "browse+shift":
                emu.browse_shift_press(hold)
            elif target == "load1":
                emu.load_deck1()
            elif target == "load2":
                emu.load_deck2()
        elif cmd == "shift":
            print(f"  shift {args}")
            if args == "down":
                emu.shift_down()
            else:
                emu.shift_up()
        elif cmd == "sleep":
            print(f"  sleep {args:.2f}")
            time.sleep(args)
    print("Sequence complete.")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Software DDJ-400 emulator for Mixxx accessibility testing."
    )
    parser.add_argument(
        "--port",
        default=DEFAULT_PORT_NAME,
        help="MIDI port name (default: %(default)s)",
    )
    parser.add_argument(
        "--virtual",
        action="store_true",
        help="Create a new virtual MIDI port (default). Use --no-virtual to "
        "send to an existing port such as the IAC Driver bus instead.",
    )
    parser.add_argument(
        "--no-virtual",
        dest="virtual",
        action="store_false",
        help="Send to an existing MIDI port (e.g. 'IAC Driver Bus 1') instead "
        "of creating a new virtual one.",
    )
    parser.set_defaults(virtual=True)
    parser.add_argument(
        "--script",
        metavar="FILE",
        help="Replay a scripted event sequence instead of interactive mode",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Do not open a real MIDI port; print messages instead. "
        "Useful for testing without mido/rtmidi or a virtual driver.",
    )
    args = parser.parse_args(argv)

    backend = MidiBackend(args.port, dry_run=args.dry_run, virtual=args.virtual)
    emu = DDJ400Emulator(backend)

    try:
        if args.script:
            run_script(emu, args.script)
        else:
            run_interactive(emu)
    except KeyboardInterrupt:
        print("\nInterrupted.")
    finally:
        backend.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())

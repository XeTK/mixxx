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

The emulator covers the full DDJ-400 control set: browse/load/shift, deck
transport (play, cue, beat sync, loops, reloop, headphone cue), deck CC knobs
and faders (tempo, trim, EQ, channel), the shared filter and mixer (crossfader,
headphone mix/level), the beat FX section, and the hot-cue pads and pad-mode
buttons.

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

# Deck transport / mixer / effects / pads (verified from Pioneer-DDJ-400.midi.xml)
#
# Note On = 0x7F down / 0x00 up. CC values are absolute (0-127) unless noted.
#
# DECK 1 (status 0x90) and DECK 2 (status 0x91):
PLAY_DECK1 = (0x90, 0x0B)           # Note, PLAY/PAUSE
PLAY_DECK2 = (0x91, 0x0B)
CUE_DECK1 = (0x90, 0x0C)            # Note, CUE
CUE_DECK2 = (0x91, 0x0C)
SYNC_DECK1 = (0x90, 0x58)           # Note, BEAT SYNC
SYNC_DECK2 = (0x91, 0x58)
LOOP_IN_DECK1 = (0x90, 0x10)        # Note, LOOP IN / 4 BEAT
LOOP_IN_DECK2 = (0x91, 0x10)
LOOP_OUT_DECK1 = (0x90, 0x11)       # Note, LOOP OUT
LOOP_OUT_DECK2 = (0x91, 0x11)
RELOOP_DECK1 = (0x90, 0x4D)         # Note, RELOOP / EXIT
RELOOP_DECK2 = (0x91, 0x4D)
PFL_DECK1 = (0x90, 0x54)            # Note, CUE channel (headphone cue)
PFL_DECK2 = (0x91, 0x54)
TEMPO_DECK1 = (0xB0, 0x00)          # CC, TEMPO fader (0x40 = center)
TEMPO_DECK2 = (0xB1, 0x00)
TRIM_DECK1 = (0xB0, 0x04)           # CC, TRIM knob (0x40 = center)
TRIM_DECK2 = (0xB1, 0x04)
EQ_HI_DECK1 = (0xB0, 0x07)          # CC, EQ HI
EQ_HI_DECK2 = (0xB1, 0x07)
EQ_MID_DECK1 = (0xB0, 0x0B)         # CC, EQ MID
EQ_MID_DECK2 = (0xB1, 0x0B)
EQ_LOW_DECK1 = (0xB0, 0x0F)         # CC, EQ LOW
EQ_LOW_DECK2 = (0xB1, 0x0F)
CHANNEL_DECK1 = (0xB0, 0x13)        # CC, CHANNEL fader
CHANNEL_DECK2 = (0xB1, 0x13)
#
# FILTER (shared, status 0xB6):
FILTER_CH1 = (0xB6, 0x17)           # CC, FILTER CH1
FILTER_CH2 = (0xB6, 0x18)           # CC, FILTER CH2
#
# MIXER (status 0xB6):
CROSSFADER = (0xB6, 0x1F)           # CC, CROSSFADER (0x40 = center)
HEAD_MIX = (0xB6, 0x0C)             # CC, HEADPHONES MIXING
HEAD_GAIN = (0xB6, 0x0D)            # CC, HEADPHONES LEVEL
#
# EFFECTS (status 0x94):
BEAT_LEFT = (0x94, 0x4A)            # Note, BEAT LEFT
BEAT_RIGHT = (0x94, 0x4B)           # Note, BEAT RIGHT
BEAT_FX_ONOFF = (0x94, 0x47)        # Note, BEAT FX ON/OFF
#
# PADS (deck1 status 0x97, deck2 status 0x99) - HOT CUE mode:
#   data byte = pad index 0-7 (pad 1 -> 0x00, pad 8 -> 0x07)
PAD_DECK1 = 0x97
PAD_DECK2 = 0x99
#
# PAD MODE buttons (deck1 status 0x90, deck2 status 0x91):
PAD_MODE_HOTCUE_DECK1 = (0x90, 0x1B)    # Note, HOT CUE MODE
PAD_MODE_HOTCUE_DECK2 = (0x91, 0x1B)
PAD_MODE_BEATLOOP_DECK1 = (0x90, 0x6D)  # Note, BEAT LOOP MODE
PAD_MODE_BEATLOOP_DECK2 = (0x91, 0x6D)

NOTE_ON = 0x7F                      # value for "pressed"
NOTE_OFF = 0x00                     # value for "released"

# CC knob/fader step used by the interactive front end: an absolute value
# offset from center (0x40). Tempo/EQ keys step the control away from center.
CC_CENTER = 0x40
CC_STEP = 0x10

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

    def _note_press(self, note, hold_seconds=0.0):
        """Momentary note press: down, then up (optionally held)."""
        self._note_down(note)
        if hold_seconds > 0:
            time.sleep(hold_seconds)
        self._note_up(note)

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

    # -- deck transport (momentary notes) -----------------------------------
    def play(self, deck):
        self._note_press(PLAY_DECK1 if deck == 1 else PLAY_DECK2)

    def cue(self, deck):
        self._note_press(CUE_DECK1 if deck == 1 else CUE_DECK2)

    def sync(self, deck):
        self._note_press(SYNC_DECK1 if deck == 1 else SYNC_DECK2)

    def loop_in(self, deck):
        self._note_press(LOOP_IN_DECK1 if deck == 1 else LOOP_IN_DECK2)

    def loop_out(self, deck):
        self._note_press(LOOP_OUT_DECK1 if deck == 1 else LOOP_OUT_DECK2)

    def reloop(self, deck):
        self._note_press(RELOOP_DECK1 if deck == 1 else RELOOP_DECK2)

    def pfl(self, deck):
        self._note_press(PFL_DECK1 if deck == 1 else PFL_DECK2)

    # -- deck CC knobs / faders (absolute value 0-127) ----------------------
    def tempo(self, deck, value):
        self._cc(*(TEMPO_DECK1 if deck == 1 else TEMPO_DECK2), value)

    def trim(self, deck, value):
        self._cc(*(TRIM_DECK1 if deck == 1 else TRIM_DECK2), value)

    def eq(self, deck, band, value):
        """band: 'hi', 'mid' or 'low'."""
        if band == "hi":
            cc = EQ_HI_DECK1 if deck == 1 else EQ_HI_DECK2
        elif band == "mid":
            cc = EQ_MID_DECK1 if deck == 1 else EQ_MID_DECK2
        else:
            cc = EQ_LOW_DECK1 if deck == 1 else EQ_LOW_DECK2
        self._cc(*cc, value)

    def channel_fader(self, deck, value):
        self._cc(*(CHANNEL_DECK1 if deck == 1 else CHANNEL_DECK2), value)

    # -- filter / mixer (shared CC) ----------------------------------------
    def filter(self, channel, value):
        self._cc(*(FILTER_CH1 if channel == 1 else FILTER_CH2), value)

    def crossfader(self, value):
        self._cc(*CROSSFADER, value)

    def head_mix(self, value):
        self._cc(*HEAD_MIX, value)

    def head_gain(self, value):
        self._cc(*HEAD_GAIN, value)

    # -- effects (momentary notes) ------------------------------------------
    def beat_left(self):
        self._note_press(BEAT_LEFT)

    def beat_right(self):
        self._note_press(BEAT_RIGHT)

    def beat_fx_onoff(self):
        self._note_press(BEAT_FX_ONOFF)

    # -- pads (momentary notes) ---------------------------------------------
    def pad(self, deck, pad_num):
        """Hot-cue pad. pad_num is 1-8; sent as data byte 0-7."""
        status = PAD_DECK1 if deck == 1 else PAD_DECK2
        self._note_press((status, pad_num - 1))

    def pad_mode(self, deck, mode):
        """mode: 'hotcue' or 'beatloop'."""
        if deck == 1:
            note = PAD_MODE_HOTCUE_DECK1 if mode == "hotcue" else PAD_MODE_BEATLOOP_DECK1
        else:
            note = PAD_MODE_HOTCUE_DECK2 if mode == "hotcue" else PAD_MODE_BEATLOOP_DECK2
        self._note_press(note)


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

Deck transport (Deck1 / Deck2):
  P / O            PLAY / PAUSE
  C / V            CUE
  Y / U            BEAT SYNC
  I / K            LOOP IN / 4 BEAT
  J / N            LOOP OUT
  B / M            RELOOP / EXIT
  F / G            CUE channel (headphone cue)

CC knobs / faders (absolute value, stepped from center):
  [ / ]            TEMPO Deck1 down / up
  { / }            TEMPO Deck2 down / up
  - / =            EQ Deck1 down / up
  _ / +            EQ Deck2 down / up

  H                show this help
  Q                quit

Hold-to-open: in a real terminal, hold Enter for > %(hold).2fs to send the
BROWSE press-down, then release to send press-up (the menu gesture). If your
terminal doesn't support raw key mode (e.g. piped input), fall back to the
line protocol: type a token (up/down/enter/l/r/s) and press Enter; a trailing
'+' means hold (e.g. 'enter+' then 'enter' to release).
""" % {"hold": HOLD_THRESHOLD}


# Map an action kind to the emulator call that performs it. Used by both the
# raw-key and line-based front ends for momentary (press-and-release) actions.
MOMENTARY_ACTIONS = {
    "load1": lambda emu: emu.load_deck1(),
    "load2": lambda emu: emu.load_deck2(),
    "play1": lambda emu: emu.play(1),
    "play2": lambda emu: emu.play(2),
    "cue1": lambda emu: emu.cue(1),
    "cue2": lambda emu: emu.cue(2),
    "sync1": lambda emu: emu.sync(1),
    "sync2": lambda emu: emu.sync(2),
    "loopin1": lambda emu: emu.loop_in(1),
    "loopin2": lambda emu: emu.loop_in(2),
    "loopout1": lambda emu: emu.loop_out(1),
    "loopout2": lambda emu: emu.loop_out(2),
    "reloop1": lambda emu: emu.reloop(1),
    "reloop2": lambda emu: emu.reloop(2),
    "pfl1": lambda emu: emu.pfl(1),
    "pfl2": lambda emu: emu.pfl(2),
    "tempo1down": lambda emu: emu.tempo(1, CC_CENTER - CC_STEP),
    "tempo1up": lambda emu: emu.tempo(1, CC_CENTER + CC_STEP),
    "tempo2down": lambda emu: emu.tempo(2, CC_CENTER - CC_STEP),
    "tempo2up": lambda emu: emu.tempo(2, CC_CENTER + CC_STEP),
    "eq1down": lambda emu: emu.eq(1, "hi", CC_CENTER - CC_STEP),
    "eq1up": lambda emu: emu.eq(1, "hi", CC_CENTER + CC_STEP),
    "eq2down": lambda emu: emu.eq(2, "hi", CC_CENTER - CC_STEP),
    "eq2up": lambda emu: emu.eq(2, "hi", CC_CENTER + CC_STEP),
}


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
        # Deck transport (Deck1 / Deck2)
        if token == "p":
            return ("play1", None)
        if token == "o":
            return ("play2", None)
        if token == "c":
            return ("cue1", None)
        if token == "v":
            return ("cue2", None)
        if token == "y":
            return ("sync1", None)
        if token == "u":
            return ("sync2", None)
        if token == "i":
            return ("loopin1", None)
        if token == "k":
            return ("loopin2", None)
        if token == "j":
            return ("loopout1", None)
        if token == "n":
            return ("loopout2", None)
        if token == "b":
            return ("reloop1", None)
        if token == "m":
            return ("reloop2", None)
        if token == "f":
            return ("pfl1", None)
        if token == "g":
            return ("pfl2", None)
        # CC knobs / faders (stepped from center)
        if token == "[":
            return ("tempo1down", None)
        if token == "]":
            return ("tempo1up", None)
        if token == "{":
            return ("tempo2down", None)
        if token == "}":
            return ("tempo2up", None)
        if token == "-":
            return ("eq1down", None)
        if token == "=":
            return ("eq1up", None)
        if token == "_":
            return ("eq2down", None)
        if token == "+":
            return ("eq2up", None)
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
            elif kind == "shift":
                # Shift is momentary in raw mode: press and release.
                emu.shift_down()
                emu.shift_up()
            else:
                MOMENTARY_ACTIONS[kind](emu)
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
    elif kind == "shift":
        emu.shift_down()
        held[token] = (kind, payload, time.time())
    elif kind in MOMENTARY_ACTIONS:
        MOMENTARY_ACTIONS[kind](emu)


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
    elif kind == "shift":
        if token in held:
            held.pop(token)
        emu.shift_up()
    elif kind in MOMENTARY_ACTIONS:
        MOMENTARY_ACTIONS[kind](emu)


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
# Deck transport (deck 1|2):
#   play 1|2
#   cue 1|2
#   sync 1|2
#   loop_in 1|2
#   loop_out 1|2
#   reloop 1|2
#   pfl 1|2
#
# CC knobs / faders (value 0-127):
#   tempo 1|2 VALUE
#   trim 1|2 VALUE
#   eq 1|2 hi|mid|low VALUE
#   channel 1|2 VALUE
#   filter 1|2 VALUE
#   crossfader VALUE
#   headmix VALUE
#   headgain VALUE
#
# Effects / pads:
#   beatleft
#   beatright
#   beatfx
#   pad 1|2 N                        # hot-cue pad 1-8
#   padmode 1|2 hotcue|beatloop
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


def _parse_deck(lineno, args, cmd):
    """Validate and return a deck/channel number (1 or 2) from args[0]."""
    if not args or args[0] not in ("1", "2"):
        raise ValueError(f"line {lineno}: {cmd} needs '1' or '2'")
    return int(args[0])


def _parse_value(lineno, args, cmd):
    """Validate and return a 0-127 CC value from args[0]."""
    try:
        value = int(args[0])
    except (IndexError, ValueError):
        raise ValueError(f"line {lineno}: {cmd} needs a value 0-127") from None
    if not 0 <= value <= 127:
        raise ValueError(f"line {lineno}: {cmd} value must be 0-127")
    return value


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
        elif cmd in ("play", "cue", "sync", "loop_in", "loop_out", "reloop", "pfl"):
            deck = _parse_deck(lineno, args, cmd)
            events.append((cmd, deck))
        elif cmd in ("tempo", "trim", "channel"):
            deck = _parse_deck(lineno, args, cmd)
            value = _parse_value(lineno, args[1:], cmd)
            events.append((cmd, (deck, value)))
        elif cmd == "eq":
            if len(args) < 3:
                raise ValueError(f"line {lineno}: eq needs deck, band, value")
            deck = _parse_deck(lineno, args, cmd)
            band = args[1].lower()
            if band not in ("hi", "mid", "low"):
                raise ValueError(
                    f"line {lineno}: eq band must be 'hi', 'mid' or 'low'"
                )
            value = _parse_value(lineno, args[2:], cmd)
            events.append(("eq", (deck, band, value)))
        elif cmd == "filter":
            channel = _parse_deck(lineno, args, cmd)
            value = _parse_value(lineno, args[1:], cmd)
            events.append(("filter", (channel, value)))
        elif cmd == "crossfader":
            events.append(("crossfader", _parse_value(lineno, args, cmd)))
        elif cmd == "headmix":
            events.append(("headmix", _parse_value(lineno, args, cmd)))
        elif cmd == "headgain":
            events.append(("headgain", _parse_value(lineno, args, cmd)))
        elif cmd in ("beatleft", "beatright", "beatfx"):
            events.append((cmd, None))
        elif cmd == "pad":
            deck = _parse_deck(lineno, args, cmd)
            try:
                pad_num = int(args[1])
            except (IndexError, ValueError):
                raise ValueError(
                    f"line {lineno}: pad needs deck and pad number 1-8"
                ) from None
            if not 1 <= pad_num <= 8:
                raise ValueError(f"line {lineno}: pad number must be 1-8")
            events.append(("pad", (deck, pad_num)))
        elif cmd == "padmode":
            deck = _parse_deck(lineno, args, cmd)
            if len(args) < 2 or args[1].lower() not in ("hotcue", "beatloop"):
                raise ValueError(
                    f"line {lineno}: padmode needs deck and 'hotcue'|'beatloop'"
                )
            events.append(("padmode", (deck, args[1].lower())))
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
        elif cmd in ("play", "cue", "sync", "loop_in", "loop_out", "reloop", "pfl"):
            deck = args
            print(f"  {cmd} deck{deck}")
            method = {
                "play": emu.play,
                "cue": emu.cue,
                "sync": emu.sync,
                "loop_in": emu.loop_in,
                "loop_out": emu.loop_out,
                "reloop": emu.reloop,
                "pfl": emu.pfl,
            }[cmd]
            method(deck)
        elif cmd in ("tempo", "trim", "channel"):
            deck, value = args
            print(f"  {cmd} deck{deck} {value}")
            method = {
                "tempo": emu.tempo,
                "trim": emu.trim,
                "channel": emu.channel_fader,
            }[cmd]
            method(deck, value)
        elif cmd == "eq":
            deck, band, value = args
            print(f"  eq deck{deck} {band} {value}")
            emu.eq(deck, band, value)
        elif cmd == "filter":
            channel, value = args
            print(f"  filter ch{channel} {value}")
            emu.filter(channel, value)
        elif cmd == "crossfader":
            print(f"  crossfader {args}")
            emu.crossfader(args)
        elif cmd == "headmix":
            print(f"  headmix {args}")
            emu.head_mix(args)
        elif cmd == "headgain":
            print(f"  headgain {args}")
            emu.head_gain(args)
        elif cmd == "beatleft":
            print("  beatleft")
            emu.beat_left()
        elif cmd == "beatright":
            print("  beatright")
            emu.beat_right()
        elif cmd == "beatfx":
            print("  beatfx")
            emu.beat_fx_onoff()
        elif cmd == "pad":
            deck, pad_num = args
            print(f"  pad deck{deck} {pad_num}")
            emu.pad(deck, pad_num)
        elif cmd == "padmode":
            deck, mode = args
            print(f"  padmode deck{deck} {mode}")
            emu.pad_mode(deck, mode)
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

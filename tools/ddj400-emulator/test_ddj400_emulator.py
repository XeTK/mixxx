#!/usr/bin/env python3
"""Unit tests for the DDJ-400 emulator's sequence parser and MIDI output.

The emulator is designed to be unit-testable without a MIDI device: the
sequence parser is pure logic, and the MIDI backend has a dry-run mode. These
tests use a small recording backend (a subclass of MidiBackend) that captures
the (status, data, value) tuples the emulator would send, instead of printing
them or talking to real MIDI hardware.
"""

import time
import unittest

import ddj400_emulator


class RecordingBackend(ddj400_emulator.MidiBackend):
    """A dry-run backend that records every send() call."""

    def __init__(self):
        super().__init__("test", dry_run=True)
        self.messages = []  # list of (status, data, value, timestamp)

    def send(self, status, data, value):
        self.messages.append((status, data, value, time.monotonic()))

    def sent(self):
        """The recorded messages, without timestamps."""
        return [(s, d, v) for (s, d, v, _t) in self.messages]


class ParseSequenceTest(unittest.TestCase):
    """Tests for the pure parse_sequence() function."""

    def test_parses_valid_lines(self):
        text = (
            "rotate up\n"
            "rotate down\n"
            "press browse\n"
            "press browse+shift\n"
            "press load1\n"
            "press load2\n"
            "sleep 0.5\n"
            "shift down\n"
            "shift up\n"
        )
        events = ddj400_emulator.parse_sequence(text)
        self.assertEqual(
            events,
            [
                ("rotate", "up"),
                ("rotate", "down"),
                ("press", ("browse", 0.0)),
                ("press", ("browse+shift", 0.0)),
                ("press", ("load1", 0.0)),
                ("press", ("load2", 0.0)),
                ("sleep", 0.5),
                ("shift", "down"),
                ("shift", "up"),
            ],
        )

    def test_parses_hold_duration(self):
        events = ddj400_emulator.parse_sequence("press browse hold=0.8\n")
        self.assertEqual(events, [("press", ("browse", 0.8))])

    def test_hold_is_float(self):
        events = ddj400_emulator.parse_sequence("press load1 hold=1.5\n")
        self.assertEqual(events, [("press", ("load1", 1.5))])

    def test_unknown_command_raises(self):
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("frobnicate\n")

    def test_rotate_missing_target_raises(self):
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("rotate\n")

    def test_rotate_bad_target_raises(self):
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("rotate sideways\n")

    def test_press_missing_target_raises(self):
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("press\n")

    def test_press_unknown_target_raises(self):
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("press start\n")

    def test_bad_hold_value_raises(self):
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("press browse hold=abc\n")

    def test_sleep_missing_duration_raises(self):
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("sleep\n")

    def test_sleep_bad_duration_raises(self):
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("sleep fast\n")

    def test_shift_missing_direction_raises(self):
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("shift\n")

    def test_shift_bad_direction_raises(self):
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("shift left\n")

    def test_blank_lines_and_comments_ignored(self):
        text = (
            "# a comment\n"
            "\n"
            "   \n"
            "rotate up   # trailing comment is not stripped, but rotate is fine\n"
            "\n"
            "# another comment\n"
        )
        events = ddj400_emulator.parse_sequence(text)
        self.assertEqual(events, [("rotate", "up")])

    def test_empty_input_yields_no_events(self):
        self.assertEqual(ddj400_emulator.parse_sequence(""), [])
        self.assertEqual(ddj400_emulator.parse_sequence("# only a comment\n"), [])

    def test_error_reports_line_number(self):
        with self.assertRaises(ValueError) as ctx:
            ddj400_emulator.parse_sequence("rotate up\nbogus\n")
        self.assertIn("line 2", str(ctx.exception))


class EmulatorMidiTest(unittest.TestCase):
    """Tests that the emulator sends the correct MIDI messages via a dry-run
    recording backend."""

    def setUp(self):
        self.backend = RecordingBackend()
        self.emu = ddj400_emulator.DDJ400Emulator(self.backend)

    def test_browse_rotate_up(self):
        self.emu.browse_rotate(1)
        self.assertEqual(self.backend.sent(), [(0xB6, 0x40, 0x41)])

    def test_browse_rotate_down(self):
        self.emu.browse_rotate(-1)
        self.assertEqual(self.backend.sent(), [(0xB6, 0x40, 0x3F)])

    def test_browse_press(self):
        self.emu.browse_press()
        self.assertEqual(
            self.backend.sent(),
            [(0x96, 0x41, 0x7F), (0x96, 0x41, 0x00)],
        )

    def test_browse_shift_press(self):
        self.emu.browse_shift_press()
        self.assertEqual(
            self.backend.sent(),
            [(0x96, 0x42, 0x7F), (0x96, 0x42, 0x00)],
        )

    def test_load_deck1(self):
        self.emu.load_deck1()
        self.assertEqual(
            self.backend.sent(),
            [(0x96, 0x46, 0x7F), (0x96, 0x46, 0x00)],
        )

    def test_load_deck2(self):
        self.emu.load_deck2()
        self.assertEqual(
            self.backend.sent(),
            [(0x96, 0x47, 0x7F), (0x96, 0x47, 0x00)],
        )

    def test_shift_down(self):
        self.emu.shift_down()
        self.assertEqual(
            self.backend.sent(),
            [(0x90, 0x3F, 0x7F), (0x91, 0x3F, 0x7F)],
        )

    def test_shift_up(self):
        self.emu.shift_up()
        self.assertEqual(
            self.backend.sent(),
            [(0x90, 0x3F, 0x00), (0x91, 0x3F, 0x00)],
        )

    def test_browse_press_hold_keeps_note_down(self):
        """A held press must send down, wait, then up -- in that order."""
        self.emu.browse_press(hold_seconds=0.8)

        sent = self.backend.sent()
        self.assertEqual(
            sent,
            [(0x96, 0x41, 0x7F), (0x96, 0x41, 0x00)],
        )

        # Verify the note stayed down for ~the hold duration by checking the
        # recorded timestamps, with generous tolerance (no exact wall-clock
        # timing is required).
        down_t, up_t = self.backend.messages[0][3], self.backend.messages[1][3]
        self.assertGreaterEqual(up_t - down_t, 0.8 - 0.1)


if __name__ == "__main__":
    unittest.main()

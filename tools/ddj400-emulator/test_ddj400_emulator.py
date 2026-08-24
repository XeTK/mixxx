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

    # -- new deck transport / CC / effects / pad tokens ---------------------

    def test_parses_deck_transport_tokens(self):
        text = (
            "play 1\n"
            "play 2\n"
            "cue 1\n"
            "sync 2\n"
            "loop_in 1\n"
            "loop_out 2\n"
            "reloop 1\n"
            "pfl 2\n"
        )
        events = ddj400_emulator.parse_sequence(text)
        self.assertEqual(
            events,
            [
                ("play", 1),
                ("play", 2),
                ("cue", 1),
                ("sync", 2),
                ("loop_in", 1),
                ("loop_out", 2),
                ("reloop", 1),
                ("pfl", 2),
            ],
        )

    def test_parses_cc_tokens(self):
        text = (
            "tempo 1 64\n"
            "trim 2 32\n"
            "eq 1 hi 96\n"
            "eq 2 mid 0\n"
            "eq 1 low 127\n"
            "channel 2 100\n"
            "filter 1 50\n"
            "crossfader 64\n"
            "headmix 30\n"
            "headgain 90\n"
        )
        events = ddj400_emulator.parse_sequence(text)
        self.assertEqual(
            events,
            [
                ("tempo", (1, 64)),
                ("trim", (2, 32)),
                ("eq", (1, "hi", 96)),
                ("eq", (2, "mid", 0)),
                ("eq", (1, "low", 127)),
                ("channel", (2, 100)),
                ("filter", (1, 50)),
                ("crossfader", 64),
                ("headmix", 30),
                ("headgain", 90),
            ],
        )

    def test_parses_effects_and_pad_tokens(self):
        text = (
            "beatleft\n"
            "beatright\n"
            "beatfx\n"
            "pad 1 1\n"
            "pad 2 8\n"
            "padmode 1 hotcue\n"
            "padmode 2 beatloop\n"
        )
        events = ddj400_emulator.parse_sequence(text)
        self.assertEqual(
            events,
            [
                ("beatleft", None),
                ("beatright", None),
                ("beatfx", None),
                ("pad", (1, 1, False)),
                ("pad", (2, 8, False)),
                ("padmode", (1, "hotcue")),
                ("padmode", (2, "beatloop")),
            ],
        )

    def test_parses_shift_pad_token(self):
        events = ddj400_emulator.parse_sequence("pad 1 3 shift\npad 2 6 shift\n")
        self.assertEqual(
            events,
            [
                ("pad", (1, 3, True)),
                ("pad", (2, 6, True)),
            ],
        )

    def test_pad_bad_shift_token_raises(self):
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("pad 1 3 sift\n")

    def test_deck_transport_bad_deck_raises(self):
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("play 3\n")
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("cue\n")

    def test_cc_bad_value_raises(self):
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("tempo 1 128\n")
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("tempo 1 -1\n")
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("tempo 1 abc\n")
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("crossfader 200\n")

    def test_eq_bad_band_raises(self):
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("eq 1 bass 64\n")

    def test_pad_bad_number_raises(self):
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("pad 1 0\n")
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("pad 1 9\n")

    def test_padmode_bad_mode_raises(self):
        with self.assertRaises(ValueError):
            ddj400_emulator.parse_sequence("padmode 1 sampler\n")


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

    # -- new deck transport -------------------------------------------------

    def test_play_deck1(self):
        self.emu.play(1)
        self.assertEqual(
            self.backend.sent(),
            [(0x90, 0x0B, 0x7F), (0x90, 0x0B, 0x00)],
        )

    def test_play_deck2(self):
        self.emu.play(2)
        self.assertEqual(
            self.backend.sent(),
            [(0x91, 0x0B, 0x7F), (0x91, 0x0B, 0x00)],
        )

    def test_cue_deck1(self):
        self.emu.cue(1)
        self.assertEqual(
            self.backend.sent(),
            [(0x90, 0x0C, 0x7F), (0x90, 0x0C, 0x00)],
        )

    def test_sync_deck2(self):
        self.emu.sync(2)
        self.assertEqual(
            self.backend.sent(),
            [(0x91, 0x58, 0x7F), (0x91, 0x58, 0x00)],
        )

    def test_loop_in_deck1(self):
        self.emu.loop_in(1)
        self.assertEqual(
            self.backend.sent(),
            [(0x90, 0x10, 0x7F), (0x90, 0x10, 0x00)],
        )

    def test_loop_out_deck2(self):
        self.emu.loop_out(2)
        self.assertEqual(
            self.backend.sent(),
            [(0x91, 0x11, 0x7F), (0x91, 0x11, 0x00)],
        )

    def test_reloop_deck1(self):
        self.emu.reloop(1)
        self.assertEqual(
            self.backend.sent(),
            [(0x90, 0x4D, 0x7F), (0x90, 0x4D, 0x00)],
        )

    def test_pfl_deck2(self):
        self.emu.pfl(2)
        self.assertEqual(
            self.backend.sent(),
            [(0x91, 0x54, 0x7F), (0x91, 0x54, 0x00)],
        )

    # -- new CC knobs / faders ----------------------------------------------

    def test_tempo_deck1(self):
        self.emu.tempo(1, 64)
        self.assertEqual(self.backend.sent(), [(0xB0, 0x00, 64)])

    def test_tempo_deck2(self):
        self.emu.tempo(2, 32)
        self.assertEqual(self.backend.sent(), [(0xB1, 0x00, 32)])

    def test_trim_deck1(self):
        self.emu.trim(1, 96)
        self.assertEqual(self.backend.sent(), [(0xB0, 0x04, 96)])

    def test_eq_hi_deck1(self):
        self.emu.eq(1, "hi", 80)
        self.assertEqual(self.backend.sent(), [(0xB0, 0x07, 80)])

    def test_eq_mid_deck2(self):
        self.emu.eq(2, "mid", 20)
        self.assertEqual(self.backend.sent(), [(0xB1, 0x0B, 20)])

    def test_eq_low_deck1(self):
        self.emu.eq(1, "low", 127)
        self.assertEqual(self.backend.sent(), [(0xB0, 0x0F, 127)])

    def test_channel_fader_deck2(self):
        self.emu.channel_fader(2, 100)
        self.assertEqual(self.backend.sent(), [(0xB1, 0x13, 100)])

    # -- filter / mixer -----------------------------------------------------

    def test_filter_ch1(self):
        self.emu.filter(1, 50)
        self.assertEqual(self.backend.sent(), [(0xB6, 0x17, 50)])

    def test_filter_ch2(self):
        self.emu.filter(2, 60)
        self.assertEqual(self.backend.sent(), [(0xB6, 0x18, 60)])

    def test_crossfader(self):
        self.emu.crossfader(64)
        self.assertEqual(self.backend.sent(), [(0xB6, 0x1F, 64)])

    def test_head_mix(self):
        self.emu.head_mix(30)
        self.assertEqual(self.backend.sent(), [(0xB6, 0x0C, 30)])

    def test_head_gain(self):
        self.emu.head_gain(90)
        self.assertEqual(self.backend.sent(), [(0xB6, 0x0D, 90)])

    # -- effects ------------------------------------------------------------

    def test_beat_left(self):
        self.emu.beat_left()
        self.assertEqual(
            self.backend.sent(),
            [(0x94, 0x4A, 0x7F), (0x94, 0x4A, 0x00)],
        )

    def test_beat_right(self):
        self.emu.beat_right()
        self.assertEqual(
            self.backend.sent(),
            [(0x94, 0x4B, 0x7F), (0x94, 0x4B, 0x00)],
        )

    def test_beat_fx_onoff(self):
        self.emu.beat_fx_onoff()
        self.assertEqual(
            self.backend.sent(),
            [(0x94, 0x47, 0x7F), (0x94, 0x47, 0x00)],
        )

    # -- pads ---------------------------------------------------------------

    def test_pad_deck1_pad1(self):
        self.emu.pad(1, 1)
        self.assertEqual(
            self.backend.sent(),
            [(0x97, 0x00, 0x7F), (0x97, 0x00, 0x00)],
        )

    def test_pad_deck2_pad8(self):
        self.emu.pad(2, 8)
        self.assertEqual(
            self.backend.sent(),
            [(0x99, 0x07, 0x7F), (0x99, 0x07, 0x00)],
        )

    def test_pad_deck1_pad3_shift(self):
        # Accessibility pads (issue #50): Shift+Pad3 = keylock toggle.
        self.emu.pad(1, 3, shift=True)
        self.assertEqual(
            self.backend.sent(),
            [(0x98, 0x02, 0x7F), (0x98, 0x02, 0x00)],
        )

    def test_pad_deck2_pad6_shift(self):
        # Accessibility pads (issue #50): Shift+Pad6 = reset_key.
        self.emu.pad(2, 6, shift=True)
        self.assertEqual(
            self.backend.sent(),
            [(0x9A, 0x05, 0x7F), (0x9A, 0x05, 0x00)],
        )

    def test_pad_mode_hotcue_deck1(self):
        self.emu.pad_mode(1, "hotcue")
        self.assertEqual(
            self.backend.sent(),
            [(0x90, 0x1B, 0x7F), (0x90, 0x1B, 0x00)],
        )

    def test_pad_mode_beatloop_deck2(self):
        self.emu.pad_mode(2, "beatloop")
        self.assertEqual(
            self.backend.sent(),
            [(0x91, 0x6D, 0x7F), (0x91, 0x6D, 0x00)],
        )


if __name__ == "__main__":
    unittest.main()
